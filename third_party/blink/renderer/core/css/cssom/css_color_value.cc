// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_color_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_csscolorvalue_cssstylevalue.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_hsl.h"
#include "third_party/blink/renderer/core/css/cssom/css_hwb.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_rgb.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_types.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

const CSSValue* CSSColorValue::ToCSSValue() const {
  return cssvalue::CSSColor::Create(ToColor());
}

CSSNumericValue* CSSColorValue::ToNumberOrPercentage(
    const V8CSSNumberish* input) {
  CSSNumericValue* value = CSSNumericValue::FromPercentish(input);
  DCHECK(value);
  if (!CSSOMTypes::IsCSSStyleValueNumber(*value) &&
      !CSSOMTypes::IsCSSStyleValuePercentage(*value)) {
    return nullptr;
  }

  return value;
}

CSSNumericValue* CSSColorValue::ToPercentage(const V8CSSNumberish* input) {
  CSSNumericValue* value = CSSNumericValue::FromPercentish(input);
  DCHECK(value);
  if (!CSSOMTypes::IsCSSStyleValuePercentage(*value)) {
    return nullptr;
  }

  return value;
}

float CSSColorValue::ComponentToColorInput(CSSNumericValue* input) {
  if (CSSOMTypes::IsCSSStyleValuePercentage(*input)) {
    return input->to(CSSPrimitiveValue::UnitType::kPercentage)->value() / 100;
  }
  return input->to(CSSPrimitiveValue::UnitType::kNumber)->value();
}

V8UnionCSSColorValueOrCSSStyleValue* CSSColorValue::parse(
    const ExecutionContext* execution_context,
    const String& css_text,
    ExceptionState& exception_state) {
  CSSParserTokenStream stream(css_text);
  stream.ConsumeWhitespace();

  const CSSValue* parsed_value = css_parsing_utils::ConsumeColor(
      stream, *MakeGarbageCollected<CSSParserContext>(*execution_context));
  stream.ConsumeWhitespace();

  if (!parsed_value) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid color expression");
    return nullptr;
  }

  if (const auto* css_color = DynamicTo<cssvalue::CSSColor>(*parsed_value)) {
    const Color& color = css_color->Value();
    switch (color.GetColorSpace()) {
      case Color::ColorSpace::kSRGB:
      case Color::ColorSpace::kSRGBLegacy:
        return MakeGarbageCollected<V8UnionCSSColorValueOrCSSStyleValue>(
            MakeGarbageCollected<CSSRGB>(color, color.GetColorSpace()));
      case Color::ColorSpace::kHSL:
        return MakeGarbageCollected<V8UnionCSSColorValueOrCSSStyleValue>(
            MakeGarbageCollected<CSSHSL>(color));
      case Color::ColorSpace::kHWB:
        return MakeGarbageCollected<V8UnionCSSColorValueOrCSSStyleValue>(
            MakeGarbageCollected<CSSHWB>(color));
      default:
        break;
    }
  }

  if (const auto* css_ident = DynamicTo<CSSIdentifierValue>(*parsed_value)) {
    const CSSValueID value_id = css_ident->GetValueID();
    std::string_view value_name = GetCSSValueName(value_id);
    if (const NamedColor* named_color = FindColor(value_name)) {
      const Color color = Color::FromRGBA32(named_color->argb_value);
      return MakeGarbageCollected<V8UnionCSSColorValueOrCSSStyleValue>(
          MakeGarbageCollected<CSSRGB>(color, Color::ColorSpace::kSRGBLegacy));
    }
    return MakeGarbageCollected<V8UnionCSSColorValueOrCSSStyleValue>(
        MakeGarbageCollected<CSSKeywordValue>(value_id));
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                    "Invalid color expression");
  return nullptr;
}

}  // namespace blink
