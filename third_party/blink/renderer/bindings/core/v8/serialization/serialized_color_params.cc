// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_color_params.h"

#include "build/build_config.h"

namespace blink {

SerializedColorParams::SerializedColorParams() = default;

SerializedColorParams::SerializedColorParams(CanvasColorParams color_params)
    : SerializedColorParams(color_params.ColorSpace(),
                            kUint8ClampedArrayStorageFormat) {
  switch (color_params.PixelFormat()) {
    case CanvasPixelFormat::kRGBA8:
      pixel_format_ = SerializedPixelFormat::kRGBA8;
      break;
    case CanvasPixelFormat::kBGRA8:
      pixel_format_ = SerializedPixelFormat::kBGRA8;
      break;
    case CanvasPixelFormat::kF16:
      pixel_format_ = SerializedPixelFormat::kF16;
      break;
  }

  opacity_mode_ = SerializedOpacityMode::kNonOpaque;
  if (color_params.GetOpacityMode() == blink::kOpaque)
    opacity_mode_ = SerializedOpacityMode::kOpaque;
}

SerializedColorParams::SerializedColorParams(
    CanvasColorSpace color_space,
    ImageDataStorageFormat storage_format) {
  switch (color_space) {
    case CanvasColorSpace::kSRGB:
      color_space_ = SerializedColorSpace::kSRGB;
      break;
    case CanvasColorSpace::kRec2020:
      color_space_ = SerializedColorSpace::kRec2020;
      break;
    case CanvasColorSpace::kP3:
      color_space_ = SerializedColorSpace::kP3;
      break;
  }
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

SerializedColorParams::SerializedColorParams(
    SerializedColorSpace color_space,
    SerializedPixelFormat pixel_format,
    SerializedOpacityMode opacity_mode,
    SerializedImageDataStorageFormat storage_format)
    : color_space_(color_space),
      pixel_format_(pixel_format),
      opacity_mode_(opacity_mode),
      storage_format_(storage_format) {}

CanvasColorParams SerializedColorParams::GetCanvasColorParams() const {
  CanvasColorSpace color_space = CanvasColorSpace::kSRGB;
  switch (color_space_) {
    case SerializedColorSpace::kLegacyObsolete:
    case SerializedColorSpace::kSRGB:
      color_space = CanvasColorSpace::kSRGB;
      break;
    case SerializedColorSpace::kRec2020:
      color_space = CanvasColorSpace::kRec2020;
      break;
    case SerializedColorSpace::kP3:
      color_space = CanvasColorSpace::kP3;
      break;
  }

  CanvasPixelFormat pixel_format = CanvasPixelFormat::kRGBA8;
  switch (pixel_format_) {
    case SerializedPixelFormat::kNative8_LegacyObsolete:
#if defined(OS_ANDROID)
      pixel_format = CanvasPixelFormat::kRGBA8;
#else
      pixel_format = CanvasPixelFormat::kBGRA8;
#endif
      break;
    case SerializedPixelFormat::kRGBA8:
      pixel_format = CanvasPixelFormat::kRGBA8;
      break;
    case SerializedPixelFormat::kBGRA8:
      pixel_format = CanvasPixelFormat::kBGRA8;
      break;
    case SerializedPixelFormat::kF16:
      pixel_format = CanvasPixelFormat::kF16;
      break;
  }

  blink::OpacityMode opacity_mode = blink::kNonOpaque;
  if (opacity_mode_ == SerializedOpacityMode::kOpaque)
    opacity_mode = blink::kOpaque;

  return CanvasColorParams(color_space, pixel_format, opacity_mode);
}

CanvasColorSpace SerializedColorParams::GetColorSpace() const {
  return GetCanvasColorParams().ColorSpace();
}

ImageDataStorageFormat SerializedColorParams::GetStorageFormat() const {
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

}  // namespace blink
