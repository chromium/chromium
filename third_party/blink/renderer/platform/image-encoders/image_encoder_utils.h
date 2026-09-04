// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_ENCODERS_IMAGE_ENCODER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_ENCODERS_IMAGE_ENCODER_UTILS_H_

#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace gfx {
struct HDRMetadata;
}

namespace blink {

class PLATFORM_EXPORT ImageEncoderUtils {
  STATIC_ONLY(ImageEncoderUtils);

 public:
  // Returns the color info to use for encoding to not lose precision or produce
  // color artifacts (e.g. unpremultiplied alpha and non-linear transfer
  // function).
  static SkColorInfo GetColorInfoForEncoder(
      const SkColorInfo& color_info,
      const gfx::HDRMetadata& hdr_metadata);

  // Default image mime type for toDataURL and toBlob functions
  static const char kDefaultRequestedMimeType[];
  static const ImageEncodingMimeType kDefaultEncodingMimeType;

  static ImageEncodingMimeType ToEncodingMimeType(const String&);

  static String MimeTypeName(ImageEncodingMimeType);
  static bool ParseMimeType(const String&, ImageEncodingMimeType&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_ENCODERS_IMAGE_ENCODER_UTILS_H_
