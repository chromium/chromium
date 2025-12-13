// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skcms_ext.h"

#include <cmath>
#include <numeric>

#include "base/check.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace skcms {

namespace {

// Use skcms_TransferFunction_eval and skcms_TransferFunction_invert once they
// produce correct results.
// https://crbug.com/450045076
float TransferFunction_eval(const skcms_TransferFunction& fn, float x) {
  const float sign = x < 0 ? -1 : 1;
  const float abs_x = std::abs(x);
  if (abs_x <= fn.d) {
    return sign * (fn.c * abs_x + fn.f);
  }
  float base = std::max(fn.a * abs_x + fn.b, 0.f);
  if (fn.g == 1.f) {
    return sign * (base + fn.e);
  }
  return sign * (std::pow(base, fn.g) + fn.e);
}

// This is a direct copy-paste of skcms_TransferFunction_invert, but with the
// std math functions instead of the inaccurate approximations.
bool TransferFunction_invert(const skcms_TransferFunction& src,
                             skcms_TransferFunction& inv) {
  inv = skcms_TransferFunction({0, 0, 0, 0, 0, 0, 0});
  float d_l = src.c * src.d + src.f,
        d_r = std::pow(src.a * src.d + src.b, src.g) + src.e;
  if (std::abs(d_l - d_r) > 1 / 512.0f) {
    return false;
  }
  inv.d = d_l;
  if (inv.d > 0) {
    inv.c = 1.0f / src.c;
    inv.f = -src.f / src.c;
  }
  float k = std::pow(src.a, -src.g);
  inv.g = 1.0f / src.g;
  inv.a = k;
  inv.b = -k * src.e;
  inv.e = -src.b / src.a;
  if (inv.a < 0) {
    return false;
  }
  if (inv.a * inv.d + inv.b < 0) {
    inv.b = -inv.a * inv.d;
  }
  float s = TransferFunction_eval(src, 1.0f);
  if (!std::isfinite(s)) {
    return false;
  }
  float sign = s < 0 ? -1.0f : 1.0f;
  s *= sign;
  if (s < inv.d) {
    inv.f = 1.0f - sign * inv.c * s;
  } else {
    inv.e = 1.0f - sign * std::pow(inv.a * s + inv.b, inv.g);
  }
  return true;
}

}  // namespace


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
      TransferFunction_eval(trfn, v.vals[0]),
      TransferFunction_eval(trfn, v.vals[1]),
      TransferFunction_eval(trfn, v.vals[2]),
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
  bool result = TransferFunction_invert(trfn, trfn_inv);
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
