// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_color_params.h"

#include "build/build_config.h"

namespace blink {

namespace {

SerializedColorSpace SerializeColorSpace(CanvasColorSpace color_space) {
  switch (color_space) {
    case CanvasColorSpace::kSRGB:
      return SerializedColorSpace::kSRGB;
    case CanvasColorSpace::kRec2020:
      return SerializedColorSpace::kRec2020;
    case CanvasColorSpace::kP3:
      return SerializedColorSpace::kP3;
  }
  NOTREACHED();
  return SerializedColorSpace::kSRGB;
}

CanvasColorSpace DeserializeColorSpace(
    SerializedColorSpace serialized_color_space) {
  switch (serialized_color_space) {
    case SerializedColorSpace::kLegacyObsolete:
    case SerializedColorSpace::kSRGB:
      return CanvasColorSpace::kSRGB;
    case SerializedColorSpace::kRec2020:
      return CanvasColorSpace::kRec2020;
    case SerializedColorSpace::kP3:
      return CanvasColorSpace::kP3;
  }
  NOTREACHED();
  return CanvasColorSpace::kSRGB;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SerializedImageDataSettings

SerializedImageDataSettings::SerializedImageDataSettings(
    CanvasColorSpace color_space,
    ImageDataStorageFormat storage_format)
    : color_space_(SerializeColorSpace(color_space)) {
  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      storage_format_ = SerializedImageDataStorageFormat::kUint8Clamped;
      break;
    case kUint16ArrayStorageFormat:
      storage_format_ = SerializedImageDataStorageFormat::kUint16;
      break;
    case kFloat32ArrayStorageFormat:
      storage_format_ = SerializedImageDataStorageFormat::kFloat32;
      break;
  }
}

SerializedImageDataSettings::SerializedImageDataSettings(
    SerializedColorSpace color_space,
    SerializedImageDataStorageFormat storage_format)
    : color_space_(color_space), storage_format_(storage_format) {}

CanvasColorSpace SerializedImageDataSettings::GetColorSpace() const {
  return DeserializeColorSpace(color_space_);
}

ImageDataStorageFormat SerializedImageDataSettings::GetStorageFormat() const {
  switch (storage_format_) {
    case SerializedImageDataStorageFormat::kUint8Clamped:
      return kUint8ClampedArrayStorageFormat;
    case SerializedImageDataStorageFormat::kUint16:
      return kUint16ArrayStorageFormat;
    case SerializedImageDataStorageFormat::kFloat32:
      return kFloat32ArrayStorageFormat;
  }
  NOTREACHED();
  return kUint8ClampedArrayStorageFormat;
}

ImageDataSettings* SerializedImageDataSettings::GetImageDataSettings() const {
  ImageDataSettings* settings = ImageDataSettings::Create();
  switch (DeserializeColorSpace(color_space_)) {
    case CanvasColorSpace::kSRGB:
      settings->setColorSpace(kSRGBCanvasColorSpaceName);
      break;
    case CanvasColorSpace::kRec2020:
      settings->setColorSpace(kRec2020CanvasColorSpaceName);
      break;
    case CanvasColorSpace::kP3:
      settings->setColorSpace(kP3CanvasColorSpaceName);
      break;
  }
  switch (storage_format_) {
    case SerializedImageDataStorageFormat::kUint8Clamped:
      settings->setStorageFormat(kUint8ClampedArrayStorageFormatName);
      break;
    case SerializedImageDataStorageFormat::kUint16:
      settings->setStorageFormat(kUint16ArrayStorageFormatName);
      break;
    case SerializedImageDataStorageFormat::kFloat32:
      settings->setStorageFormat(kFloat32ArrayStorageFormatName);
      break;
  }
  return settings;
}

////////////////////////////////////////////////////////////////////////////////
// SerializedImageBitmapSettings

SerializedImageBitmapSettings::SerializedImageBitmapSettings() = default;

SerializedImageBitmapSettings::SerializedImageBitmapSettings(SkImageInfo info) {
  color_space_ =
      SerializeColorSpace(CanvasColorSpaceFromSkColorSpace(info.colorSpace()));

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
      CanvasColorSpaceToSkColorSpace(DeserializeColorSpace(color_space_));

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
