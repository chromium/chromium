// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_definition.h"

#include <utility>

#include "third_party/blink/renderer/core/css/css_attr_value_tainting.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_component.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

namespace blink {
namespace {

const CSSValue* ConsumeSingleTypeInternal(const CSSSyntaxComponent& syntax,
                                          CSSParserTokenStream& stream,
                                          const CSSParserContext& context) {
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
          stream, context, CSSPrimitiveValue::ValueRange::kAll,
          css_parsing_utils::UnitlessQuirk::kForbid, kCSSAnchorQueryTypesAll);
    }
    case CSSSyntaxType::kColor: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      return css_parsing_utils::ConsumeColor(stream, context);
    }
    case CSSSyntaxType::kImage:
      return css_parsing_utils::ConsumeImage(stream, context);
    case CSSSyntaxType::kUrl:
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
      DCHECK(RuntimeEnabledFeatures::CSSAtPropertyStringSyntaxEnabled());
      return css_parsing_utils::ConsumeString(stream);
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

const CSSValue* TaintedCopyIfNeeded(const CSSValue* value) {
  if (const auto* v = DynamicTo<CSSStringValue>(value)) {
    return v->TaintedCopy();
  }
  // Only needed for CSSStringValue for now.
  return value;
}

const CSSValue* ConsumeSingleType(const CSSSyntaxComponent& syntax,
                                  CSSParserTokenStream& stream,
                                  const CSSParserContext& context) {
  wtf_size_t offset_before = stream.Offset();
  const CSSValue* value = ConsumeSingleTypeInternal(syntax, stream, context);
  if (value) {
    stream.EnsureLookAhead();
    wtf_size_t offset_after = stream.LookAheadOffset();
    if (IsAttrTainted(stream.StringRangeAt(
            offset_before, /* length */ offset_after - offset_before))) {
      value = TaintedCopyIfNeeded(value);
    }
  }
  return value;
}
const CSSValue* ConsumeSyntaxComponent(const CSSSyntaxComponent& syntax,
                                       CSSParserTokenStream& stream,
                                       const CSSParserContext& context) {
  // CSS-wide keywords are already handled by the CSSPropertyParser
  if (syntax.GetRepeat() == CSSSyntaxRepeat::kSpaceSeparated) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    while (!stream.AtEnd()) {
      const CSSValue* value = ConsumeSingleType(syntax, stream, context);
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
      const CSSValue* value = ConsumeSingleType(syntax, stream, context);
      if (!value) {
        return nullptr;
      }
      list->Append(*value);
    } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream));
    return list->length() && stream.AtEnd() ? list : nullptr;
  }
  const CSSValue* result = ConsumeSingleType(syntax, stream, context);
  if (!stream.AtEnd()) {
    return nullptr;
  }
  return result;
}

}  // namespace

const CSSValue* CSSSyntaxDefinition::Parse(StringView text,
                                           const CSSParserContext& context,
                                           bool is_animation_tainted) const {
  if (IsUniversal()) {
    return CSSVariableParser::ParseUniversalSyntaxValue(text, context,
                                                        is_animation_tainted);
  }
  for (const CSSSyntaxComponent& component : syntax_components_) {
    CSSParserTokenStream stream(text);
    stream.ConsumeWhitespace();
    if (const CSSValue* result =
            ConsumeSyntaxComponent(component, stream, context)) {
      return result;
    }
  }
  return nullptr;
}

CSSSyntaxDefinition CSSSyntaxDefinition::IsolatedCopy() const {
  Vector<CSSSyntaxComponent> syntax_components_copy;
  syntax_components_copy.reserve(syntax_components_.size());
  for (const auto& syntax_component : syntax_components_) {
    syntax_components_copy.push_back(CSSSyntaxComponent(
        syntax_component.GetType(), syntax_component.GetString(),
        syntax_component.GetRepeat()));
  }
  return CSSSyntaxDefinition(std::move(syntax_components_copy), original_text_);
}

CSSSyntaxDefinition::CSSSyntaxDefinition(Vector<CSSSyntaxComponent> components,
                                         const String& original_text)
    : syntax_components_(std::move(components)), original_text_(original_text) {
  DCHECK(syntax_components_.size());
}

CSSSyntaxDefinition CSSSyntaxDefinition::CreateUniversal() {
  Vector<CSSSyntaxComponent> components;
  components.push_back(CSSSyntaxComponent(
      CSSSyntaxType::kTokenStream, g_empty_string, CSSSyntaxRepeat::kNone));
  return CSSSyntaxDefinition(std::move(components), {});
}

String CSSSyntaxDefinition::ToString() const {
  return IsUniversal() ? String("*") : original_text_;
}

}  // namespace blink
