// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skcms_ext.h"

#include <cmath>
#include <numeric>

#include "base/check.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace {

// Evaluate the specified transfer function. This can be replaced by
// skcms_TransferFunction_eval when https://crbug.com/331320414 is fixed.
float skcmsTrFnEvalExt(const skcms_TransferFunction* fn, float x) {
  float sign = x < 0 ? -1 : 1;
  x *= sign;
  if (x >= fn->d && fn->a * x + fn->b < 0) {
    return sign * fn->e;
  }
  return sign * skcms_TransferFunction_eval(fn, x);
}

}  // namespace

namespace skcms {

Vector3 Matrix3x3_apply(const skcms_Matrix3x3& m, const Vector3& v) {
  return Vector3{{
      m.vals[0][0] * v.vals[0] + m.vals[0][1] * v.vals[1] +
          m.vals[0][2] * v.vals[2],
      m.vals[1][0] * v.vals[0] + m.vals[1][1] * v.vals[1] +
          m.vals[1][2] * v.vals[2],
      m.vals[2][0] * v.vals[0] + m.vals[2][1] * v.vals[1] +
          m.vals[2][2] * v.vals[2],
  }};
}

Vector3 Matrix3x3_apply_inverse(const skcms_Matrix3x3& m,
                                const Vector3& v,
                                bool* succeeded) {
  // Early out for the identity matrix.
  if (Equal(m, SkNamedGamut::kXYZ)) {
    return v;
  }
  return Matrix3x3_apply(Matrix3x3_invert(m, succeeded), v);
}

skcms_Matrix3x3 Matrix3x3_invert(const skcms_Matrix3x3& m, bool* succeeded) {
  skcms_Matrix3x3 m_inv = {{{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}}};
  bool result = skcms_Matrix3x3_invert(&m, &m_inv);
  if (succeeded) {
    *succeeded = result;
  } else {
    CHECK(result);
  }
  return m_inv;
}

Vector3 TransferFunction_apply(const skcms_TransferFunction& trfn,
                               const Vector3& v) {
  return Vector3{{
      skcmsTrFnEvalExt(&trfn, v.vals[0]),
      skcmsTrFnEvalExt(&trfn, v.vals[1]),
      skcmsTrFnEvalExt(&trfn, v.vals[2]),
  }};
}

Vector3 TransferFunction_apply_inverse(const skcms_TransferFunction& trfn,
                                       const Vector3& v,
                                       bool* succeeded) {
  // Early out for the identity transfer function.
  if (Equal(trfn, SkNamedTransferFn::kLinear)) {
    return v;
  }
  skcms_TransferFunction trfn_inv = SkNamedTransferFn::kLinear;
  bool result = skcms_TransferFunction_invert(&trfn, &trfn_inv);
  if (succeeded) {
    *succeeded = result;
  } else {
    CHECK(result);
  }
  return TransferFunction_apply(trfn_inv, v);
}

bool Equal(const skcms_TransferFunction& a, const skcms_TransferFunction& b) {
  return a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d && a.e == b.e &&
         a.f == b.f && a.g == b.g;
}

bool Equal(const skcms_Matrix3x3& a, const skcms_Matrix3x3& b) {
  return a.vals[0][0] == b.vals[0][0] && a.vals[0][1] == b.vals[0][1] &&
         a.vals[0][2] == b.vals[0][2] && a.vals[1][0] == b.vals[1][0] &&
         a.vals[1][1] == b.vals[1][1] && a.vals[1][2] == b.vals[1][2] &&
         a.vals[2][0] == b.vals[2][0] && a.vals[2][1] == b.vals[2][1] &&
         a.vals[2][2] == b.vals[2][2];
}

}  // namespace skcms
