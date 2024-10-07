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
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"

namespace WTF {
class String;
}

namespace blink {

typedef uintptr_t DisplayItemClientId;
static const DisplayItemClientId kInvalidDisplayItemClientId = 0u;

using DynamicRangeLimit = ::cc::PaintFlags::DynamicRangeLimitMixture;

enum AlphaDisposition {
  kPremultiplyAlpha,
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

enum TextPaintOrder { kFillStroke, kStrokeFill };

enum TextDrawingMode {
  kTextModeFill = 1 << 0,
  kTextModeStroke = 1 << 1,
};
typedef unsigned TextDrawingModeFlags;

enum WindRule {
  RULE_NONZERO = static_cast<int>(SkPathFillType::kWinding),
  RULE_EVENODD = static_cast<int>(SkPathFillType::kEvenOdd)
};

// Reasons for requesting that recorded PaintOps be flushed. Used in code
// loosely related to 2d canvas rendering contexts.
enum class FlushReason {
  // This enum is used by a histogram. Do not change item values.

  // Use at call sites that never require flushing recorded paint ops
  // For example when requesting WebGL or WebGPU snapshots. Does not
  // impede vector printing.
  kNone = 0,

  // Used in C++ unit tests
  kTesting = 1,

  // Call site may be flushing paint ops, but they're for a use case
  // unrelated to Canvas rendering contexts. Does not impede vector printing.
  kNon2DCanvas = 2,

  // Canvas contents were cleared. This makes the canvas vector printable
  // again.
  kClear = 3,

  // The canvas content is being swapped-out because its tab is hidden.
  // Should not happen while printing.
  kHibernating = 4,

  // `OffscreenCanvas::commit` was called.
  // Should not happen while printing.
  kOffscreenCanvasCommit_OBSOLETE = 5,

  // `OffscreenCanvas` dispatched a frame to the compositor as part of the
  // regular animation frame presentation flow.
  // Should not happen while printing.
  kOffscreenCanvasPushFrame = 6,

  // createImageBitmap() was called with the canvas as its argument.
  // Should not happen while printing.
  kCreateImageBitmap = 7,

  // The `getImageData` API method was called on the canvas's 2d context.
  // This inhibits vector printing.
  kGetImageData = 8,

  // A paint op was recorded that referenced a volatile source image and
  // therefore the recording needed to be flush immediately before the
  // source image contents could be overwritten. For example, a video frame.
  // This inhibits vector printing.
  kVolatileSourceImage = 9,

  // The canvas element dispatched a frame to the compositor
  // This inhibits vector printing.
  kCanvasPushFrame = 10,

  // The canvas element dispatched a frame to the compositor while printing
  // was in progress.
  // This does not prevent vector printing as long as the current frame is
  // clear.
  kCanvasPushFrameWhilePrinting = 11,

  // Direct write access to the pixel buffer (e.g. `putImageData`)
  // This inhibits vector printing.
  kWritePixels = 12,

  // To blob was called on the canvas.
  // This inhibits vector printing.
  kToBlob = 13,

  // A `VideoFrame` object was created with the canvas as an image source
  // This inhibits vector printing.
  kCreateVideoFrame = 14,

  // The canvas was used as a source image in a call to
  // `CanvasRenderingContext2D.drawImage`.
  // This inhibits vector printing.
  kDrawImage = 15,

  // The canvas is observed by a `CanvasDrawListener`. This typically means
  // that canvas contents are being streamed to a WebRTC video stream.
  // This inhibits vector printing.
  kDrawListener = 16,

  // The canvas contents were painted to its parent content layer, this
  // is the non-composited presentation code path.
  // This should never happen while printing.
  kPaint = 17,

  // Canvas contents were transferred to an `ImageBitmap`. This does not
  // inhibit vector printing since it effectively clears the canvas.
  kTransfer = 18,

  // The canvas is being printed.
  kPrinting = 19,

  // The canvas was loaded as a WebGPU external image.
  // This inhibits vector printing.
  kWebGPUExternalImage = 20,

  // The canvas was processed by a `ShapeDetector`.
  // This inhibits vector printing.
  kShapeDetector = 21,

  // The canvas was uploaded to a WebGL texture.
  // This inhibits vector printing.
  kWebGLTexImage = 22,

  // The canvas was used as a source in a call to
  // `CanvasRenderingContext2D.createPattern`.
  // This inhibits vector printing.
  kCreatePattern = 23,

  // The canvas contents were copied to the clipboard.
  // This inhibits vector printing.
  kClipboard = 24,

  // The canvas's recorded ops had a reference to an image whose contents
  // were about to change.
  // This inhibits vector printing.
  kSourceImageWillChange = 25,

  // The canvas was uploade to a WebGPU texture.
  // This inhibits vector printing.
  kWebGPUTexture = 26,

  // The HTMLCanvasElement.toDataURL method was called on the canvas.
  kToDataURL = 27,

  // The canvas's layer bridge was replaced. This happens when switching
  // between GPU and CPU rendering.
  // This inhibits vector printing.
  kReplaceLayerBridge = 28,

  // The auto-flush heuristic kicked-in. Should not happen while
  // printing.
  kRecordingLimitExceeded = 29,

  // The canvas was used as a source image in a call to
  // `CanvasRenderingContext2D.drawMesh`.
  // This inhibits vector printing.
  kDrawMesh = 30,

  kMaxValue = kDrawMesh,
};

// Note that this is only appropriate to use in canvas globalCompositeOperator
// cases.
// TODO(vmpstr): Move these functions to near where they are used.
PLATFORM_EXPORT WTF::String CanvasCompositeOperatorName(CompositeOperator,
                                                        BlendMode);
PLATFORM_EXPORT bool ParseCanvasCompositeAndBlendMode(const WTF::String&,
                                                      CompositeOperator&,
                                                      BlendMode&);

PLATFORM_EXPORT WTF::String BlendModeToString(BlendMode);

PLATFORM_EXPORT WTF::String ImageEncodingMimeTypeName(ImageEncodingMimeType);
PLATFORM_EXPORT bool ParseImageEncodingMimeType(const WTF::String&,
                                                ImageEncodingMimeType&);

PLATFORM_EXPORT WTF::String LineCapName(LineCap);
PLATFORM_EXPORT bool ParseLineCap(const WTF::String&, LineCap&);

PLATFORM_EXPORT WTF::String LineJoinName(LineJoin);
PLATFORM_EXPORT bool ParseLineJoin(const WTF::String&, LineJoin&);

PLATFORM_EXPORT WTF::String TextAlignName(TextAlign);
PLATFORM_EXPORT bool ParseTextAlign(const WTF::String&, TextAlign&);

PLATFORM_EXPORT WTF::String TextBaselineName(TextBaseline);
PLATFORM_EXPORT bool ParseTextBaseline(const WTF::String&, TextBaseline&);

PLATFORM_EXPORT WTF::String PredefinedColorSpaceName(PredefinedColorSpace);

PLATFORM_EXPORT WTF::String CanvasPixelFormatName(CanvasPixelFormat);

PLATFORM_EXPORT WTF::String ImageDataStorageFormatName(ImageDataStorageFormat);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_TYPES_H_
