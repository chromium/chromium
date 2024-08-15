// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/public/mojom/image_info_mojom_traits.h"

#include <optional>

#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/bindings/array_data_view.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace mojo {

namespace {

std::optional<SkImageInfo> MakeSkImageInfo(
    SkColorType color_type,
    SkAlphaType alpha_type,
    int width,
    int height,
    mojo::ArrayDataView<float> color_transfer_function,
    mojo::ArrayDataView<float> color_to_xyz_matrix) {
  if (width < 0 || height < 0) {
    return std::nullopt;
  }
  sk_sp<SkColorSpace> color_space;
  if (!color_transfer_function.is_null() && !color_to_xyz_matrix.is_null()) {
    const float* data = color_transfer_function.data();
    skcms_TransferFunction transfer_function;
    // TODO(crbug.com/40061960): Mojo should validate this array size. We can
    // CHECK it instead when it does.
    if (color_transfer_function.size() != 7u) {
      return std::nullopt;
    }
    transfer_function.g = data[0];
    transfer_function.a = data[1];
    transfer_function.b = data[2];
    transfer_function.c = data[3];
    transfer_function.d = data[4];
    transfer_function.e = data[5];
    transfer_function.f = data[6];

    skcms_Matrix3x3 to_xyz_matrix;
    // TODO(crbug.com/40061960): Mojo should validate this array size. We can
    // CHECK it instead when it does.
    if (color_to_xyz_matrix.size() != 9u) {
      return std::nullopt;
    }
    memcpy(to_xyz_matrix.vals, color_to_xyz_matrix.data(), 9 * sizeof(float));
    color_space = SkColorSpace::MakeRGB(transfer_function, to_xyz_matrix);
  }

  return SkImageInfo::Make(width, height, color_type, alpha_type,
                           std::move(color_space));
}

}  // namespace

// static
skia::mojom::AlphaType EnumTraits<skia::mojom::AlphaType, SkAlphaType>::ToMojom(
    SkAlphaType type) {
  switch (type) {
    case kOpaque_SkAlphaType:
      return skia::mojom::AlphaType::ALPHA_TYPE_OPAQUE;
    case kPremul_SkAlphaType:
      return skia::mojom::AlphaType::PREMUL;
    case kUnpremul_SkAlphaType:
      return skia::mojom::AlphaType::UNPREMUL;
    case kUnknown_SkAlphaType:
      // Unknown types should not be sent over mojo.
      break;
  }
  NOTREACHED();
}

// static
bool EnumTraits<skia::mojom::AlphaType, SkAlphaType>::FromMojom(
    skia::mojom::AlphaType in,
    SkAlphaType* out) {
  switch (in) {
    case skia::mojom::AlphaType::ALPHA_TYPE_OPAQUE:
      *out = kOpaque_SkAlphaType;
      return true;
    case skia::mojom::AlphaType::PREMUL:
      *out = kPremul_SkAlphaType;
      return true;
    case skia::mojom::AlphaType::UNPREMUL:
      *out = kUnpremul_SkAlphaType;
      return true;
    case skia::mojom::AlphaType::UNKNOWN:
      // Unknown types should not be sent over mojo.
      return false;
  }
  return false;
}

// static
skia::mojom::ColorType EnumTraits<skia::mojom::ColorType, SkColorType>::ToMojom(
    SkColorType type) {
  switch (type) {
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
    case kUnknown_SkColorType:
      // Fall through as unknown values should not be sent over the wire.
    default:
      // Skia has color types not used by Chrome.
      break;
  }
  NOTREACHED();
}

// static
bool EnumTraits<skia::mojom::ColorType, SkColorType>::FromMojom(
    skia::mojom::ColorType in,
    SkColorType* out) {
  switch (in) {
    case skia::mojom::ColorType::ALPHA_8:
      *out = kAlpha_8_SkColorType;
      return true;
    case skia::mojom::ColorType::RGB_565:
      *out = kRGB_565_SkColorType;
      return true;
    case skia::mojom::ColorType::ARGB_4444:
      *out = kARGB_4444_SkColorType;
      return true;
    case skia::mojom::ColorType::RGBA_8888:
      *out = kRGBA_8888_SkColorType;
      return true;
    case skia::mojom::ColorType::BGRA_8888:
      *out = kBGRA_8888_SkColorType;
      return true;
    case skia::mojom::ColorType::GRAY_8:
      *out = kGray_8_SkColorType;
      return true;
    case skia::mojom::ColorType::DEPRECATED_INDEX_8:
    case skia::mojom::ColorType::UNKNOWN:
      // UNKNOWN or unsupported values should not be sent over mojo.
      break;
  }
  return false;
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
std::optional<std::vector<float>>
StructTraits<skia::mojom::ImageInfoDataView,
             SkImageInfo>::color_transfer_function(const SkImageInfo& info) {
  SkColorSpace* color_space = info.colorSpace();
  if (!color_space) {
    return std::nullopt;
  }
  skcms_TransferFunction fn;
  color_space->transferFn(&fn);
  return std::vector<float>({fn.g, fn.a, fn.b, fn.c, fn.d, fn.e, fn.f});
}

// static
std::optional<std::vector<float>>
StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::color_to_xyz_matrix(
    const SkImageInfo& info) {
  SkColorSpace* color_space = info.colorSpace();
  if (!color_space) {
    return std::nullopt;
  }
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
  SkColorType color_type;
  SkAlphaType alpha_type;

  if (!data.ReadColorType(&color_type) || !data.ReadAlphaType(&alpha_type)) {
    return false;
  }

  mojo::ArrayDataView<float> color_transfer_function;
  data.GetColorTransferFunctionDataView(&color_transfer_function);
  mojo::ArrayDataView<float> color_to_xyz_matrix;
  data.GetColorToXyzMatrixDataView(&color_to_xyz_matrix);

  // The ImageInfo wire types are uint32_t, but the Skia type uses int, and the
  // values can't be negative.
  auto width = base::MakeCheckedNum(data.width()).Cast<int>();
  auto height = base::MakeCheckedNum(data.height()).Cast<int>();
  if (!width.IsValid() || !height.IsValid()) {
    return false;
  }

  std::optional<SkImageInfo> maybe_info = MakeSkImageInfo(
      color_type, alpha_type, width.ValueOrDie(), height.ValueOrDie(),
      std::move(color_transfer_function), std::move(color_to_xyz_matrix));
  if (!maybe_info.has_value()) {
    return false;
  }
  *info = *maybe_info;
  return true;
}

// static
bool StructTraits<skia::mojom::BitmapN32ImageInfoDataView, SkImageInfo>::Read(
    skia::mojom::BitmapN32ImageInfoDataView data,
    SkImageInfo* info) {
  SkAlphaType alpha_type;
  if (!data.ReadAlphaType(&alpha_type)) {
    return false;
  }

  mojo::ArrayDataView<float> color_transfer_function;
  data.GetColorTransferFunctionDataView(&color_transfer_function);
  mojo::ArrayDataView<float> color_to_xyz_matrix;
  data.GetColorToXyzMatrixDataView(&color_to_xyz_matrix);

  // The ImageInfo wire types are uint32_t, but the Skia type uses int, and the
  // values can't be negative.
  auto width = base::MakeCheckedNum(data.width()).Cast<int>();
  auto height = base::MakeCheckedNum(data.height()).Cast<int>();
  if (!width.IsValid() || !height.IsValid()) {
    return false;
  }

  std::optional<SkImageInfo> maybe_info = MakeSkImageInfo(
      kN32_SkColorType, alpha_type, width.ValueOrDie(), height.ValueOrDie(),
      std::move(color_transfer_function), std::move(color_to_xyz_matrix));
  if (!maybe_info.has_value()) {
    return false;
  }
  *info = *maybe_info;
  return true;
}

}  // namespace mojo
