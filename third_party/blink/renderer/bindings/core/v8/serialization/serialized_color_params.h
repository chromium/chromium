// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_SERIALIZED_COLOR_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_SERIALIZED_COLOR_PARAMS_H_

#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"

namespace blink {

// This enumeration specifies the extra tags used to specify the color settings
// of the serialized/deserialized ImageData and ImageBitmap objects.
enum class ImageSerializationTag : uint32_t {
  kEndTag = 0,
  // followed by a SerializedColorSpace enum.
  kCanvasColorSpaceTag = 1,
  // followed by a SerializedPixelFormat enum, used only for ImageBitmap.
  kCanvasPixelFormatTag = 2,
  // followed by a SerializedImageDataStorageFormat enum, used only for
  // ImageData.
  kImageDataStorageFormatTag = 3,
  // followed by 1 if the image is origin clean and zero otherwise.
  kOriginCleanTag = 4,
  // followed by 1 if the image is premultiplied and zero otherwise.
  kIsPremultipliedTag = 5,
  // followed by 1 if the image is known to be opaque (alpha = 1 everywhere)
  kCanvasOpacityModeTag = 6,

  kLast = kCanvasOpacityModeTag,
};

// This enumeration specifies the values used to serialize CanvasColorSpace.
enum class SerializedColorSpace : uint32_t {
  // Legacy sRGB color space is deprecated as of M65. Objects in legacy color
  // space will be serialized as sRGB since the legacy behavior is now merged
  // with sRGB color space. Deserialized objects with legacy color space also
  // will be interpreted as sRGB.
  kLegacyObsolete = 0,
  kSRGB = 1,
  kRec2020 = 2,
  kP3 = 3,
  kLinearRGB = 4,
  kLast = kLinearRGB,
};

// This enumeration specifies the values used to serialize CanvasPixelFormat.
enum class SerializedPixelFormat : uint32_t {
  kRGBA8 = 0,
  kF16 = 1,
  kLast = kF16,
};

// This enumeration specifies the values used to serialize
// ImageDataStorageFormat.
enum class SerializedImageDataStorageFormat : uint32_t {
  kUint8Clamped = 0,
  kUint16 = 1,
  kFloat32 = 2,
  kLast = kFloat32,
};

enum class SerializedOpacityMode : uint32_t {
  kNonOpaque = 0,
  kOpaque = 1,
  kLast = kOpaque,
};

class SerializedColorParams {
 public:
  SerializedColorParams();
  SerializedColorParams(CanvasColorParams);
  SerializedColorParams(CanvasColorParams, ImageDataStorageFormat);
  SerializedColorParams(SerializedColorSpace,
                        SerializedPixelFormat,
                        SerializedOpacityMode,
                        SerializedImageDataStorageFormat);

  CanvasColorParams GetCanvasColorParams() const;
  CanvasColorSpace GetColorSpace() const;
  ImageDataStorageFormat GetStorageFormat() const;

  void SetSerializedColorSpace(SerializedColorSpace);
  void SetSerializedPixelFormat(SerializedPixelFormat);
  void SetSerializedOpacityMode(SerializedOpacityMode);
  void SetSerializedImageDataStorageFormat(SerializedImageDataStorageFormat);

  SerializedColorSpace GetSerializedColorSpace() const;
  SerializedPixelFormat GetSerializedPixelFormat() const;
  SerializedImageDataStorageFormat GetSerializedImageDataStorageFormat() const;
  SerializedOpacityMode GetSerializedOpacityMode() const;

 private:
  SerializedColorSpace color_space_;
  SerializedPixelFormat pixel_format_;
  SerializedOpacityMode opacity_mode_;
  SerializedImageDataStorageFormat storage_format_;
};

}  // namespace blink

#endif
