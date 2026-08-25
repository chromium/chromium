// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skcolorspace_ext.h"

#include <cmath>

#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace skia {

bool ApproximatelyEqual(const SkColorSpace* a, const SkColorSpace* b) {
  if (a == b) {
    return true;
  }
  if (!a || !b) {
    return false;
  }
  if (SkColorSpace::Equals(a, b)) {
    return true;
  }
  skcms_TransferFunction a_fn;
  skcms_TransferFunction b_fn;

  // Require an exact match for everything except the sRGB-like transfer
  // functions.
  if (!a->isNumericalTransferFn(&a_fn) || !b->isNumericalTransferFn(&b_fn)) {
    return false;
  }
  // Matches Skia's internal `transfer_fn_almost_equal` threshold of 0.001f
  // (see third_party/skia/src/core/SkColorSpacePriv.h).
  constexpr float kTransferFnMaxError = 0.001f;
  if (std::abs(a_fn.g - b_fn.g) > kTransferFnMaxError ||
      std::abs(a_fn.a - b_fn.a) > kTransferFnMaxError ||
      std::abs(a_fn.b - b_fn.b) > kTransferFnMaxError ||
      std::abs(a_fn.c - b_fn.c) > kTransferFnMaxError ||
      std::abs(a_fn.d - b_fn.d) > kTransferFnMaxError ||
      std::abs(a_fn.e - b_fn.e) > kTransferFnMaxError ||
      std::abs(a_fn.f - b_fn.f) > kTransferFnMaxError) {
    return false;
  }
  skcms_Matrix3x3 a_mat;
  skcms_Matrix3x3 b_mat;
  if (!a->toXYZD50(&a_mat) || !b->toXYZD50(&b_mat)) {
    return false;
  }
  // Matches Skia's internal `xyz_almost_equal` / `color_space_almost_equal`
  // threshold of 0.01f (see third_party/skia/src/core/SkColorSpacePriv.h and
  // third_party/skia/src/core/SkColorSpace.cpp).
  constexpr float kMatrixMaxError = 0.01f;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      // SAFETY: `skcms_Matrix3x3::vals` is a fixed 3x3 C array, and `r` and `c`
      // are strictly in the range [0, 2].
      if (std::abs(UNSAFE_BUFFERS(a_mat.vals[r][c]) -
                   UNSAFE_BUFFERS(b_mat.vals[r][c])) > kMatrixMaxError) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace skia
