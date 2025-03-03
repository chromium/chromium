/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_TYPES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_TYPES_H_

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/geometry/path_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace WTF {
class String;
}

namespace blink {

using DynamicRangeLimit = ::cc::PaintFlags::DynamicRangeLimitMixture;

enum AlphaDisposition {
  kPremultiplyAlpha,
  kDontChangeAlpha,
};

enum ImageEncodingMimeType {
  kMimeTypePng,
  kMimeTypeJpeg,
  kMimeTypeWebp,
};

enum InterpolationQuality {
  kInterpolationNone = static_cast<int>(cc::PaintFlags::FilterQuality::kNone),
  kInterpolationLow = static_cast<int>(cc::PaintFlags::FilterQuality::kLow),
  kInterpolationMedium =
      static_cast<int>(cc::PaintFlags::FilterQuality::kMedium),
};

enum CompositeOperator {
  kCompositeClear,
  kCompositeCopy,
  kCompositeSourceOver,
  kCompositeSourceIn,
  kCompositeSourceOut,
  kCompositeSourceAtop,
  kCompositeDestinationOver,
  kCompositeDestinationIn,
  kCompositeDestinationOut,
  kCompositeDestinationAtop,
  kCompositeXOR,
  kCompositePlusLighter
};

enum class BlendMode : uint8_t {
  kNormal,
  kMultiply,
  kScreen,
  kOverlay,
  kDarken,
  kLighten,
  kColorDodge,
  kColorBurn,
  kHardLight,
  kSoftLight,
  kDifference,
  kExclusion,
  kHue,
  kSaturation,
  kColor,
  kLuminosity,
  // The following is only used in CSS mix-blend-mode, and maps to a composite
  // operator. Canvas uses the same enum but the kPlusLighter is not a valid
  // canvas value. We should consider splitting the enums.
  kPlusLighter,

  kMaxBlendMode = kPlusLighter,
};

enum OpacityMode {
  kNonOpaque,
  kOpaque,
};

enum AntiAliasingMode { kNotAntiAliased, kAntiAliased };

enum TextPaintOrder { kFillStroke, kStrokeFill };

enum TextDrawingMode {
  kTextModeFill = 1 << 0,
  kTextModeStroke = 1 << 1,
};
typedef unsigned TextDrawingModeFlags;

// Note that this is only appropriate to use in canvas globalCompositeOperator
// cases.
// TODO(vmpstr): Move these functions to near where they are used.
PLATFORM_EXPORT WTF::String CanvasCompositeOperatorName(CompositeOperator,
                                                        BlendMode);
PLATFORM_EXPORT bool ParseCanvasCompositeAndBlendMode(const WTF::String&,
                                                      CompositeOperator&,
                                                      BlendMode&);
PLATFORM_EXPORT InterpolationQuality GetDefaultInterpolationQuality();

PLATFORM_EXPORT WTF::String BlendModeToString(BlendMode);

PLATFORM_EXPORT WTF::String ImageEncodingMimeTypeName(ImageEncodingMimeType);
PLATFORM_EXPORT bool ParseImageEncodingMimeType(const WTF::String&,
                                                ImageEncodingMimeType&);

PLATFORM_EXPORT WTF::String LineCapName(LineCap);
PLATFORM_EXPORT bool ParseLineCap(const WTF::String&, LineCap&);

PLATFORM_EXPORT WTF::String LineJoinName(LineJoin);
PLATFORM_EXPORT bool ParseLineJoin(const WTF::String&, LineJoin&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_TYPES_H_
