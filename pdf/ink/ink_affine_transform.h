// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_AFFINE_TRANSFORM_H_
#define PDF_INK_INK_AFFINE_TRANSFORM_H_

#include <iosfwd>

namespace chrome_pdf {

// NOTE: This is the equivalent to the following 3x3 matrix:
//
//  a  b  c
//  d  e  f
//  0  0  1
//
// Thus the identity matrix is {1, 0, 0, 0, 1, 0}, and not {1, 0, 0, 1, 0, 0}.
struct InkAffineTransform {
  float a;
  float b;
  float c;
  float d;
  float e;
  float f;
};

bool operator==(const InkAffineTransform& lhs, const InkAffineTransform& rhs);

// Supports pretty-printing transforms for test failures.
void PrintTo(const InkAffineTransform& transform, std::ostream* os);

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_AFFINE_TRANSFORM_H_
