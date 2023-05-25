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

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/cssom/css_color_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {

static ColorParseResult ParseColor(Color& parsed_color,
                                   const String& color_string,
                                   mojom::blink::ColorScheme color_scheme) {
  if (EqualIgnoringASCIICase(color_string, "currentcolor"))
    return ColorParseResult::kCurrentColor;
  const bool kUseStrictParsing = true;
  if (CSSParser::ParseColor(parsed_color, color_string, kUseStrictParsing))
    return ColorParseResult::kColor;
  if (CSSParser::ParseSystemColor(parsed_color, color_string, color_scheme))
    return ColorParseResult::kColor;
  return ColorParseResult::kParseFailed;
}

ColorParseResult ParseCanvasColorString(const String& color_string,
                                        mojom::blink::ColorScheme color_scheme,
                                        Color& parsed_color) {
  return ParseColor(parsed_color,
                    color_string.StripWhiteSpace(IsHTMLSpace<UChar>),
                    color_scheme);
}

bool ParseCanvasColorString(const String& color_string, Color& parsed_color) {
  const ColorParseResult parse_result = ParseCanvasColorString(
      color_string, mojom::blink::ColorScheme::kLight, parsed_color);
  switch (parse_result) {
    case ColorParseResult::kColor:
      return true;
    case ColorParseResult::kCurrentColor:
      parsed_color = Color::kBlack;
      return true;
    case ColorParseResult::kParseFailed:
      return false;
  }
}

void CanvasStyle::ApplyToFlags(cc::PaintFlags& flags,
                               float global_alpha) const {
  switch (type_) {
    case kColor:
      ApplyColorToFlags(flags, global_alpha);
      break;
    case kGradient:
      GetCanvasGradient()->GetGradient()->ApplyToFlags(flags, SkMatrix::I(),
                                                       ImageDrawOptions());
      flags.setColor(SkColor4f(0.0f, 0.0f, 0.0f, global_alpha));
      break;
    case kImagePattern:
      GetCanvasPattern()->GetPattern()->ApplyToFlags(
          flags, AffineTransformToSkMatrix(GetCanvasPattern()->GetTransform()));
      flags.setColor(SkColor4f(0.0f, 0.0f, 0.0f, global_alpha));
      break;
    default:
      NOTREACHED();
  }
}

void CanvasStyle::Trace(Visitor* visitor) const {
  visitor->Trace(gradient_);
  visitor->Trace(pattern_);
}

}  // namespace blink
