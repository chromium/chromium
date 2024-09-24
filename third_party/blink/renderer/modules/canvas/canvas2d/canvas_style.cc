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

#include "base/notreached.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/dom/text_link_colors.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/platform/graphics/gradient.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/pattern.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"

namespace blink {

static ColorParseResult ParseColor(Color& parsed_color,
                                   const String& color_string,
                                   mojom::blink::ColorScheme color_scheme,
                                   const ui::ColorProvider* color_provider,
                                   bool is_in_web_app_scope) {
  if (EqualIgnoringASCIICase(color_string, "currentcolor"))
    return ColorParseResult::kCurrentColor;
  const bool kUseStrictParsing = true;
  if (CSSParser::ParseColor(parsed_color, color_string, kUseStrictParsing))
    return ColorParseResult::kColor;
  if (CSSParser::ParseSystemColor(parsed_color, color_string, color_scheme,
                                  color_provider, is_in_web_app_scope)) {
    return ColorParseResult::kColor;
  }
  const CSSValue* parsed_value = CSSParser::ParseSingleValue(
      CSSPropertyID::kColor, color_string,
      StrictCSSParserContext(SecureContextMode::kInsecureContext));
  if (parsed_value && (parsed_value->IsColorMixValue() ||
                       parsed_value->IsRelativeColorValue())) {
    static const TextLinkColors kDefaultTextLinkColors{};
    // TODO(40946458): Don't use default length resolver here!
    const ResolveColorValueContext context{
        .length_resolver = CSSToLengthConversionData(),
        .text_link_colors = kDefaultTextLinkColors,
        .used_color_scheme = color_scheme,
        .color_provider = color_provider,
        .is_in_web_app_scope = is_in_web_app_scope};
    const StyleColor style_color = ResolveColorValue(*parsed_value, context);
    parsed_color = style_color.Resolve(Color::kBlack, color_scheme);
    return ColorParseResult::kColorFunction;
  }
  return ColorParseResult::kParseFailed;
}

ColorParseResult ParseCanvasColorString(const String& color_string,
                                        mojom::blink::ColorScheme color_scheme,
                                        Color& parsed_color,
                                        const ui::ColorProvider* color_provider,
                                        bool is_in_web_app_scope) {
  return ParseColor(parsed_color,
                    color_string.StripWhiteSpace(IsHTMLSpace<UChar>),
                    color_scheme, color_provider, is_in_web_app_scope);
}

bool ParseCanvasColorString(const String& color_string, Color& parsed_color) {
  const ColorParseResult parse_result = ParseCanvasColorString(
      color_string, mojom::blink::ColorScheme::kLight, parsed_color,
      /*color_provider=*/nullptr, /*is_in_web_app_scope=*/false);
  switch (parse_result) {
    case ColorParseResult::kColor:
    case ColorParseResult::kColorFunction:
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
      NOTREACHED_IN_MIGRATION();
  }
}

void CanvasStyle::Trace(Visitor* visitor) const {
  visitor->Trace(gradient_);
  visitor->Trace(pattern_);
}

}  // namespace blink
