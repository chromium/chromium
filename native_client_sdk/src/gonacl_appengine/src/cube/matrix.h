#ifndef GONACL_APPENGINE_SRC_CUBE_MATRIX_H_
#define GONACL_APPENGINE_SRC_CUBE_MATRIX_H_

/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** @file matrix.cc
 * Implements simple matrix manipulation functions.
 */

//-----------------------------------------------------------------------------
#define _USE_MATH_DEFINES 1
#include <limits.h>
#include <math.h>
#include <GLES2/gl2.h>

typedef GLfloat Matrix_t[16];

/// Since GLES2 doesn't have all the nifty matrix transform functions that GL
/// has, we emulate some of them here for the sake of sanity from:
/// http://www.opengl.org/wiki/GluPerspective_code
void glhFrustumf2(Matrix_t mat,
                  GLfloat left,
                  GLfloat right,
                  GLfloat bottom,
                  GLfloat top,
                  GLfloat znear,
                  GLfloat zfar);

void glhPerspectivef2(Matrix_t mat,
                      GLfloat fovyInDegrees,
                      GLfloat aspectRatio,
                      GLfloat znear,
                      GLfloat zfar);

void identity_matrix(Matrix_t mat);
void multiply_matrix(const Matrix_t a, const Matrix_t b, Matrix_t mat);
void rotate_matrix(GLfloat x_deg, GLfloat y_deg, GLfloat z_deg, Matrix_t mat);
void translate_matrix(GLfloat x, GLfloat y, GLfloat z, Matrix_t mat);

#endif  // GONACL_APPENGINE_SRC_CUBE_MATRIX_H_
