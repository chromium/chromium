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
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

enum CSSColorType { kInvalid, kInvalidOrNamedColor, kRGB, kHSL, kHWB };

CSSRGB* CSSColorValue::toRGB() const {
  return MakeGarbageCollected<CSSRGB>(ToColor());
}

CSSHSL* CSSColorValue::toHSL() const {
  return MakeGarbageCollected<CSSHSL>(ToColor());
}

CSSHWB* CSSColorValue::toHWB() const {
  return MakeGarbageCollected<CSSHWB>(ToColor());
}

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

static CSSColorType DetermineColorType(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kFunctionToken) {
    switch (stream.Peek().FunctionId()) {
      case CSSValueID::kRgb:
      case CSSValueID::kRgba:
        return CSSColorType::kRGB;
      case CSSValueID::kHsl:
      case CSSValueID::kHsla:
        return CSSColorType::kHSL;
      case CSSValueID::kHwb:
        return CSSColorType::kHWB;
      default:
        return CSSColorType::kInvalid;
    }
  } else if (stream.Peek().GetType() == kHashToken) {
    return CSSColorType::kRGB;
  }
  return CSSColorType::kInvalidOrNamedColor;
}

static CSSRGB* CreateCSSRGBByNumbers(int red, int green, int blue, int alpha) {
  return MakeGarbageCollected<CSSRGB>(
      CSSNumericValue::FromNumberish(MakeGarbageCollected<V8CSSNumberish>(red)),
      CSSNumericValue::FromNumberish(
          MakeGarbageCollected<V8CSSNumberish>(green)),
      CSSNumericValue::FromNumberish(
          MakeGarbageCollected<V8CSSNumberish>(blue)),
      CSSNumericValue::FromPercentish(
          MakeGarbageCollected<V8CSSNumberish>(alpha / 255.0)));
}

V8UnionCSSColorValueOrCSSStyleValue* CSSColorValue::parse(
    const ExecutionContext* execution_context,
    const String& css_text,
    ExceptionState& exception_state) {
  CSSParserTokenStream stream(css_text);
  stream.ConsumeWhitespace();

  const CSSColorType color_type = DetermineColorType(stream);

  // Validate it is not color function before parsing execution
  if (color_type == CSSColorType::kInvalid) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid color expression");
    return nullptr;
  }

  const CSSValue* parsed_value = css_parsing_utils::ConsumeColor(
      stream, *MakeGarbageCollected<CSSParserContext>(*execution_context));
  stream.ConsumeWhitespace();

  if (!parsed_value) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid color expression");
    return nullptr;
  }

  if (parsed_value->IsColorValue()) {
    const cssvalue::CSSColor* result = To<cssvalue::CSSColor>(parsed_value);
    switch (color_type) {
      case CSSColorType::kRGB:
        return MakeGarbageCollected<V8UnionCSSColorValueOrCSSStyleValue>(
            CreateCSSRGBByNumbers(
                result->Value().Red(), result->Value().Green(),
                result->Value().Blue(), result->Value().AlphaAsInteger()));
      case CSSColorType::kHSL:
        return MakeGarbageCollected<V8UnionCSSColorValueOrCSSStyleValue>(
            MakeGarbageCollected<CSSHSL>(result->Value()));
      case CSSColorType::kHWB:
        return MakeGarbageCollected<V8UnionCSSColorValueOrCSSStyleValue>(
            MakeGarbageCollected<CSSHWB>(result->Value()));
      default:
        break;
    }
  }

  DCHECK(parsed_value->IsIdentifierValue());

  const char* value_name =
      getValueName(To<CSSIdentifierValue>(parsed_value)->GetValueID());
  if (const NamedColor* named_color =
          FindColor(value_name, static_cast<wtf_size_t>(strlen(value_name)))) {
    Color color = Color::FromRGBA32(named_color->argb_value);

    return MakeGarbageCollected<V8UnionCSSColorValueOrCSSStyleValue>(
        CreateCSSRGBByNumbers(color.Red(), color.Green(), color.Blue(),
                              color.AlphaAsInteger()));
  }

  return MakeGarbageCollected<V8UnionCSSColorValueOrCSSStyleValue>(
      CSSKeywordValue::Create(css_text));
}

}  // namespace blink
