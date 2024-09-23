// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_color_params.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"

namespace blink {

namespace {

SerializedPredefinedColorSpace SerializeColorSpace(
    PredefinedColorSpace color_space) {
  switch (color_space) {
    case PredefinedColorSpace::kSRGB:
      return SerializedPredefinedColorSpace::kSRGB;
    case PredefinedColorSpace::kRec2020:
      return SerializedPredefinedColorSpace::kRec2020;
    case PredefinedColorSpace::kP3:
      return SerializedPredefinedColorSpace::kP3;
    case PredefinedColorSpace::kRec2100HLG:
      return SerializedPredefinedColorSpace::kRec2100HLG;
    case PredefinedColorSpace::kRec2100PQ:
      return SerializedPredefinedColorSpace::kRec2100PQ;
    case PredefinedColorSpace::kSRGBLinear:
      return SerializedPredefinedColorSpace::kSRGBLinear;
  }
  NOTREACHED_IN_MIGRATION();
  return SerializedPredefinedColorSpace::kSRGB;
}

PredefinedColorSpace DeserializeColorSpace(
    SerializedPredefinedColorSpace serialized_color_space) {
  switch (serialized_color_space) {
    case SerializedPredefinedColorSpace::kLegacyObsolete:
    case SerializedPredefinedColorSpace::kSRGB:
      return PredefinedColorSpace::kSRGB;
    case SerializedPredefinedColorSpace::kRec2020:
      return PredefinedColorSpace::kRec2020;
    case SerializedPredefinedColorSpace::kP3:
      return PredefinedColorSpace::kP3;
    case SerializedPredefinedColorSpace::kRec2100HLG:
      return PredefinedColorSpace::kRec2100HLG;
    case SerializedPredefinedColorSpace::kRec2100PQ:
      return PredefinedColorSpace::kRec2100PQ;
    case SerializedPredefinedColorSpace::kSRGBLinear:
      return PredefinedColorSpace::kSRGBLinear;
  }
  NOTREACHED_IN_MIGRATION();
  return PredefinedColorSpace::kSRGB;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SerializedImageDataSettings

SerializedImageDataSettings::SerializedImageDataSettings(
    PredefinedColorSpace color_space,
    ImageDataStorageFormat storage_format)
    : color_space_(SerializeColorSpace(color_space)) {
  switch (storage_format) {
    case ImageDataStorageFormat::kUint8:
      storage_format_ = SerializedImageDataStorageFormat::kUint8Clamped;
      break;
    case ImageDataStorageFormat::kUint16:
      storage_format_ = SerializedImageDataStorageFormat::kUint16;
      break;
    case ImageDataStorageFormat::kFloat32:
      storage_format_ = SerializedImageDataStorageFormat::kFloat32;
      break;
  }
}

SerializedImageDataSettings::SerializedImageDataSettings(
    SerializedPredefinedColorSpace color_space,
    SerializedImageDataStorageFormat storage_format)
    : color_space_(color_space), storage_format_(storage_format) {}

PredefinedColorSpace SerializedImageDataSettings::GetColorSpace() const {
  return DeserializeColorSpace(color_space_);
}

ImageDataStorageFormat SerializedImageDataSettings::GetStorageFormat() const {
  switch (storage_format_) {
    case SerializedImageDataStorageFormat::kUint8Clamped:
      return ImageDataStorageFormat::kUint8;
    case SerializedImageDataStorageFormat::kUint16:
      return ImageDataStorageFormat::kUint16;
    case SerializedImageDataStorageFormat::kFloat32:
      return ImageDataStorageFormat::kFloat32;
  }
  NOTREACHED_IN_MIGRATION();
  return ImageDataStorageFormat::kUint8;
}

ImageDataSettings* SerializedImageDataSettings::GetImageDataSettings() const {
  ImageDataSettings* settings = ImageDataSettings::Create();
  settings->setColorSpace(PredefinedColorSpaceName(GetColorSpace()));
  settings->setStorageFormat(ImageDataStorageFormatName(GetStorageFormat()));
  return settings;
}

////////////////////////////////////////////////////////////////////////////////
// SerializedImageBitmapSettings

SerializedImageBitmapSettings::SerializedImageBitmapSettings() = default;

SerializedImageBitmapSettings::SerializedImageBitmapSettings(
    SkImageInfo info,
    ImageOrientationEnum image_orientation)
    : sk_color_space_(kSerializedParametricColorSpaceLength) {
  auto color_space =
      info.colorSpace() ? info.refColorSpace() : SkColorSpace::MakeSRGB();
  skcms_TransferFunction trfn = {};
  skcms_Matrix3x3 to_xyz = {};
  // The return value of `isNumericalTransferFn` is false for HLG and PQ
  // transfer functions, but `trfn` is still populated appropriately. DCHECK
  // that the constants for HLG and PQ have not changed.
  color_space->isNumericalTransferFn(&trfn);
  if (skcms_TransferFunction_isPQish(&trfn))
    DCHECK_EQ(trfn.g, kSerializedPQConstant);
  if (skcms_TransferFunction_isHLGish(&trfn))
    DCHECK_EQ(trfn.g, kSerializedHLGConstant);
  bool to_xyzd50_result = color_space->toXYZD50(&to_xyz);
  DCHECK(to_xyzd50_result);
  sk_color_space_.resize(16);
  sk_color_space_[0] = trfn.g;
  sk_color_space_[1] = trfn.a;
  sk_color_space_[2] = trfn.b;
  sk_color_space_[3] = trfn.c;
  sk_color_space_[4] = trfn.d;
  sk_color_space_[5] = trfn.e;
  sk_color_space_[6] = trfn.f;
  for (uint32_t i = 0; i < 3; ++i)
    for (uint32_t j = 0; j < 3; ++j)
      sk_color_space_[7 + 3 * i + j] = to_xyz.vals[i][j];

  switch (info.colorType()) {
    default:
    case kRGBA_8888_SkColorType:
      pixel_format_ = SerializedPixelFormat::kRGBA8;
      break;
    case kBGRA_8888_SkColorType:
      pixel_format_ = SerializedPixelFormat::kBGRA8;
      break;
    case kRGB_888x_SkColorType:
      pixel_format_ = SerializedPixelFormat::kRGBX8;
      break;
    case kRGBA_F16_SkColorType:
      pixel_format_ = SerializedPixelFormat::kF16;
      break;
  }

  switch (info.alphaType()) {
    case kUnknown_SkAlphaType:
    case kPremul_SkAlphaType:
      opacity_mode_ = SerializedOpacityMode::kNonOpaque;
      is_premultiplied_ = true;
      break;
    case kUnpremul_SkAlphaType:
      opacity_mode_ = SerializedOpacityMode::kNonOpaque;
      is_premultiplied_ = false;
      break;
    case kOpaque_SkAlphaType:
      opacity_mode_ = SerializedOpacityMode::kOpaque;
      is_premultiplied_ = true;
      break;
  }

  switch (image_orientation) {
    case ImageOrientationEnum::kOriginTopLeft:
      image_orientation_ = SerializedImageOrientation::kTopLeft;
      break;
    case ImageOrientationEnum::kOriginTopRight:
      image_orientation_ = SerializedImageOrientation::kTopRight;
      break;
    case ImageOrientationEnum::kOriginBottomRight:
      image_orientation_ = SerializedImageOrientation::kBottomRight;
      break;
    case ImageOrientationEnum::kOriginBottomLeft:
      image_orientation_ = SerializedImageOrientation::kBottomLeft;
      break;
    case ImageOrientationEnum::kOriginLeftTop:
      image_orientation_ = SerializedImageOrientation::kLeftTop;
      break;
    case ImageOrientationEnum::kOriginRightTop:
      image_orientation_ = SerializedImageOrientation::kRightTop;
      break;
    case ImageOrientationEnum::kOriginRightBottom:
      image_orientation_ = SerializedImageOrientation::kRightBottom;
      break;
    case ImageOrientationEnum::kOriginLeftBottom:
      image_orientation_ = SerializedImageOrientation::kLeftBottom;
      break;
  }
}

SerializedImageBitmapSettings::SerializedImageBitmapSettings(
    SerializedPredefinedColorSpace color_space,
    const Vector<double>& sk_color_space,
    SerializedPixelFormat pixel_format,
    SerializedOpacityMode opacity_mode,
    uint32_t is_premultiplied,
    SerializedImageOrientation image_orientation)
    : color_space_(color_space),
      sk_color_space_(sk_color_space),
      pixel_format_(pixel_format),
      opacity_mode_(opacity_mode),
      is_premultiplied_(is_premultiplied),
      image_orientation_(image_orientation) {}

SkImageInfo SerializedImageBitmapSettings::GetSkImageInfo(
    uint32_t width,
    uint32_t height) const {
  sk_sp<SkColorSpace> sk_color_space =
      PredefinedColorSpaceToSkColorSpace(DeserializeColorSpace(color_space_));

  if (sk_color_space_.size() == kSerializedParametricColorSpaceLength) {
    skcms_TransferFunction trfn;
    skcms_Matrix3x3 to_xyz;
    trfn.g = static_cast<float>(sk_color_space_[0]);
    trfn.a = static_cast<float>(sk_color_space_[1]);
    trfn.b = static_cast<float>(sk_color_space_[2]);
    trfn.c = static_cast<float>(sk_color_space_[3]);
    trfn.d = static_cast<float>(sk_color_space_[4]);
    trfn.e = static_cast<float>(sk_color_space_[5]);
    trfn.f = static_cast<float>(sk_color_space_[6]);
    for (uint32_t i = 0; i < 3; ++i)
      for (uint32_t j = 0; j < 3; ++j)
        to_xyz.vals[i][j] = static_cast<float>(sk_color_space_[7 + 3 * i + j]);
    sk_color_space = SkColorSpace::MakeRGB(trfn, to_xyz);
  }

  SkColorType sk_color_type = kRGBA_8888_SkColorType;
  switch (pixel_format_) {
    case SerializedPixelFormat::kNative8_LegacyObsolete:
      sk_color_type = kN32_SkColorType;
      break;
    case SerializedPixelFormat::kRGBA8:
      sk_color_type = kRGBA_8888_SkColorType;
      break;
    case SerializedPixelFormat::kBGRA8:
      sk_color_type = kBGRA_8888_SkColorType;
      break;
    case SerializedPixelFormat::kRGBX8:
      sk_color_type = kRGB_888x_SkColorType;
      break;
    case SerializedPixelFormat::kF16:
      sk_color_type = kRGBA_F16_SkColorType;
      break;
  }

  SkAlphaType sk_alpha_type = kPremul_SkAlphaType;
  if (opacity_mode_ == SerializedOpacityMode::kOpaque) {
    sk_alpha_type = kOpaque_SkAlphaType;
  } else if (is_premultiplied_) {
    sk_alpha_type = kPremul_SkAlphaType;
  } else {
    sk_alpha_type = kUnpremul_SkAlphaType;
  }

  return SkImageInfo::Make(width, height, sk_color_type, sk_alpha_type,
                           std::move(sk_color_space));
}

ImageOrientationEnum SerializedImageBitmapSettings::GetImageOrientation()
    const {
  switch (image_orientation_) {
    case SerializedImageOrientation::kTopLeft:
      return ImageOrientationEnum::kOriginTopLeft;
    case SerializedImageOrientation::kTopRight:
      return ImageOrientationEnum::kOriginTopRight;
    case SerializedImageOrientation::kBottomRight:
      return ImageOrientationEnum::kOriginBottomRight;
    case SerializedImageOrientation::kBottomLeft:
      return ImageOrientationEnum::kOriginBottomLeft;
    case SerializedImageOrientation::kLeftTop:
      return ImageOrientationEnum::kOriginLeftTop;
    case SerializedImageOrientation::kRightTop:
      return ImageOrientationEnum::kOriginRightTop;
    case SerializedImageOrientation::kRightBottom:
      return ImageOrientationEnum::kOriginRightBottom;
    case SerializedImageOrientation::kLeftBottom:
      return ImageOrientationEnum::kOriginLeftBottom;
  }
  NOTREACHED_IN_MIGRATION();
  return ImageOrientationEnum::kOriginTopLeft;
}

}  // namespace blink
