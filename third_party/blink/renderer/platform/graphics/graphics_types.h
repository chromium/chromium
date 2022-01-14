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
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"

namespace blink {

typedef uintptr_t DisplayItemClientId;
static const DisplayItemClientId kInvalidDisplayItemClientId = 0u;

enum AlphaDisposition {
  kPremultiplyAlpha,
  kUnpremultiplyAlpha,
  kDontChangeAlpha,
};

enum class PredefinedColorSpace {
  kSRGB,
  kRec2020,
  kP3,
  kRec2100HLG,
  kRec2100PQ,
  kSRGBLinear,
};

enum class CanvasPixelFormat {
  kUint8,
  kF16,
};

enum class ImageDataStorageFormat {
  kUint8,
  kUint16,
  kFloat32,
};

enum ImageEncodingMimeType {
  kMimeTypePng,
  kMimeTypeJpeg,
  kMimeTypeWebp,
};

enum StrokeStyle {
  kNoStroke,
  kSolidStroke,
  kDottedStroke,
  kDashedStroke,
  kDoubleStroke,
  kWavyStroke,
};

enum InterpolationQuality {
  kInterpolationNone = static_cast<int>(cc::PaintFlags::FilterQuality::kNone),
  kInterpolationLow = static_cast<int>(cc::PaintFlags::FilterQuality::kLow),
  kInterpolationMedium =
      static_cast<int>(cc::PaintFlags::FilterQuality::kMedium),
#if defined(WTF_USE_LOW_QUALITY_IMAGE_INTERPOLATION)
  kInterpolationDefault = kInterpolationLow,
#else
  kInterpolationDefault = kInterpolationMedium,
#endif
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

enum class BlendMode {
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
};

enum OpacityMode {
  kNonOpaque,
  kOpaque,
};

enum class RasterEffectOutset : uint8_t {
  kNone,
  kHalfPixel,
  kWholePixel,
};

// Specifies whether the provider should rasterize paint commands on the CPU
// or GPU. This is used to support software raster with GPU compositing.
enum class RasterMode {
  kGPU,
  kCPU,
};

enum class RasterModeHint {
  kPreferGPU,
  kPreferCPU,
};

enum MailboxSyncMode {
  kVerifiedSyncToken,
  kUnverifiedSyncToken,
  kOrderingBarrier,
};

enum AntiAliasingMode { kNotAntiAliased, kAntiAliased };

enum GradientSpreadMethod {
  kSpreadMethodPad,
  kSpreadMethodReflect,
  kSpreadMethodRepeat
};

enum LineCap {
  kButtCap = SkPaint::kButt_Cap,
  kRoundCap = SkPaint::kRound_Cap,
  kSquareCap = SkPaint::kSquare_Cap
};

enum LineJoin {
  kMiterJoin = SkPaint::kMiter_Join,
  kRoundJoin = SkPaint::kRound_Join,
  kBevelJoin = SkPaint::kBevel_Join
};

enum TextBaseline {
  kAlphabeticTextBaseline,
  kTopTextBaseline,
  kMiddleTextBaseline,
  kBottomTextBaseline,
  kIdeographicTextBaseline,
  kHangingTextBaseline
};

enum TextAlign {
  kStartTextAlign,
  kEndTextAlign,
  kLeftTextAlign,
  kCenterTextAlign,
  kRightTextAlign
};

enum TextDrawingMode {
  kTextModeFill = 1 << 0,
  kTextModeStroke = 1 << 1,
};
typedef unsigned TextDrawingModeFlags;

enum ColorFilter {
  kColorFilterNone,
  kColorFilterLuminanceToAlpha,
  kColorFilterSRGBToLinearRGB,
  kColorFilterLinearRGBToSRGB
};

enum WindRule {
  RULE_NONZERO = static_cast<int>(SkPathFillType::kWinding),
  RULE_EVENODD = static_cast<int>(SkPathFillType::kEvenOdd)
};

// Note that this is only appropriate to use in canvas globalCompositeOperator
// cases.
// TODO(vmpstr): Move these functions to near where they are used.
PLATFORM_EXPORT String CanvasCompositeOperatorName(CompositeOperator,
                                                   BlendMode);
PLATFORM_EXPORT bool ParseCanvasCompositeAndBlendMode(const String&,
                                                      CompositeOperator&,
                                                      BlendMode&);

PLATFORM_EXPORT String BlendModeToString(BlendMode);

PLATFORM_EXPORT String ImageEncodingMimeTypeName(ImageEncodingMimeType);
PLATFORM_EXPORT bool ParseImageEncodingMimeType(const String&,
                                                ImageEncodingMimeType&);

PLATFORM_EXPORT String LineCapName(LineCap);
PLATFORM_EXPORT bool ParseLineCap(const String&, LineCap&);

PLATFORM_EXPORT String LineJoinName(LineJoin);
PLATFORM_EXPORT bool ParseLineJoin(const String&, LineJoin&);

PLATFORM_EXPORT String TextAlignName(TextAlign);
PLATFORM_EXPORT bool ParseTextAlign(const String&, TextAlign&);

PLATFORM_EXPORT String TextBaselineName(TextBaseline);
PLATFORM_EXPORT bool ParseTextBaseline(const String&, TextBaseline&);

PLATFORM_EXPORT String PredefinedColorSpaceName(PredefinedColorSpace);

PLATFORM_EXPORT String CanvasPixelFormatName(CanvasPixelFormat);

PLATFORM_EXPORT String ImageDataStorageFormatName(ImageDataStorageFormat);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_TYPES_H_
