// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_MATH_H_
#define REMOTING_CLIENT_DISPLAY_GL_MATH_H_

#include <array>
#include <string>

namespace remoting {

// Transposes matrix [ m0, m1, m2, m3, m4, m5, m6, m7, m8 ]:
//
// | m0, m1, m2, |   | x |
// | m3, m4, m5, | * | y |
// | m6, m7, m8  |   | 1 |
//
// Into [ m0, m3, m6, m1, m4, m7, m2, m5, m8 ].
void TransposeTransformationMatrix(std::array<float, 9>* matrix);

// Given left, top, width, height of a rectangle, fills |positions| with
// coordinates of four vertices of the rectangle.
// positions: [ x_upperleft, y_upperleft, x_lowerleft, y_lowerleft,
//              x_upperright, y_upperright, x_lowerright, y_lowerright ]
void FillRectangleVertexPositions(float left,
                                  float top,
                                  float width,
                                  float height,
                                  std::array<float, 8>* positions);

// Returns the string representation of the matrix for debugging.
//
// For example:
// [
// 1, 0, 0,
// 0, 1, 0,
// 0, 0, 1,
// ]
std::string MatrixToString(const float* mat, int num_rows, int num_cols);

}  // namespace remoting

#endif  // REMOTING_CLIENT_DISPLAY_GL_MATH_H_
