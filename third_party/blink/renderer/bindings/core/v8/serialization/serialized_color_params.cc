// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_color_params.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"

namespace blink {

namespace {

SerializedColorSpace SerializeColorSpace(PredefinedColorSpace color_space) {
  switch (color_space) {
    case PredefinedColorSpace::kSRGB:
      return SerializedColorSpace::kSRGB;
    case PredefinedColorSpace::kRec2020:
      return SerializedColorSpace::kRec2020;
    case PredefinedColorSpace::kP3:
      return SerializedColorSpace::kP3;
    case PredefinedColorSpace::kRec2100HLG:
      return SerializedColorSpace::kRec2100HLG;
    case PredefinedColorSpace::kRec2100PQ:
      return SerializedColorSpace::kRec2100PQ;
    case PredefinedColorSpace::kSRGBLinear:
      return SerializedColorSpace::kSRGBLinear;
  }
  NOTREACHED();
  return SerializedColorSpace::kSRGB;
}

PredefinedColorSpace DeserializeColorSpace(
    SerializedColorSpace serialized_color_space) {
  switch (serialized_color_space) {
    case SerializedColorSpace::kLegacyObsolete:
    case SerializedColorSpace::kSRGB:
      return PredefinedColorSpace::kSRGB;
    case SerializedColorSpace::kRec2020:
      return PredefinedColorSpace::kRec2020;
    case SerializedColorSpace::kP3:
      return PredefinedColorSpace::kP3;
    case SerializedColorSpace::kRec2100HLG:
      return PredefinedColorSpace::kRec2100HLG;
    case SerializedColorSpace::kRec2100PQ:
      return PredefinedColorSpace::kRec2100PQ;
    case SerializedColorSpace::kSRGBLinear:
      return PredefinedColorSpace::kSRGBLinear;
  }
  NOTREACHED();
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
    SerializedColorSpace color_space,
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
  NOTREACHED();
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

SerializedImageBitmapSettings::SerializedImageBitmapSettings(SkImageInfo info) {
  color_space_ = SerializeColorSpace(
      PredefinedColorSpaceFromSkColorSpace(info.colorSpace()));

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
}

SerializedImageBitmapSettings::SerializedImageBitmapSettings(
    SerializedColorSpace color_space,
    SerializedPixelFormat pixel_format,
    SerializedOpacityMode opacity_mode,
    uint32_t is_premultiplied)
    : color_space_(color_space),
      pixel_format_(pixel_format),
      opacity_mode_(opacity_mode),
      is_premultiplied_(is_premultiplied) {}

SkImageInfo SerializedImageBitmapSettings::GetSkImageInfo(
    uint32_t width,
    uint32_t height) const {
  sk_sp<SkColorSpace> sk_color_space =
      PredefinedColorSpaceToSkColorSpace(DeserializeColorSpace(color_space_));

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

}  // namespace blink
