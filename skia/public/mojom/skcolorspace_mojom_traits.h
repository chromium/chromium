// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef SKIA_PUBLIC_MOJOM_SKCOLORSPACE_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_SKCOLORSPACE_MOJOM_TRAITS_H_

#include "skia/public/mojom/skcolorspace.mojom-shared.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace mojo {

template <>
struct StructTraits<skia::mojom::SkcmsMatrix3x3DataView, ::skcms_Matrix3x3> {
  static std::array<float, 9> vals(const skcms_Matrix3x3& in) {
    std::array<float, 9> out;
    for (size_t i = 0; i < 3; ++i) {
      for (size_t j = 0; j < 3; ++j) {
        out[3 * i + j] = in.vals[i][j];
      }
    }
    return out;
  }

  static bool Read(skia::mojom::SkcmsMatrix3x3DataView data,
                   skcms_Matrix3x3* out) {
    mojo::ArrayDataView<float> data_matrix;
    data.GetValsDataView(&data_matrix);
    if (data_matrix.is_null() || data_matrix.size() != 9) {
      return false;
    }
    for (size_t i = 0; i < 3; ++i) {
      for (size_t j = 0; j < 3; ++j) {
        out->vals[i][j] = data_matrix[3 * i + j];
      }
    }
    return true;
  }
};

template <>
struct StructTraits<skia::mojom::SkcmsTransferFunctionDataView,
                    ::skcms_TransferFunction> {
  static float g(const ::skcms_TransferFunction& trfn) { return trfn.g; }
  static float a(const ::skcms_TransferFunction& trfn) { return trfn.a; }
  static float b(const ::skcms_TransferFunction& trfn) { return trfn.b; }
  static float c(const ::skcms_TransferFunction& trfn) { return trfn.c; }
  static float d(const ::skcms_TransferFunction& trfn) { return trfn.d; }
  static float e(const ::skcms_TransferFunction& trfn) { return trfn.e; }
  static float f(const ::skcms_TransferFunction& trfn) { return trfn.f; }

  static bool Read(skia::mojom::SkcmsTransferFunctionDataView data,
                   ::skcms_TransferFunction* trfn) {
    trfn->g = data.g();
    trfn->a = data.a();
    trfn->b = data.b();
    trfn->c = data.c();
    trfn->d = data.d();
    trfn->e = data.e();
    trfn->f = data.f();
    return true;
  }
};

template <>
struct StructTraits<skia::mojom::SkColorSpaceDataView, ::sk_sp<SkColorSpace>> {
  static std::optional<skcms_TransferFunction> to_linear(
      const ::sk_sp<SkColorSpace>& in) {
    if (!in) {
      return std::nullopt;
    }
    skcms_TransferFunction trfn = {
        1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    };
    in->transferFn(&trfn);
    return trfn;
  }
  static std::optional<skcms_Matrix3x3> to_xyzd50(
      const ::sk_sp<SkColorSpace>& in) {
    if (!in) {
      return std::nullopt;
    }
    skcms_Matrix3x3 m = {{{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}}};
    // The function toXYZD50 returns a boolean for historical reasons. It will
    // always return true.
    bool result = in->toXYZD50(&m);
    CHECK(result);
    return m;
  }

  static bool Read(skia::mojom::SkColorSpaceDataView data,
                   ::sk_sp<SkColorSpace>* out) {
    std::optional<skcms_TransferFunction> to_linear;
    if (!data.ReadToLinear(&to_linear)) {
      return false;
    }
    std::optional<skcms_Matrix3x3> to_xyzd50;
    if (!data.ReadToXyzd50(&to_xyzd50)) {
      return false;
    }
    if (to_linear.has_value() != to_xyzd50.has_value()) {
      // Either none or both of `to_linear` and `to_xyzd50` must have a value.
      return false;
    }
    if (!to_linear.has_value() || !to_xyzd50.has_value()) {
      *out = nullptr;
    } else {
      *out = SkColorSpace::MakeRGB(to_linear.value(), to_xyzd50.value());
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_SKCOLORSPACE_MOJOM_TRAITS_H_
