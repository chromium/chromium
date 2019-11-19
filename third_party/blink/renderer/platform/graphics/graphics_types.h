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

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"

namespace blink {

enum AlphaDisposition {
  kPremultiplyAlpha,
  kUnpremultiplyAlpha,
  kDontChangeAlpha,
};

enum DataU8ColorType {
  kRGBAColorType,
  kN32ColorType,
};

enum ImageDataStorageFormat {
  kUint8ClampedArrayStorageFormat,
  kUint16ArrayStorageFormat,
  kFloat32ArrayStorageFormat,
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
  kInterpolationNone = kNone_SkFilterQuality,
  kInterpolationLow = kLow_SkFilterQuality,
  kInterpolationMedium = kMedium_SkFilterQuality,
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
};

enum OpacityMode {
  kNonOpaque,
  kOpaque,
};

enum AccelerationHint {
  kPreferAcceleration,
  // The PreferAccelerationAfterVisibilityChange hint suggests we should switch
  // back to acceleration in the context of the canvas becoming visible again.
  kPreferAccelerationAfterVisibilityChange,
  kPreferNoAcceleration,
};

enum MailboxSyncMode {
  kVerifiedSyncToken,
  kUnverifiedSyncToken,
  kOrderingBarrier,
};

enum class DarkModeClassification {
  kNotClassified,
  kApplyFilter,
  kDoNotApplyFilter,
};

// TODO(junov): crbug.com/453113 Relocate ShadowMode to
// CanvasRenderingContext2DState.h once GraphicsContext no longer uses it.
enum ShadowMode {
  kDrawShadowAndForeground,
  kDrawShadowOnly,
  kDrawForegroundOnly
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

enum HorizontalAlignment { kAlignLeft, kAlignRight, kAlignHCenter };

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
  RULE_NONZERO = SkPath::kWinding_FillType,
  RULE_EVENODD = SkPath::kEvenOdd_FillType
};

PLATFORM_EXPORT String CompositeOperatorName(CompositeOperator, BlendMode);
PLATFORM_EXPORT bool ParseCompositeAndBlendMode(const String&,
                                                CompositeOperator&,
                                                BlendMode&);

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

}  // namespace blink

#endif
