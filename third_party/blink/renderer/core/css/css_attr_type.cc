// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_attr_type.h"

#include <optional>

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_save_point.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

std::optional<CSSPrimitiveValue::UnitType> ConsumeDimensionUnitType(
    CSSParserTokenStream& stream) {
  CSSParserTokenType type = stream.Peek().GetType();
  if (type == kDelimiterToken && stream.Peek().Delimiter() == '%') {
    stream.Consume();
    return CSSPrimitiveValue::UnitType::kPercentage;
  }
  if (type != kIdentToken) {
    return std::nullopt;
  }
  if (stream.Peek().Value() == "number") {
    stream.Consume();
    return CSSPrimitiveValue::UnitType::kNumber;
  }
  CSSPrimitiveValue::UnitType unit =
      CSSPrimitiveValue::StringToUnitType(stream.Peek().Value());
  // The <dimension-unit> production matches a literal "%"
  // character (that is, a <delim-token> with a value of "%")
  // or an ident whose value is any of the CSS units for
  // <length>, <angle>, <time>, <frequency>, or <flex> values.
  if (!CSSPrimitiveValue::IsLength(unit) && !CSSPrimitiveValue::IsAngle(unit) &&
      !CSSPrimitiveValue::IsTime(unit) &&
      !CSSPrimitiveValue::IsFrequency(unit) &&
      !CSSPrimitiveValue::IsFlex(unit) &&
      !CSSPrimitiveValue::IsPercentage(unit)) {
    return std::nullopt;
  }
  stream.Consume();
  return unit;
}

}  // namespace

std::optional<CSSAttrType> CSSAttrType::Consume(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kIdentToken &&
      stream.Peek().Value() == "raw-string") {
    stream.Consume();
    return CSSAttrType();
  }
  std::optional<CSSPrimitiveValue::UnitType> unit_type =
      ConsumeDimensionUnitType(stream);
  if (unit_type.has_value()) {
    return CSSAttrType(*unit_type);
  }
  if (stream.Peek().FunctionId() == CSSValueID::kType) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    std::optional<CSSSyntaxDefinition> syntax =
        CSSSyntaxDefinition::Consume(stream);
    // TODO(crbug.com/384959111): Consider adding support for <url>.
    if (syntax.has_value() && !syntax->ContainsUrlComponent() &&
        guard.Release()) {
      stream.ConsumeWhitespace();
      return CSSAttrType(*syntax);
    }
  }
  return std::nullopt;
}

const CSSValue* CSSAttrType::Parse(StringView text,
                                   const CSSParserContext& context) const {
  if (IsString()) {
    return MakeGarbageCollected<CSSStringValue>(text.ToString());
  }
  if (IsDimensionUnit()) {
    CSSParserTokenStream stream(text);
    CSSPrimitiveValue* number_value = css_parsing_utils::ConsumeNumber(
        stream, context, CSSPrimitiveValue::ValueRange::kAll);
    if (CSSNumericLiteralValue* literal =
            DynamicTo<CSSNumericLiteralValue>(number_value)) {
      return MakeGarbageCollected<CSSNumericLiteralValue>(
          literal->ClampedDoubleValue(), *dimension_unit_);
    }
    return nullptr;
  }
  if (IsSyntax()) {
    return syntax_->Parse(text, context, false);
  }
  return nullptr;
}

CSSAttrType CSSAttrType::GetDefaultValue() {
  return CSSAttrType();
}

}  // namespace blink
