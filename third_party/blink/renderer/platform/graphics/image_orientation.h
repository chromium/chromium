/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_ORIENTATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_ORIENTATION_H_

#include <stdint.h>

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace gfx {
class SizeF;
}

namespace blink {

class AffineTransform;

// This enum intentionally matches the orientation values from the EXIF spec.
// See JEITA CP-3451, page 18. http://www.exif.org/Exif2-2.PDF
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ImageOrientationEnum : int8_t {
  // "TopLeft" means that the 0 row starts at the Top, the 0 column starts at
  // the Left.
  kOriginTopLeft = 1,      // default
  kOriginTopRight = 2,     // mirror along y-axis
  kOriginBottomRight = 3,  // 180 degree rotation
  kOriginBottomLeft = 4,   // mirror along the x-axis
  kOriginLeftTop = 5,      // mirror along x-axis + 270 degree CW rotation
  kOriginRightTop = 6,     // 90 degree CW rotation
  kOriginRightBottom = 7,  // mirror along x-axis + 90 degree CW rotation
  kOriginLeftBottom = 8,   // 270 degree CW rotation
  // All other values are "reserved" as of EXIF 2.2
  kDefault = kOriginTopLeft,
  kMinValue = kOriginTopLeft,
  kMaxValue = kOriginLeftBottom,
};

enum RespectImageOrientationEnum {
  kDoNotRespectImageOrientation = 0,
  kRespectImageOrientation = 1
};

class PLATFORM_EXPORT ImageOrientation final {
  DISALLOW_NEW();

 public:
  ImageOrientation(
      ImageOrientationEnum orientation = ImageOrientationEnum::kDefault)
      : orientation_(orientation) {}

  bool UsesWidthAsHeight() const {
    // Values 5 through 8 all flip the width/height.
    return orientation_ >= ImageOrientationEnum::kOriginLeftTop;
  }

  // This transform can be used for drawing an image according to the
  // orientation. It should be used in a right-handed coordinate system.
  AffineTransform TransformFromDefault(const gfx::SizeF& drawn_size) const;

  // This transform can be used to reverse an image orientation, it's for
  // drawing an image according to the way it is encoded. It should be used in a
  // right-handed coordinate system.
  AffineTransform TransformToDefault(const gfx::SizeF& drawn_size) const;

  inline bool operator==(const ImageOrientation& other) const {
    return other.orientation_ == orientation_;
  }
  inline bool operator!=(const ImageOrientation& other) const {
    return !(*this == other);
  }

  ImageOrientationEnum Orientation() const { return orientation_; }

 private:
  ImageOrientationEnum orientation_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_ORIENTATION_H_
