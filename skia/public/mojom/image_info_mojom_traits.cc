// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/public/mojom/image_info_mojom_traits.h"

#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/array_data_view.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"

namespace mojo {

namespace {

SkColorType MojoColorTypeToSk(skia::mojom::ColorType type) {
  switch (type) {
    case skia::mojom::ColorType::UNKNOWN:
      return kUnknown_SkColorType;
    case skia::mojom::ColorType::ALPHA_8:
      return kAlpha_8_SkColorType;
    case skia::mojom::ColorType::RGB_565:
      return kRGB_565_SkColorType;
    case skia::mojom::ColorType::ARGB_4444:
      return kARGB_4444_SkColorType;
    case skia::mojom::ColorType::RGBA_8888:
      return kRGBA_8888_SkColorType;
    case skia::mojom::ColorType::BGRA_8888:
      return kBGRA_8888_SkColorType;
    case skia::mojom::ColorType::GRAY_8:
      return kGray_8_SkColorType;
    case skia::mojom::ColorType::DEPRECATED_INDEX_8:
      // no longer supported
      break;
  }
  NOTREACHED();
  return kUnknown_SkColorType;
}

SkAlphaType MojoAlphaTypeToSk(skia::mojom::AlphaType type) {
  switch (type) {
    case skia::mojom::AlphaType::UNKNOWN:
      return kUnknown_SkAlphaType;
    case skia::mojom::AlphaType::ALPHA_TYPE_OPAQUE:
      return kOpaque_SkAlphaType;
    case skia::mojom::AlphaType::PREMUL:
      return kPremul_SkAlphaType;
    case skia::mojom::AlphaType::UNPREMUL:
      return kUnpremul_SkAlphaType;
  }
  NOTREACHED();
  return kUnknown_SkAlphaType;
}

skia::mojom::ColorType SkColorTypeToMojo(SkColorType type) {
  switch (type) {
    case kUnknown_SkColorType:
      return skia::mojom::ColorType::UNKNOWN;
    case kAlpha_8_SkColorType:
      return skia::mojom::ColorType::ALPHA_8;
    case kRGB_565_SkColorType:
      return skia::mojom::ColorType::RGB_565;
    case kARGB_4444_SkColorType:
      return skia::mojom::ColorType::ARGB_4444;
    case kRGBA_8888_SkColorType:
      return skia::mojom::ColorType::RGBA_8888;
    case kBGRA_8888_SkColorType:
      return skia::mojom::ColorType::BGRA_8888;
    case kGray_8_SkColorType:
      return skia::mojom::ColorType::GRAY_8;
    default:
      // Skia has color types not used by Chrome.
      break;
  }
  NOTREACHED();
  return skia::mojom::ColorType::UNKNOWN;
}

skia::mojom::AlphaType SkAlphaTypeToMojo(SkAlphaType type) {
  switch (type) {
    case kUnknown_SkAlphaType:
      return skia::mojom::AlphaType::UNKNOWN;
    case kOpaque_SkAlphaType:
      return skia::mojom::AlphaType::ALPHA_TYPE_OPAQUE;
    case kPremul_SkAlphaType:
      return skia::mojom::AlphaType::PREMUL;
    case kUnpremul_SkAlphaType:
      return skia::mojom::AlphaType::UNPREMUL;
  }
  NOTREACHED();
  return skia::mojom::AlphaType::UNKNOWN;
}

}  // namespace

// static
skia::mojom::ColorType
StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::color_type(
    const SkImageInfo& info) {
  return SkColorTypeToMojo(info.colorType());
}

// static
skia::mojom::AlphaType
StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::alpha_type(
    const SkImageInfo& info) {
  return SkAlphaTypeToMojo(info.alphaType());
}

// static
uint32_t StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::width(
    const SkImageInfo& info) {
  // Negative width images are invalid.
  return base::checked_cast<uint32_t>(info.width());
}

// static
uint32_t StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::height(
    const SkImageInfo& info) {
  // Negative height images are invalid.
  return base::checked_cast<uint32_t>(info.height());
}

// static
base::Optional<std::vector<float>>
StructTraits<skia::mojom::ImageInfoDataView,
             SkImageInfo>::color_transfer_function(const SkImageInfo& info) {
  SkColorSpace* color_space = info.colorSpace();
  if (!color_space)
    return base::nullopt;
  skcms_TransferFunction fn;
  color_space->transferFn(&fn);
  return std::vector<float>({fn.g, fn.a, fn.b, fn.c, fn.d, fn.e, fn.f});
}

// static
base::Optional<std::vector<float>>
StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::color_to_xyz_matrix(
    const SkImageInfo& info) {
  SkColorSpace* color_space = info.colorSpace();
  if (!color_space)
    return base::nullopt;
  skcms_Matrix3x3 to_xyz_matrix;
  CHECK(color_space->toXYZD50(&to_xyz_matrix));

  // C-style arrays-of-arrays are tightly packed, so directly copy into vector.
  static_assert(sizeof(to_xyz_matrix.vals) == sizeof(float) * 9,
                "matrix must be 3x3 floats");
  float* values = &to_xyz_matrix.vals[0][0];
  return std::vector<float>(values, values + 9);
}

// static
bool StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::Read(
    skia::mojom::ImageInfoDataView data,
    SkImageInfo* info) {
  mojo::ArrayDataView<float> color_transfer_function;
  data.GetColorTransferFunctionDataView(&color_transfer_function);
  mojo::ArrayDataView<float> color_to_xyz_matrix;
  data.GetColorToXyzMatrixDataView(&color_to_xyz_matrix);

  // Sender must supply both color space fields or neither. This approach is
  // simpler than having an optional ColorSpace mojo struct, due to BUILD.gn
  // complexity with blink variants.
  CHECK_EQ(color_transfer_function.is_null(), color_to_xyz_matrix.is_null());

  sk_sp<SkColorSpace> sk_color_space;
  if (!color_transfer_function.is_null() && !color_to_xyz_matrix.is_null()) {
    const float* data = color_transfer_function.data();
    skcms_TransferFunction transfer_function;
    CHECK_EQ(7u, color_transfer_function.size());
    transfer_function.g = data[0];
    transfer_function.a = data[1];
    transfer_function.b = data[2];
    transfer_function.c = data[3];
    transfer_function.d = data[4];
    transfer_function.e = data[5];
    transfer_function.f = data[6];

    skcms_Matrix3x3 to_xyz_matrix;
    CHECK_EQ(9u, color_to_xyz_matrix.size());
    memcpy(to_xyz_matrix.vals, color_to_xyz_matrix.data(), 9 * sizeof(float));
    sk_color_space = SkColorSpace::MakeRGB(transfer_function, to_xyz_matrix);
  }

  *info = SkImageInfo::Make(
      data.width(), data.height(), MojoColorTypeToSk(data.color_type()),
      MojoAlphaTypeToSk(data.alpha_type()), std::move(sk_color_space));
  return true;
}

}  // namespace mojo
