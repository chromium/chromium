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
  kLast = kP3,
};

// This enumeration specifies the values used to serialize CanvasPixelFormat.
enum class SerializedPixelFormat : uint32_t {
  // This is to preserve legacy object when Native was a possible enum state
  // this will be resolved as either a RGB or BGR pixel format for
  // canvas_color_params
  kNative8_LegacyObsolete = 0,
  kF16 = 1,
  kRGBA8 = 2,
  kBGRA8 = 3,
  kRGBX8 = 4,
  kLast = kRGBX8,
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

class SerializedImageDataSettings {
 public:
  SerializedImageDataSettings(CanvasColorSpace, ImageDataStorageFormat);
  SerializedImageDataSettings(SerializedColorSpace,
                              SerializedImageDataStorageFormat);

  CanvasColorSpace GetColorSpace() const;
  ImageDataStorageFormat GetStorageFormat() const;
  ImageDataSettings* GetImageDataSettings() const;

  SerializedColorSpace GetSerializedColorSpace() const { return color_space_; }
  SerializedImageDataStorageFormat GetSerializedImageDataStorageFormat() const {
    return storage_format_;
  }

 private:
  SerializedColorSpace color_space_ = SerializedColorSpace::kSRGB;
  SerializedImageDataStorageFormat storage_format_ =
      SerializedImageDataStorageFormat::kUint8Clamped;
};

class SerializedImageBitmapSettings {
 public:
  SerializedImageBitmapSettings();
  explicit SerializedImageBitmapSettings(SkImageInfo);
  SerializedImageBitmapSettings(SerializedColorSpace,
                                SerializedPixelFormat,
                                SerializedOpacityMode,
                                uint32_t is_premultiplied);

  SkImageInfo GetSkImageInfo(uint32_t width, uint32_t height) const;

  SerializedColorSpace GetSerializedColorSpace() const { return color_space_; }
  SerializedPixelFormat GetSerializedPixelFormat() const {
    return pixel_format_;
  }
  SerializedOpacityMode GetSerializedOpacityMode() const {
    return opacity_mode_;
  }
  uint32_t IsPremultiplied() const { return is_premultiplied_; }

 private:
  SerializedColorSpace color_space_ = SerializedColorSpace::kSRGB;
  SerializedPixelFormat pixel_format_ = SerializedPixelFormat::kRGBA8;
  SerializedOpacityMode opacity_mode_ = SerializedOpacityMode::kNonOpaque;
  bool is_premultiplied_ = true;
};

}  // namespace blink

#endif
