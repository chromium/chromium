// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @file matrix.cc
 * Implements simple matrix manipulation functions.
 */

//-----------------------------------------------------------------------------
#include <stdlib.h>
#include <string.h>
#include "matrix.h"
#define deg_to_rad(x) (x * (M_PI / 180.0f))

void glhFrustumf2(Matrix_t mat,
                  GLfloat left,
                  GLfloat right,
                  GLfloat bottom,
                  GLfloat top,
                  GLfloat znear,
                  GLfloat zfar) {
  float temp, temp2, temp3, temp4;
  temp = 2.0f * znear;
  temp2 = right - left;
  temp3 = top - bottom;
  temp4 = zfar - znear;
  mat[0] = temp / temp2;
  mat[1] = 0.0f;
  mat[2] = 0.0f;
  mat[3] = 0.0f;
  mat[4] = 0.0f;
  mat[5] = temp / temp3;
  mat[6] = 0.0f;
  mat[7] = 0.0f;
  mat[8] = (right + left) / temp2;
  mat[9] = (top + bottom) / temp3;
  mat[10] = (-zfar - znear) / temp4;
  mat[11] = -1.0f;
  mat[12] = 0.0f;
  mat[13] = 0.0f;
  mat[14] = (-temp * zfar) / temp4;
  mat[15] = 0.0f;
}

void glhPerspectivef2(Matrix_t mat,
                      GLfloat fovyInDegrees,
                      GLfloat aspectRatio,
                      GLfloat znear,
                      GLfloat zfar) {
  float ymax, xmax;
  ymax = znear * tanf(fovyInDegrees * 3.14f / 360.0f);
  xmax = ymax * aspectRatio;
  glhFrustumf2(mat, -xmax, xmax, -ymax, ymax, znear, zfar);
}

void identity_matrix(Matrix_t mat) {
  memset(mat, 0, sizeof(Matrix_t));
  mat[0] = 1.0;
  mat[5] = 1.0;
  mat[10] = 1.0;
  mat[15] = 1.0;
}

void multiply_matrix(const Matrix_t a, const Matrix_t b, Matrix_t mat) {
  // Generate to a temporary first in case the output matrix and input
  // matrix are the same.
  Matrix_t out;

  out[0] = a[0] * b[0] + a[4] * b[1] + a[8] * b[2] + a[12] * b[3];
  out[1] = a[1] * b[0] + a[5] * b[1] + a[9] * b[2] + a[13] * b[3];
  out[2] = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
  out[3] = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];

  out[4] = a[0] * b[4] + a[4] * b[5] + a[8] * b[6] + a[12] * b[7];
  out[5] = a[1] * b[4] + a[5] * b[5] + a[9] * b[6] + a[13] * b[7];
  out[6] = a[2] * b[4] + a[6] * b[5] + a[10] * b[6] + a[14] * b[7];
  out[7] = a[3] * b[4] + a[7] * b[5] + a[11] * b[6] + a[15] * b[7];

  out[8] = a[0] * b[8] + a[4] * b[9] + a[8] * b[10] + a[12] * b[11];
  out[9] = a[1] * b[8] + a[5] * b[9] + a[9] * b[10] + a[13] * b[11];
  out[10] = a[2] * b[8] + a[6] * b[9] + a[10] * b[10] + a[14] * b[11];
  out[11] = a[3] * b[8] + a[7] * b[9] + a[11] * b[10] + a[15] * b[11];

  out[12] = a[0] * b[12] + a[4] * b[13] + a[8] * b[14] + a[12] * b[15];
  out[13] = a[1] * b[12] + a[5] * b[13] + a[9] * b[14] + a[13] * b[15];
  out[14] = a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15];
  out[15] = a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15];

  memcpy(mat, out, sizeof(Matrix_t));
}

void rotate_x_matrix(GLfloat x_rad, Matrix_t mat) {
  identity_matrix(mat);
  mat[5] = cosf(x_rad);
  mat[6] = -sinf(x_rad);
  mat[9] = -mat[6];
  mat[10] = mat[5];
}

void rotate_y_matrix(GLfloat y_rad, Matrix_t mat) {
  identity_matrix(mat);
  mat[0] = cosf(y_rad);
  mat[2] = sinf(y_rad);
  mat[8] = -mat[2];
  mat[10] = mat[0];
}

void rotate_z_matrix(GLfloat z_rad, Matrix_t mat) {
  identity_matrix(mat);
  mat[0] = cosf(z_rad);
  mat[1] = sinf(z_rad);
  mat[4] = -mat[1];
  mat[5] = mat[0];
}

void rotate_matrix(GLfloat x_deg, GLfloat y_deg, GLfloat z_deg, Matrix_t mat) {
  GLfloat x_rad = (GLfloat) deg_to_rad(x_deg);
  GLfloat y_rad = (GLfloat) deg_to_rad(y_deg);
  GLfloat z_rad = (GLfloat) deg_to_rad(z_deg);

  Matrix_t x_matrix;
  Matrix_t y_matrix;
  Matrix_t z_matrix;

  rotate_x_matrix(x_rad, x_matrix);
  rotate_y_matrix(y_rad, y_matrix);
  rotate_z_matrix(z_rad, z_matrix);

  Matrix_t xy_matrix;
  multiply_matrix(y_matrix, x_matrix, xy_matrix);
  multiply_matrix(z_matrix, xy_matrix, mat);
}

void translate_matrix(GLfloat x, GLfloat y, GLfloat z, Matrix_t mat) {
  identity_matrix(mat);
  mat[12] += x;
  mat[13] += y;
  mat[14] += z;
}
