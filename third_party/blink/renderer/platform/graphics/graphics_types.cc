/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2012 Rik Cabanier (cabanier@adobe.com)
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/graphics_types.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// TODO(vmpstr): Move these closer to canvas, along with the parsing code.
static const char* const kCanvasCompositeOperatorNames[] = {"clear",
                                                            "copy",
                                                            "source-over",
                                                            "source-in",
                                                            "source-out",
                                                            "source-atop",
                                                            "destination-over",
                                                            "destination-in",
                                                            "destination-out",
                                                            "destination-atop",
                                                            "xor",
                                                            "lighter"};

static const char* const kCanvasBlendModeNames[] = {
    "normal",     "multiply",   "screen",      "overlay",
    "darken",     "lighten",    "color-dodge", "color-burn",
    "hard-light", "soft-light", "difference",  "exclusion",
    "hue",        "saturation", "color",       "luminosity"};
const int kNumCompositeOperatorNames = std::size(kCanvasCompositeOperatorNames);
const int kNumBlendModeNames = std::size(kCanvasBlendModeNames);

bool ParseCanvasCompositeAndBlendMode(const String& s,
                                      CompositeOperator& op,
                                      BlendMode& blend_op) {
  for (int i = 0; i < kNumCompositeOperatorNames; i++) {
    if (s == kCanvasCompositeOperatorNames[i]) {
      op = static_cast<CompositeOperator>(i);
      blend_op = BlendMode::kNormal;
      return true;
    }
  }

  for (int i = 0; i < kNumBlendModeNames; i++) {
    if (s == kCanvasBlendModeNames[i]) {
      blend_op = static_cast<BlendMode>(i);
      op = kCompositeSourceOver;
      return true;
    }
  }

  return false;
}

String CanvasCompositeOperatorName(CompositeOperator op, BlendMode blend_op) {
  DCHECK_GE(op, 0);
  DCHECK_LT(op, kNumCompositeOperatorNames);
  DCHECK_GE(static_cast<int>(blend_op), 0);
  DCHECK_LT(static_cast<int>(blend_op), kNumBlendModeNames);
  if (blend_op != BlendMode::kNormal)
    return kCanvasBlendModeNames[static_cast<unsigned>(blend_op)];
  return kCanvasCompositeOperatorNames[op];
}

String BlendModeToString(BlendMode blend_op) {
  switch (blend_op) {
    case BlendMode::kNormal:
      return "normal";
    case BlendMode::kMultiply:
      return "multiply";
    case BlendMode::kScreen:
      return "screen";
    case BlendMode::kOverlay:
      return "overlay";
    case BlendMode::kDarken:
      return "darken";
    case BlendMode::kLighten:
      return "lighten";
    case BlendMode::kColorDodge:
      return "color-dodge";
    case BlendMode::kColorBurn:
      return "color-burn";
    case BlendMode::kHardLight:
      return "hard-light";
    case BlendMode::kSoftLight:
      return "soft-light";
    case BlendMode::kDifference:
      return "difference";
    case BlendMode::kExclusion:
      return "exclusion";
    case BlendMode::kHue:
      return "hue";
    case BlendMode::kSaturation:
      return "saturation";
    case BlendMode::kColor:
      return "color";
    case BlendMode::kLuminosity:
      return "luminosity";
    case BlendMode::kPlusLighter:
      return "plus-lighter";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

bool ParseImageEncodingMimeType(const String& mime_type_name,
                                ImageEncodingMimeType& mime_type) {
  if (mime_type_name == "image/png")
    mime_type = kMimeTypePng;
  else if (mime_type_name == "image/jpeg")
    mime_type = kMimeTypeJpeg;
  else if (mime_type_name == "image/webp")
    mime_type = kMimeTypeWebp;
  else
    return false;
  return true;
}

String ImageEncodingMimeTypeName(ImageEncodingMimeType mime_type) {
  DCHECK_GE(mime_type, 0);
  DCHECK_LT(mime_type, 3);
  const char* const kMimeTypeNames[3] = {"image/png", "image/jpeg",
                                         "image/webp"};
  return kMimeTypeNames[mime_type];
}

bool ParseLineCap(const String& s, LineCap& cap) {
  if (s == "butt") {
    cap = kButtCap;
    return true;
  }
  if (s == "round") {
    cap = kRoundCap;
    return true;
  }
  if (s == "square") {
    cap = kSquareCap;
    return true;
  }
  return false;
}

String LineCapName(LineCap cap) {
  DCHECK_GE(cap, 0);
  DCHECK_LT(cap, 3);
  const char* const kNames[3] = {"butt", "round", "square"};
  return kNames[cap];
}

bool ParseLineJoin(const String& s, LineJoin& join) {
  if (s == "miter") {
    join = kMiterJoin;
    return true;
  }
  if (s == "round") {
    join = kRoundJoin;
    return true;
  }
  if (s == "bevel") {
    join = kBevelJoin;
    return true;
  }
  return false;
}

String LineJoinName(LineJoin join) {
  DCHECK_GE(join, 0);
  DCHECK_LT(join, 3);
  const char* const kNames[3] = {"miter", "round", "bevel"};
  return kNames[join];
}

String TextAlignName(TextAlign align) {
  DCHECK_GE(align, 0);
  DCHECK_LT(align, 5);
  const char* const kNames[5] = {"start", "end", "left", "center", "right"};
  return kNames[align];
}

bool ParseTextAlign(const String& s, TextAlign& align) {
  if (s == "start") {
    align = kStartTextAlign;
    return true;
  }
  if (s == "end") {
    align = kEndTextAlign;
    return true;
  }
  if (s == "left") {
    align = kLeftTextAlign;
    return true;
  }
  if (s == "center") {
    align = kCenterTextAlign;
    return true;
  }
  if (s == "right") {
    align = kRightTextAlign;
    return true;
  }
  return false;
}

String TextBaselineName(TextBaseline baseline) {
  DCHECK_GE(baseline, 0);
  DCHECK_LT(baseline, 6);
  const char* const kNames[6] = {"alphabetic", "top",         "middle",
                                 "bottom",     "ideographic", "hanging"};
  return kNames[baseline];
}

bool ParseTextBaseline(const String& s, TextBaseline& baseline) {
  if (s == "alphabetic") {
    baseline = kAlphabeticTextBaseline;
    return true;
  }
  if (s == "top") {
    baseline = kTopTextBaseline;
    return true;
  }
  if (s == "middle") {
    baseline = kMiddleTextBaseline;
    return true;
  }
  if (s == "bottom") {
    baseline = kBottomTextBaseline;
    return true;
  }
  if (s == "ideographic") {
    baseline = kIdeographicTextBaseline;
    return true;
  }
  if (s == "hanging") {
    baseline = kHangingTextBaseline;
    return true;
  }
  return false;
}

String ImageDataStorageFormatName(ImageDataStorageFormat format) {
  switch (format) {
    case ImageDataStorageFormat::kUint8:
      return "uint8";
    case ImageDataStorageFormat::kUint16:
      return "uint16";
    case ImageDataStorageFormat::kFloat32:
      return "float32";
  }
  NOTREACHED_IN_MIGRATION();
  return String();
}

String PredefinedColorSpaceName(PredefinedColorSpace color_space) {
  switch (color_space) {
    case PredefinedColorSpace::kSRGB:
      return "srgb";
    case PredefinedColorSpace::kRec2020:
      return "rec2020";
    case PredefinedColorSpace::kP3:
      return "display-p3";
    case PredefinedColorSpace::kRec2100HLG:
      return "rec2100-hlg";
    case PredefinedColorSpace::kRec2100PQ:
      return "rec2100-pq";
    case PredefinedColorSpace::kSRGBLinear:
      return "srgb-linear";
  };
  NOTREACHED_IN_MIGRATION();
  return String();
}

String CanvasPixelFormatName(CanvasPixelFormat pixel_format) {
  switch (pixel_format) {
    case CanvasPixelFormat::kUint8:
      return "uint8";
    case CanvasPixelFormat::kF16:
      return "float16";
  }
  NOTREACHED_IN_MIGRATION();
  return String();
}

}  // namespace blink
