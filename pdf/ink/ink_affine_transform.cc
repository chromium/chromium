// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/ink_affine_transform.h"

#include <ostream>

namespace chrome_pdf {

bool operator==(const InkAffineTransform& lhs, const InkAffineTransform& rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b && lhs.c == rhs.c && lhs.d == rhs.d &&
         lhs.e == rhs.e && lhs.f == rhs.f;
}

void PrintTo(const InkAffineTransform& transform, std::ostream* os) {
  *os << "[ " << transform.a << ", " << transform.b << ", " << transform.c
      << ", " << transform.d << ", " << transform.e << ", " << transform.f
      << " ]";
}

}  // namespace chrome_pdf
