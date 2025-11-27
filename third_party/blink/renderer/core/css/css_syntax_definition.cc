// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_definition.h"

#include <optional>
#include <utility>

#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_component.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_save_point.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

namespace blink {
namespace {

bool ConsumeSyntaxCombinator(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kDelimiterToken &&
      stream.Peek().Delimiter() == '|') {
    stream.ConsumeIncludingWhitespace();
    return true;
  }
  return false;
}

CSSSyntaxRepeat ConsumeSyntaxMultiplier(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kDelimiterToken &&
      stream.Peek().Delimiter() == '#') {
    stream.ConsumeIncludingWhitespace();
    return CSSSyntaxRepeat::kCommaSeparated;
  }
  if (stream.Peek().GetType() == kDelimiterToken &&
      stream.Peek().Delimiter() == '+') {
    stream.ConsumeIncludingWhitespace();
    return CSSSyntaxRepeat::kSpaceSeparated;
  }
  return CSSSyntaxRepeat::kNone;
}

std::optional<CSSSyntaxType> ConsumeTypeName(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kIdentToken) {
    return std::nullopt;
  }
  if (stream.Peek().Value() == "angle") {
    stream.Consume();
    return CSSSyntaxType::kAngle;
  }
  if (stream.Peek().Value() == "color") {
    stream.Consume();
    return CSSSyntaxType::kColor;
  }
  if (stream.Peek().Value() == "custom-ident") {
    stream.Consume();
    return CSSSyntaxType::kCustomIdent;
  }
  if (stream.Peek().Value() == "image") {
    stream.Consume();
    return CSSSyntaxType::kImage;
  }
  if (stream.Peek().Value() == "integer") {
    stream.Consume();
    return CSSSyntaxType::kInteger;
  }
  if (stream.Peek().Value() == "length") {
    stream.Consume();
    return CSSSyntaxType::kLength;
  }
  if (stream.Peek().Value() == "length-percentage") {
    stream.Consume();
    return CSSSyntaxType::kLengthPercentage;
  }
  if (stream.Peek().Value() == "number") {
    stream.Consume();
    return CSSSyntaxType::kNumber;
  }
  if (stream.Peek().Value() == "percentage") {
    stream.Consume();
    return CSSSyntaxType::kPercentage;
  }
  if (stream.Peek().Value() == "resolution") {
    stream.Consume();
    return CSSSyntaxType::kResolution;
  }
  if (stream.Peek().Value() == "string") {
    stream.Consume();
    return CSSSyntaxType::kString;
  }
  if (stream.Peek().Value() == "time") {
    stream.Consume();
    return CSSSyntaxType::kTime;
  }
  if (stream.Peek().Value() == "url") {
    stream.Consume();
    return CSSSyntaxType::kUrl;
  }
  if (stream.Peek().Value() == "transform-function") {
    stream.Consume();
    return CSSSyntaxType::kTransformFunction;
  }
  if (stream.Peek().Value() == "transform-list") {
    stream.Consume();
    return CSSSyntaxType::kTransformList;
  }
  return std::nullopt;
}

std::optional<std::tuple<CSSSyntaxType, String>> ConsumeSyntaxSingleComponent(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kDelimiterToken &&
      stream.Peek().Delimiter() == '<') {
    CSSParserSavePoint save_point(stream);
    stream.Consume();
    std::optional<CSSSyntaxType> syntax_type = ConsumeTypeName(stream);
    if (!syntax_type.has_value()) {
      return std::nullopt;
    }
    if (stream.Peek().GetType() != kDelimiterToken ||
        stream.Peek().Delimiter() != '>') {
      return std::nullopt;
    }
    stream.Consume();
    save_point.Release();
    return std::make_tuple(*syntax_type, String());
  }
  CSSParserToken peek = stream.Peek();
  if (peek.GetType() != kIdentToken) {
    return std::nullopt;
  }
  if (css_parsing_utils::IsCSSWideKeyword(peek.Value()) ||
      css_parsing_utils::IsDefaultKeyword(peek.Value())) {
    return std::nullopt;
  }
  return std::make_tuple(CSSSyntaxType::kIdent,
                         stream.Consume().Value().ToString());
}

std::optional<CSSSyntaxComponent> ConsumeSyntaxComponent(
    CSSParserTokenStream& stream) {
  stream.EnsureLookAhead();
  CSSParserSavePoint save_point(stream);

  std::optional<std::tuple<CSSSyntaxType, String>> css_syntax_type_ident =
      ConsumeSyntaxSingleComponent(stream);
  if (!css_syntax_type_ident.has_value()) {
    return std::nullopt;
  }
  CSSSyntaxType syntax_type;
  String ident;
  std::tie(syntax_type, ident) = *css_syntax_type_ident;
  CSSSyntaxRepeat repeat = ConsumeSyntaxMultiplier(stream);
  stream.ConsumeWhitespace();
  if (syntax_type == CSSSyntaxType::kTransformList &&
      repeat != CSSSyntaxRepeat::kNone) {
    // <transform-list> may not be followed by a <syntax-multiplier>.
    // https://drafts.csswg.org/css-values-5/#css-syntax
    return std::nullopt;
  }
  save_point.Release();
  return CSSSyntaxComponent(syntax_type, ident, repeat);
}

const CSSValue* ConsumeSingleType(const CSSSyntaxComponent& syntax,
                                  CSSParserTokenStream& stream,
                                  const CSSParserContext& context,
                                  bool is_attr_tainted) {
  switch (syntax.GetType()) {
    case CSSSyntaxType::kIdent:
      if (stream.Peek().GetType() == kIdentToken &&
          stream.Peek().Value() == syntax.GetString()) {
        stream.ConsumeIncludingWhitespace();
        return MakeGarbageCollected<CSSCustomIdentValue>(
            AtomicString(syntax.GetString()));
      }
      return nullptr;
    case CSSSyntaxType::kLength: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      return css_parsing_utils::ConsumeLength(
          stream, context, CSSPrimitiveValue::ValueRange::kAll);
    }
    case CSSSyntaxType::kNumber:
      return css_parsing_utils::ConsumeNumber(
          stream, context, CSSPrimitiveValue::ValueRange::kAll);
    case CSSSyntaxType::kPercentage:
      return css_parsing_utils::ConsumePercent(
          stream, context, CSSPrimitiveValue::ValueRange::kAll);
    case CSSSyntaxType::kLengthPercentage: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      return css_parsing_utils::ConsumeLengthOrPercent(
          stream, context, CSSPrimitiveValue::ValueRange::kAll);
    }
    case CSSSyntaxType::kColor: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      return css_parsing_utils::ConsumeColor(stream, context);
    }
    case CSSSyntaxType::kImage:
      return css_parsing_utils::ConsumeImage(stream, context);
    case CSSSyntaxType::kUrl:
      if (is_attr_tainted) {
        return nullptr;
      }
      return css_parsing_utils::ConsumeUrl(stream, context);
    case CSSSyntaxType::kInteger:
      return css_parsing_utils::ConsumeIntegerOrNumberCalc(stream, context);
    case CSSSyntaxType::kAngle:
      return css_parsing_utils::ConsumeAngle(stream, context,
                                             std::optional<WebFeature>());
    case CSSSyntaxType::kTime:
      return css_parsing_utils::ConsumeTime(
          stream, context, CSSPrimitiveValue::ValueRange::kAll);
    case CSSSyntaxType::kResolution:
      return css_parsing_utils::ConsumeResolution(stream, context);
    case CSSSyntaxType::kTransformFunction:
      return css_parsing_utils::ConsumeTransformValue(stream, context);
    case CSSSyntaxType::kTransformList:
      return css_parsing_utils::ConsumeTransformList(stream, context);
    case CSSSyntaxType::kCustomIdent:
      return css_parsing_utils::ConsumeCustomIdent(stream, context);
    case CSSSyntaxType::kString:
      return css_parsing_utils::ConsumeString(stream);
    default:
      NOTREACHED();
  }
}

const CSSValue* ConsumeSyntaxComponent(const CSSSyntaxComponent& syntax,
                                       CSSParserTokenStream& stream,
                                       const CSSParserContext& context,
                                       bool is_attr_tainted) {
  // CSS-wide keywords are already handled by the CSSPropertyParser
  if (syntax.GetRepeat() == CSSSyntaxRepeat::kSpaceSeparated) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    while (!stream.AtEnd()) {
      const CSSValue* value =
          ConsumeSingleType(syntax, stream, context, is_attr_tainted);
      if (!value) {
        return nullptr;
      }
      list->Append(*value);
    }
    return list->length() ? list : nullptr;
  }
  if (syntax.GetRepeat() == CSSSyntaxRepeat::kCommaSeparated) {
    CSSValueList* list = CSSValueList::CreateCommaSeparated();
    do {
      const CSSValue* value =
          ConsumeSingleType(syntax, stream, context, is_attr_tainted);
      if (!value) {
        return nullptr;
      }
      list->Append(*value);
    } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream));
    return list->length() && stream.AtEnd() ? list : nullptr;
  }
  const CSSValue* result =
      ConsumeSingleType(syntax, stream, context, is_attr_tainted);
  if (!stream.AtEnd()) {
    return nullptr;
  }
  return result;
}

}  // namespace

std::optional<CSSSyntaxDefinition> CSSSyntaxDefinition::Consume(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kDelimiterToken &&
      stream.Peek().Delimiter() == '*') {
    stream.ConsumeIncludingWhitespace();
    return CSSSyntaxDefinition::CreateUniversal();
  }

  Vector<CSSSyntaxComponent> syntax_components;
  CSSParserSavePoint save_point(stream);
  do {
    std::optional<CSSSyntaxComponent> syntax_component =
        ConsumeSyntaxComponent(stream);
    if (!syntax_component.has_value()) {
      return std::nullopt;
    }
    syntax_components.emplace_back(*syntax_component);
  } while (ConsumeSyntaxCombinator(stream));

  save_point.Release();
  return CSSSyntaxDefinition(std::move(syntax_components));
}

std::optional<CSSSyntaxDefinition> CSSSyntaxDefinition::ConsumeComponent(
    CSSParserTokenStream& stream) {
  if (std::optional<CSSSyntaxComponent> syntax_component =
          ConsumeSyntaxComponent(stream)) {
    Vector<CSSSyntaxComponent> syntax_components(1u, syntax_component.value());
    return CSSSyntaxDefinition(std::move(syntax_components));
  }
  return std::nullopt;
}

const CSSValue* CSSSyntaxDefinition::Parse(StringView text,
                                           const CSSParserContext& context,
                                           bool is_animation_tainted,
                                           bool is_attr_tainted) const {
  if (IsUniversal()) {
    return CSSVariableParser::ParseUniversalSyntaxValue(text, context,
                                                        is_animation_tainted);
  }
  for (const CSSSyntaxComponent& component : syntax_components_) {
    CSSParserTokenStream stream(text);
    stream.ConsumeWhitespace();
    if (const CSSValue* result = ConsumeSyntaxComponent(
            component, stream, context, is_attr_tainted)) {
      return result;
    }
  }
  return nullptr;
}

CSSSyntaxDefinition::CSSSyntaxDefinition(Vector<CSSSyntaxComponent> components)
    : syntax_components_(std::move(components)) {
  DCHECK(syntax_components_.size());
}

CSSSyntaxDefinition CSSSyntaxDefinition::CreateUniversal() {
  Vector<CSSSyntaxComponent> components;
  components.push_back(CSSSyntaxComponent(
      CSSSyntaxType::kTokenStream, g_empty_string, CSSSyntaxRepeat::kNone));
  return CSSSyntaxDefinition(std::move(components));
}

String CSSSyntaxDefinition::ToString() const {
  if (IsUniversal()) {
    return String("*");
  }
  StringBuilder builder;
  builder.AppendRange(syntax_components_, " | ", [](const auto& component) {
    return component.ToString();
  });
  return builder.ReleaseString();
}

CSSSyntaxDefinition CSSSyntaxDefinition::CreateNumericSyntax() {
  CSSParserTokenStream stream(
      "<number> | <length> | <percentage> | <angle> | <time> | <resolution>");
  std::optional<CSSSyntaxDefinition> syntax_definition =
      CSSSyntaxDefinition::Consume(stream);
  DCHECK(syntax_definition.has_value());
  return *syntax_definition;
}

}  // namespace blink
