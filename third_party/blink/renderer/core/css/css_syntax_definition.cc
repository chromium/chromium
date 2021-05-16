// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_definition.h"

#include <utility>
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_syntax_component.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

namespace blink {
namespace {

// The 'revert' and 'default' keywords are reserved.
//
// https://drafts.csswg.org/css-cascade/#default
// https://drafts.csswg.org/css-values-4/#identifier-value
//
// TODO(crbug.com/579788): Implement 'revert'.
// TODO(crbug.com/882285): Make 'default' invalid as <custom-ident>.
bool IsReservedIdentToken(const CSSParserToken& token) {
  if (token.GetType() != kIdentToken)
    return false;
  return css_parsing_utils::IsRevertKeyword(token.Value()) ||
         css_parsing_utils::IsDefaultKeyword(token.Value());
}

bool CouldConsumeReservedKeyword(CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  if (IsReservedIdentToken(range.ConsumeIncludingWhitespace()))
    return range.AtEnd();
  return false;
}

const CSSValue* ConsumeSingleType(const CSSSyntaxComponent& syntax,
                                  CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
  switch (syntax.GetType()) {
    case CSSSyntaxType::kIdent:
      if (range.Peek().GetType() == kIdentToken &&
          range.Peek().Value() == syntax.GetString()) {
        range.ConsumeIncludingWhitespace();
        return MakeGarbageCollected<CSSCustomIdentValue>(
            AtomicString(syntax.GetString()));
      }
      return nullptr;
    case CSSSyntaxType::kLength: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      return css_parsing_utils::ConsumeLength(range, context,
                                              ValueRange::kValueRangeAll);
    }
    case CSSSyntaxType::kNumber:
      return css_parsing_utils::ConsumeNumber(range, context,
                                              ValueRange::kValueRangeAll);
    case CSSSyntaxType::kPercentage:
      return css_parsing_utils::ConsumePercent(range, context,
                                               ValueRange::kValueRangeAll);
    case CSSSyntaxType::kLengthPercentage: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      return css_parsing_utils::ConsumeLengthOrPercent(
          range, context, ValueRange::kValueRangeAll);
    }
    case CSSSyntaxType::kColor: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kHTMLStandardMode);
      return css_parsing_utils::ConsumeColor(range, context);
    }
    case CSSSyntaxType::kImage:
      return css_parsing_utils::ConsumeImage(range, context);
    case CSSSyntaxType::kUrl:
      return css_parsing_utils::ConsumeUrl(range, context);
    case CSSSyntaxType::kInteger:
      return css_parsing_utils::ConsumeIntegerOrNumberCalc(range, context);
    case CSSSyntaxType::kAngle:
      return css_parsing_utils::ConsumeAngle(range, context,
                                             absl::optional<WebFeature>());
    case CSSSyntaxType::kTime:
      return css_parsing_utils::ConsumeTime(range, context,
                                            ValueRange::kValueRangeAll);
    case CSSSyntaxType::kResolution:
      return css_parsing_utils::ConsumeResolution(range);
    case CSSSyntaxType::kTransformFunction:
      return css_parsing_utils::ConsumeTransformValue(range, context);
    case CSSSyntaxType::kTransformList:
      return css_parsing_utils::ConsumeTransformList(range, context);
    case CSSSyntaxType::kCustomIdent:
      // TODO(crbug.com/579788): Implement 'revert'.
      // TODO(crbug.com/882285): Make 'default' invalid as <custom-ident>.
      if (IsReservedIdentToken(range.Peek()))
        return nullptr;
      return css_parsing_utils::ConsumeCustomIdent(range, context);
    default:
      NOTREACHED();
      return nullptr;
  }
}

const CSSValue* ConsumeSyntaxComponent(const CSSSyntaxComponent& syntax,
                                       CSSParserTokenRange range,
                                       const CSSParserContext& context) {
  // CSS-wide keywords are already handled by the CSSPropertyParser
  if (syntax.GetRepeat() == CSSSyntaxRepeat::kSpaceSeparated) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    while (!range.AtEnd()) {
      const CSSValue* value = ConsumeSingleType(syntax, range, context);
      if (!value)
        return nullptr;
      list->Append(*value);
    }
    return list->length() ? list : nullptr;
  }
  if (syntax.GetRepeat() == CSSSyntaxRepeat::kCommaSeparated) {
    CSSValueList* list = CSSValueList::CreateCommaSeparated();
    do {
      const CSSValue* value = ConsumeSingleType(syntax, range, context);
      if (!value)
        return nullptr;
      list->Append(*value);
    } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));
    return list->length() ? list : nullptr;
  }
  const CSSValue* result = ConsumeSingleType(syntax, range, context);
  if (!range.AtEnd())
    return nullptr;
  return result;
}

}  // namespace

const CSSValue* CSSSyntaxDefinition::Parse(CSSParserTokenRange range,
                                           const CSSParserContext& context,
                                           bool is_animation_tainted) const {
  if (IsUniversal()) {
    // TODO(crbug.com/579788): Implement 'revert'.
    // TODO(crbug.com/882285): Make 'default' invalid as <custom-ident>.
    if (CouldConsumeReservedKeyword(range))
      return nullptr;
    return CSSVariableParser::ParseRegisteredPropertyValue(
        range, context, false, is_animation_tainted);
  }
  range.ConsumeWhitespace();
  for (const CSSSyntaxComponent& component : syntax_components_) {
    if (const CSSValue* result =
            ConsumeSyntaxComponent(component, range, context))
      return result;
  }
  return CSSVariableParser::ParseRegisteredPropertyValue(range, context, true,
                                                         is_animation_tainted);
}

CSSSyntaxDefinition CSSSyntaxDefinition::IsolatedCopy() const {
  Vector<CSSSyntaxComponent> syntax_components_copy;
  syntax_components_copy.ReserveCapacity(syntax_components_.size());
  for (const auto& syntax_component : syntax_components_) {
    syntax_components_copy.push_back(CSSSyntaxComponent(
        syntax_component.GetType(), syntax_component.GetString().IsolatedCopy(),
        syntax_component.GetRepeat()));
  }
  return CSSSyntaxDefinition(std::move(syntax_components_copy));
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

}  // namespace blink
