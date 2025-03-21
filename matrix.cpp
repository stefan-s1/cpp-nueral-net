#ifndef __QS_MATRIX_CPP
#define __QS_MATRIX_CPP

#include "matrix.h"
#include <functional>
#include <random>
#include <cassert>
#include <algorithm>  // for std::copy and std::transform

// --- QSMatrix Implementation ---

// Parameter Constructor: initialize with given number of rows, columns, and an initial value.
template<typename T>
Matrix<T>::Matrix(unsigned _rows, unsigned _cols, const T& _initial)
    : rows(_rows), cols(_cols), mat(_rows * _cols, _initial)
{
}

// Constructor from a vector-of-vector (assumes rectangular input)
// Stores the data in row-major order.
template<typename T>
Matrix<T>::Matrix(const std::vector<std::vector<T>>& _mat)
    : rows(_mat.size()), cols((_mat.empty() ? 0 : _mat[0].size())), mat(rows * cols)
{
    for (size_t i = 0; i < rows; ++i) {
        assert(_mat[i].size() == cols);  // ensure all rows have the same number of columns
        // Use std::copy to fill the corresponding row in 'mat'
        std::copy(_mat[i].begin(), _mat[i].end(), mat.begin() + i * cols);
    }
}

// Constructor from a vector (assumes row major input)
// Stores the data in row-major order.
template<typename T>
Matrix<T>::Matrix(const std::vector<T>& _mat, size_t _rows, size_t _cols)
    : rows(_rows), cols(_cols), mat(_mat)
{
}

// Create a random matrix with values in [-maxWeight, maxWeight].
template<typename T>
Matrix<T> Matrix<T>::initRandomQSMatrix(size_t _rows, size_t _cols, const T& maxWeight) {
    Matrix<T> my_matrix(_rows, _cols, T());
    static std::mt19937 gen{42}; // Fixed seed for reproducibility. Has to be static so we don't reseed the same fixed seed everytime
    std::uniform_real_distribution<T> d(-maxWeight, maxWeight);
    //std::normal_distribution<T> d(0, maxWeight / 4); // set s.d. to max / 4 so that 99.9% of values are less than max weight
    for (size_t i = 0; i < _rows * _cols; ++i) {
        my_matrix.mat[i] = d(gen);
    }
    return my_matrix;
}

// Move Constructor.
template<typename T>
Matrix<T>::Matrix(Matrix<T>&& rhs) noexcept
    : rows(rhs.rows), cols(rhs.cols), mat(std::move(rhs.mat))
{
    rhs.rows = 0;
    rhs.cols = 0;
}

// Copy Constructor.
template<typename T>
Matrix<T>::Matrix(const Matrix<T>& rhs)
    : rows(rhs.rows), cols(rhs.cols), mat(rhs.mat)
{
}

// Destructor.
template<typename T>
Matrix<T>::~Matrix() {}

// Assignment Operator.
template<typename T>
Matrix<T>& Matrix<T>::operator=(const Matrix<T>& rhs) {
    if (this == &rhs)
        return *this;
    rows = rhs.rows;
    cols = rhs.cols;
    mat = rhs.mat;
    return *this;
}

// Move Assignment Operator
template<typename T>
Matrix<T>& Matrix<T>::operator=(Matrix<T>&& rhs) noexcept {
    if (this != &rhs) {
        rows = std::exchange(rhs.rows, 0);
        cols = std::exchange(rhs.cols, 0);
        mat = std::move(rhs.mat);
    }
    return *this;
}


// Matrix addition with broadcasting support.
template<typename T>
Matrix<T> Matrix<T>::operator+(const Matrix<T>& rhs) const {
    // Case 1: Standard elementwise addition.
    if (rows == rhs.rows && cols == rhs.cols) {
        Matrix<T> result(rows, cols, T());
        // Use std::transform for element-wise addition. Can experiment with std::execution::par if on multi-core machine
        std::transform(mat.begin(), mat.end(), rhs.mat.begin(), result.mat.begin(),
                       std::plus<T>());
        return result;
    }
    // Case 2: rhs is a row vector that should be broadcast to all rows.
    else if (rhs.rows == 1 && rhs.cols == cols) {
        Matrix<T> result(rows, cols, T());
        for (size_t i = 0; i < rows; ++i) {
            for (size_t j = 0; j < cols; ++j) {
                result(i, j) = (*this)(i, j) + rhs(0, j);
            }
        }
        return result;
    }
    // Case 3: *this is a row vector that should be broadcast to all rows of rhs.
    else if (rows == 1 && cols == rhs.cols) {
        Matrix<T> result(rhs.rows, cols, T());
        for (size_t i = 0; i < rhs.rows; ++i) {
            for (size_t j = 0; j < cols; ++j) {
                result(i, j) = (*this)(0, j) + rhs(i, j);
            }
        }
        return result;
    }
    else {
        assert(false && "Matrix dimensions incompatible for addition (no broadcasting available)");
        return Matrix<T>(0, 0, T());
    }
}

// Cumulative addition.
template<typename T>
Matrix<T>& Matrix<T>::operator+=(const Matrix<T>& rhs) {
    assert(rows == rhs.rows && cols == rhs.cols);
    for (size_t i = 0; i < rows * cols; ++i) {
        mat[i] += rhs.mat[i];
    }
    return *this;
}

// Matrix subtraction.
template<typename T>
Matrix<T> Matrix<T>::operator-(const Matrix<T>& rhs) const {
    assert(rows == rhs.rows && cols == rhs.cols);
    Matrix<T> result(rows, cols, T());
    std::transform(mat.begin(), mat.end(), rhs.mat.begin(), result.mat.begin(), std::minus<T>());
    return result;
}

// Cumulative subtraction.
template<typename T>
Matrix<T>& Matrix<T>::operator-=(const Matrix<T>& rhs) {
    assert(rows == rhs.rows && cols == rhs.cols);
    for (size_t i = 0; i < rows * cols; ++i) {
        mat[i] -= rhs.mat[i];
    }
    return *this;
}

// Optimized Matrix multiplication.
// Reorder loops for better cache locality by fixing a value from *this.
template<typename T>
Matrix<T> Matrix<T>::operator*(const Matrix<T>& rhs) const {
    assert(cols == rhs.rows);
    Matrix<T> result(rows, rhs.cols, T());
    for (size_t i = 0; i < rows; ++i) {
        for (size_t k = 0; k < cols; ++k) {
            T temp = (*this)(i, k);
            // using offset directly here instead of () operator as this is performance critical code
            size_t rhs_offset = k * rhs.cols;
            size_t result_offset = i * result.cols;
            for (size_t j = 0; j < rhs.cols; ++j) {
                result.mat[result_offset + j] += temp * rhs.mat[rhs_offset + j]; 
            }
        }
    }
    return result;
}

// Cumulative multiplication.
template<typename T>
Matrix<T>& Matrix<T>::operator*=(const Matrix<T>& rhs) {
    *this = std::move((*this) * rhs);
    return *this;
}

// Transpose (non in-place).
template<typename T>
Matrix<T> Matrix<T>::transpose() const {

    // Matrix<T> result(this->mat, cols, rows); would this work?
    Matrix<T> result(cols, rows, T());
    
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            result(j, i) = (*this)(i, j);
        }
    }
    return result;
}

// In-place transpose.
template<typename T>
Matrix<T>& Matrix<T>::transpose_in_place() {
    *this = std::move(this->transpose());
    return *this;
}

// Matrix/scalar addition.
template<typename T>
Matrix<T> Matrix<T>::operator+(const T& rhs) const {
    Matrix<T> result(rows, cols, T());
    std::transform(mat.begin(), mat.end(), result.mat.begin(),
                   [rhs](T val) { return val + rhs; });
    return result;
}

// Matrix/scalar subtraction.
template<typename T>
Matrix<T> Matrix<T>::operator-(const T& rhs) const {
    Matrix<T> result(rows, cols, T());
    std::transform(mat.begin(), mat.end(), result.mat.begin(),
                   [rhs](T val) { return val - rhs; });
    return result;
}

// Matrix/scalar multiplication.
template<typename T>
Matrix<T> Matrix<T>::operator*(const T& rhs) const {
    Matrix<T> result(rows, cols, T());
    std::transform(mat.begin(), mat.end(), result.mat.begin(),
                   [rhs](T val) { return val * rhs; });
    return result;
}

// Cumulative scalar multiplication.
template<typename T>
Matrix<T>& Matrix<T>::operator*=(const T& rhs) {
    for (size_t i = 0; i < rows * cols; ++i) {
        mat[i] *= rhs;
    }
    return *this;
}

// Matrix/scalar division.
template<typename T>
Matrix<T> Matrix<T>::operator/(const T& rhs) const {
    Matrix<T> result(rows, cols, T());
    std::transform(mat.begin(), mat.end(), result.mat.begin(),
                   [rhs](T val) { return val / rhs; });
    return result;
}

// Matrix-vector multiplication (vector size must equal number of columns).
template<typename T>
std::vector<T> Matrix<T>::operator*(const std::vector<T>& rhs) {
    assert(rhs.size() == cols);
    std::vector<T> result(rows, T());
    for (size_t i = 0; i < rows; ++i) {
        T sum = T();
        for (size_t j = 0; j < cols; ++j) {
            sum += (*this)(i, j) * rhs[j];
        }
        result[i] = sum;
    }
    return result;
}

// Return a vector containing the diagonal elements.
template<typename T>
std::vector<T> Matrix<T>::diag_vec() {
    size_t n = (rows < cols) ? rows : cols;
    std::vector<T> result(n, T());
    for (size_t i = 0; i < n; ++i) {
        result[i] = (*this)(i, i);
    }
    return result;
}

// Component-wise transformation (returns a new matrix).
template<typename T>
Matrix<T> Matrix<T>::component_wise_transformation(const std::function<T(T)>& transformation) const {
    Matrix<T> result(rows, cols, T());
    std::transform(mat.begin(), mat.end(), result.mat.begin(), transformation);
    return result;
}

// In-place component-wise transformation.
template<typename T>
Matrix<T>& Matrix<T>::component_wise_transformation_in_place(const std::function<T(T)>& transformation) {
    std::transform(mat.begin(), mat.end(), mat.begin(), transformation);
    return *this;
}

// Overloaded operator() for non-const element access.
template<typename T>
T& Matrix<T>::operator()(const unsigned& row, const unsigned& col) {
    assert(row < rows && col < cols);
    return mat[row * cols + col];
}

// Overloaded operator() for const element access.
template<typename T>
const T& Matrix<T>::operator()(const unsigned& row, const unsigned& col) const {
    assert(row < rows && col < cols);
    return mat[row * cols + col];
}

// Return the number of rows.
template<typename T>
unsigned Matrix<T>::get_rows() const {
    return rows;
}

// Return the number of columns.
template<typename T>
unsigned Matrix<T>::get_cols() const {
    return cols;
}

// Element-wise (Hadamard) multiplication.
template<typename T>
Matrix<T> Matrix<T>::hadamardMultiplication(const Matrix<T>& rhs) const {
    assert(rows == rhs.rows && cols == rhs.cols);
    Matrix<T> result(rows, cols, T());
    std::transform(mat.begin(), mat.end(), rhs.mat.begin(), result.mat.begin(),
                   std::multiplies<T>());
    return result;
}

// In-place Hadamard multiplication.
template<typename T>
Matrix<T>& Matrix<T>::hadamardMultiplicationInPlace(const Matrix<T>& rhs) {
    assert(rows == rhs.rows && cols == rhs.cols);
    for (size_t i = 0; i < rows * cols; ++i) {
        mat[i] *= rhs.mat[i];
    }
    return *this;
}

#endif
