/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {

enum ColorParseResult {
  kParsedRGBA,
  kParsedCurrentColor,
  kParsedSystemColor,
  kParseFailed
};

static ColorParseResult ParseColor(Color& parsed_color,
                                   const String& color_string,
                                   WebColorScheme color_scheme) {
  if (DeprecatedEqualIgnoringCase(color_string, "currentcolor"))
    return kParsedCurrentColor;
  const bool kUseStrictParsing = true;
  if (CSSParser::ParseColor(parsed_color, color_string, kUseStrictParsing))
    return kParsedRGBA;
  if (CSSParser::ParseSystemColor(parsed_color, color_string, color_scheme))
    return kParsedSystemColor;
  return kParseFailed;
}

static Color CurrentColor(HTMLCanvasElement* canvas) {
  if (!canvas || !canvas->isConnected() || !canvas->InlineStyle())
    return Color::kBlack;
  Color color = Color::kBlack;
  CSSParser::ParseColor(
      color, canvas->InlineStyle()->GetPropertyValue(CSSPropertyID::kColor));
  return color;
}

static WebColorScheme ColorScheme(HTMLCanvasElement* canvas) {
  if (canvas && canvas->isConnected()) {
    if (auto* style = canvas->GetComputedStyle())
      return style->UsedColorScheme();
  }
  return ComputedStyle::InitialStyle().UsedColorScheme();
}

bool ParseColorOrCurrentColor(Color& parsed_color,
                              const String& color_string,
                              HTMLCanvasElement* canvas) {
  ColorParseResult parse_result =
      ParseColor(parsed_color, color_string.StripWhiteSpace(IsHTMLSpace<UChar>),
                 ColorScheme(canvas));
  switch (parse_result) {
    case kParsedRGBA:
    case kParsedSystemColor:
      return true;
    case kParsedCurrentColor:
      parsed_color = canvas ? CurrentColor(canvas) : Color::kBlack;
      return true;
    case kParseFailed:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

CanvasStyle::CanvasStyle(RGBA32 rgba) : type_(kColorRGBA), rgba_(rgba) {}

CanvasStyle::CanvasStyle(CanvasGradient* gradient)
    : type_(kGradient), gradient_(gradient) {}

CanvasStyle::CanvasStyle(CanvasPattern* pattern)
    : type_(kImagePattern), pattern_(pattern) {}

CanvasStyle* CanvasStyle::CreateFromGradient(CanvasGradient* gradient) {
  DCHECK(gradient);
  return MakeGarbageCollected<CanvasStyle>(gradient);
}

CanvasStyle* CanvasStyle::CreateFromPattern(CanvasPattern* pattern) {
  DCHECK(pattern);
  return MakeGarbageCollected<CanvasStyle>(pattern);
}

void CanvasStyle::ApplyToFlags(PaintFlags& flags) const {
  switch (type_) {
    case kColorRGBA:
      flags.setShader(nullptr);
      break;
    case kGradient:
      GetCanvasGradient()->GetGradient()->ApplyToFlags(flags, SkMatrix::I());
      break;
    case kImagePattern:
      GetCanvasPattern()->GetPattern()->ApplyToFlags(
          flags, AffineTransformToSkMatrix(GetCanvasPattern()->GetTransform()));
      break;
    default:
      NOTREACHED();
  }
}

RGBA32 CanvasStyle::PaintColor() const {
  if (type_ == kColorRGBA)
    return rgba_;
  DCHECK(type_ == kGradient || type_ == kImagePattern);
  return Color::kBlack;
}

void CanvasStyle::Trace(blink::Visitor* visitor) {
  visitor->Trace(gradient_);
  visitor->Trace(pattern_);
}

}  // namespace blink
