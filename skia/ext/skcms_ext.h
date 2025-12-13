// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKCMS_EXT_H_
#define SKIA_EXT_SKCMS_EXT_H_

#include <array>

#include "third_party/skia/include/private/base/SkAPI.h"
#include "third_party/skia/modules/skcms/skcms.h"

// Functionality that is currently not provided by skcms, but may eventually
// be rolled into skcms.
namespace skcms {

// A vector that represents an RGB triple.
struct SK_API Vector3 {
  std::array<float, 3> vals;
};

// Perform the specified matrix-vector multiplication and return the result.
SK_API Vector3 Matrix3x3_apply(const skcms_Matrix3x3& m, const Vector3& v);

// Return the result of `m` inverted and multiplied by `v`. If `succeeded`
// is non-nullptr then set it to false if `m` is uninvertible. If `succeeded`
// is nullptr, then CHECK that `m` is invertible.
SK_API Vector3 Matrix3x3_apply_inverse(const skcms_Matrix3x3& m,
                                       const Vector3& v,
                                       bool* succeeded = nullptr);

// Invert and apply the specified matrix. If `succeeded` is non-nullptr, then
// set it to false if `m` is uninvertible. If `succeeded` is nullptr, then
// CHECK that `m` is invertible.
SK_API skcms_Matrix3x3 Matrix3x3_invert(const skcms_Matrix3x3& m,
                                        bool* succeeded = nullptr);

// Apply the specified transfer function to the specified vector. This only
// handles sRGB-ish functions, and has slightly different precision compared
// with skcms_TransferFunction_eval. See https://crbug.com/331320414.
SK_API Vector3 TransferFunction_apply(const skcms_TransferFunction& trfn,
                                      const Vector3& v);

// Invert and apply the specified transfer function. If `succeeded` is
// non-nullptr, then set it to false if `trfn` is uninvertible. If `succeeded`
// is nullptr, then CHECK that `trfn` is invertible.
SK_API Vector3
TransferFunction_apply_inverse(const skcms_TransferFunction& trfn,
                               const Vector3& v,
                               bool* succeeded = nullptr);

SK_API bool Equal(const skcms_TransferFunction& a,
                  const skcms_TransferFunction& b);
SK_API bool Equal(const skcms_Matrix3x3& a, const skcms_Matrix3x3& b);

}  // namespace skcms

#endif  // SKIA_EXT_SKCMS_EXT_H_
