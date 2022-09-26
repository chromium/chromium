// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"

#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_unicode_range_value.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/hash_tools.h"
#include "third_party/blink/renderer/core/css/known_exposed_properties.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/shorthand.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"

namespace blink {

using css_parsing_utils::ConsumeIdent;
using css_parsing_utils::IsImplicitProperty;
using css_parsing_utils::ParseLonghand;

class CSSIdentifierValue;

namespace {

const CSSValue* MaybeConsumeCSSWideKeyword(CSSParserTokenRange& range) {
  CSSParserTokenRange original_range = range;

  if (CSSValue* value = css_parsing_utils::ConsumeCSSWideKeyword(range)) {
    if (range.AtEnd())
      return value;
  }

  range = original_range;
  return nullptr;
}

bool IsPropertyAllowedInRule(const CSSProperty& property,
                             StyleRule::RuleType rule_type) {
  // This function should be called only when parsing a property. Shouldn't
  // reach here with a descriptor.
  DCHECK(property.IsProperty());
  switch (rule_type) {
    case StyleRule::kStyle:
      return true;
    case StyleRule::kKeyframe:
      return property.IsValidForKeyframe();
    case StyleRule::kTry:
      return property.IsValidForPositionFallback();
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace

CSSPropertyParser::CSSPropertyParser(
    const CSSParserTokenRange& range,
    const CSSParserContext* context,
    HeapVector<CSSPropertyValue, 64>* parsed_properties)
    : range_(range), context_(context), parsed_properties_(parsed_properties) {
  range_.ConsumeWhitespace();
}

bool CSSPropertyParser::ParseValue(
    CSSPropertyID unresolved_property,
    bool important,
    const CSSParserTokenRange& range,
    const CSSParserContext* context,
    HeapVector<CSSPropertyValue, 64>& parsed_properties,
    StyleRule::RuleType rule_type) {
  int parsed_properties_size = parsed_properties.size();

  CSSPropertyParser parser(range, context, &parsed_properties);
  CSSPropertyID resolved_property = ResolveCSSPropertyID(unresolved_property);
  bool parse_success;
  if (rule_type == StyleRule::kFontFace) {
    parse_success = parser.ParseFontFaceDescriptor(resolved_property);
  } else {
    parse_success =
        parser.ParseValueStart(unresolved_property, rule_type, important);
  }

  // This doesn't count UA style sheets
  if (parse_success)
    context->Count(context->Mode(), unresolved_property);

  if (!parse_success)
    parsed_properties.Shrink(parsed_properties_size);

  return parse_success;
}

const CSSValue* CSSPropertyParser::ParseSingleValue(
    CSSPropertyID property,
    const CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(context);
  CSSPropertyParser parser(range, context, nullptr);

  if (const CSSValue* value = MaybeConsumeCSSWideKeyword(parser.range_))
    return value;

  const CSSValue* value = ParseLonghand(property, CSSPropertyID::kInvalid,
                                        *parser.context_, parser.range_);
  if (!value || !parser.range_.AtEnd())
    return nullptr;
  return value;
}

bool CSSPropertyParser::ParseValueStart(CSSPropertyID unresolved_property,
                                        StyleRule::RuleType rule_type,
                                        bool important) {
  if (ConsumeCSSWideKeyword(unresolved_property, important, rule_type))
    return true;

  CSSParserTokenRange original_range = range_;
  CSSPropertyID property_id = ResolveCSSPropertyID(unresolved_property);
  const CSSProperty& property = CSSProperty::Get(property_id);
  // If a CSSPropertyID is only a known descriptor (@fontface, @property), not a
  // style property, it will not be a valid declaration.
  if (!property.IsProperty())
    return false;
  if (!IsPropertyAllowedInRule(property, rule_type))
    return false;
  bool is_shorthand = property.IsShorthand();
  DCHECK(context_);
  if (is_shorthand) {
    const auto local_context =
        CSSParserLocalContext()
            .WithAliasParsing(IsPropertyAlias(unresolved_property))
            .WithCurrentShorthand(property_id);
    // Variable references will fail to parse here and will fall out to the
    // variable ref parser below.
    if (To<Shorthand>(property).ParseShorthand(
            important, range_, *context_, local_context, *parsed_properties_))
      return true;
  } else {
    if (const CSSValue* parsed_value = ParseLonghand(
            unresolved_property, CSSPropertyID::kInvalid, *context_, range_)) {
      if (range_.AtEnd()) {
        AddProperty(property_id, CSSPropertyID::kInvalid, *parsed_value,
                    important, IsImplicitProperty::kNotImplicit,
                    *parsed_properties_);
        return true;
      }
    }
  }

  if (CSSVariableParser::ContainsValidVariableReferences(original_range)) {
    bool is_animation_tainted = false;
    auto* variable = MakeGarbageCollected<CSSVariableReferenceValue>(
        CSSVariableData::Create({original_range, StringView()},
                                is_animation_tainted, true, context_->BaseURL(),
                                context_->Charset()),
        *context_);

    if (is_shorthand) {
      const cssvalue::CSSPendingSubstitutionValue& pending_value =
          *MakeGarbageCollected<cssvalue::CSSPendingSubstitutionValue>(
              property_id, variable);
      css_parsing_utils::AddExpandedPropertyForValue(
          property_id, pending_value, important, *parsed_properties_);
    } else {
      AddProperty(property_id, CSSPropertyID::kInvalid, *variable, important,
                  IsImplicitProperty::kNotImplicit, *parsed_properties_);
    }
    return true;
  }

  return false;
}

static inline bool IsExposedInMode(const ExecutionContext* execution_context,
                                   const CSSProperty& property,
                                   CSSParserMode mode) {
  return mode == kUASheetMode ? property.IsUAExposed(execution_context)
                              : property.IsWebExposed(execution_context);
}

template <typename CharacterType>
static CSSPropertyID UnresolvedCSSPropertyID(
    const ExecutionContext* execution_context,
    const CharacterType* property_name,
    unsigned length,
    CSSParserMode mode) {
  if (length == 0)
    return CSSPropertyID::kInvalid;
  if (length >= 3 && property_name[0] == '-' && property_name[1] == '-')
    return CSSPropertyID::kVariable;
  if (length > kMaxCSSPropertyNameLength)
    return CSSPropertyID::kInvalid;

  char buffer[kMaxCSSPropertyNameLength + 1];  // 1 for null character

  for (unsigned i = 0; i != length; ++i) {
    CharacterType c = property_name[i];
    if (c == 0 || c >= 0x7F)
      return CSSPropertyID::kInvalid;  // illegal character
    buffer[i] = ToASCIILower(c);
  }
  buffer[length] = '\0';

  const char* name = buffer;
  const Property* hash_table_entry = FindProperty(name, length);
  if (!hash_table_entry)
    return CSSPropertyID::kInvalid;

  CSSPropertyID property_id = static_cast<CSSPropertyID>(hash_table_entry->id);
  if (kKnownExposedProperties.Has(property_id)) {
#if DCHECK_IS_ON()
    const CSSProperty& property =
        CSSProperty::Get(ResolveCSSPropertyID(property_id));
    DCHECK(IsExposedInMode(execution_context, property, mode));
#endif
  } else {
    // The property is behind a runtime flag, so we need to go ahead
    // and actually do the resolution to see if that flag is on or not.
    // This should happen only occasionally.
    const CSSProperty& property =
        CSSProperty::Get(ResolveCSSPropertyID(property_id));
    if (!IsExposedInMode(execution_context, property, mode)) {
      return CSSPropertyID::kInvalid;
    }
  }
  return property_id;
}

CSSPropertyID UnresolvedCSSPropertyID(const ExecutionContext* execution_context,
                                      const String& string) {
  return WTF::VisitCharacters(string, [&](const auto* chars, unsigned length) {
    return UnresolvedCSSPropertyID(execution_context, chars, length,
                                   kHTMLStandardMode);
  });
}

CSSPropertyID UnresolvedCSSPropertyID(const ExecutionContext* execution_context,
                                      StringView string,
                                      CSSParserMode mode) {
  return WTF::VisitCharacters(string, [&](const auto* chars, unsigned length) {
    return UnresolvedCSSPropertyID(execution_context, chars, length, mode);
  });
}

template <typename CharacterType>
static CSSValueID CssValueKeywordID(const CharacterType* value_keyword,
                                    unsigned length) {
  char buffer[maxCSSValueKeywordLength + 1];  // 1 for null character

  for (unsigned i = 0; i != length; ++i) {
    CharacterType c = value_keyword[i];
    if (c == 0 || c >= 0x7F)
      return CSSValueID::kInvalid;  // illegal character
    buffer[i] = WTF::ToASCIILower(c);
  }
  buffer[length] = '\0';

  const Value* hash_table_entry = FindValue(buffer, length);
  return hash_table_entry ? static_cast<CSSValueID>(hash_table_entry->id)
                          : CSSValueID::kInvalid;
}

CSSValueID CssValueKeywordID(StringView string) {
  unsigned length = string.length();
  if (!length)
    return CSSValueID::kInvalid;
  if (length > maxCSSValueKeywordLength)
    return CSSValueID::kInvalid;

  return string.Is8Bit() ? CssValueKeywordID(string.Characters8(), length)
                         : CssValueKeywordID(string.Characters16(), length);
}

bool CSSPropertyParser::ConsumeCSSWideKeyword(CSSPropertyID unresolved_property,
                                              bool important,
                                              StyleRule::RuleType rule_type) {
  CSSParserTokenRange range_copy = range_;

  const CSSValue* value = MaybeConsumeCSSWideKeyword(range_copy);
  if (!value)
    return false;

  if (value->IsRevertValue() || value->IsRevertLayerValue()) {
    // Declarations in @try are not cascaded and cannot be reverted.
    if (rule_type == StyleRule::kTry)
      return false;
  }

  CSSPropertyID property = ResolveCSSPropertyID(unresolved_property);
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  if (!shorthand.length()) {
    if (!CSSProperty::Get(property).IsProperty())
      return false;
    AddProperty(property, CSSPropertyID::kInvalid, *value, important,
                IsImplicitProperty::kNotImplicit, *parsed_properties_);
  } else {
    css_parsing_utils::AddExpandedPropertyForValue(property, *value, important,
                                                   *parsed_properties_);
  }
  range_ = range_copy;
  return true;
}

bool CSSPropertyParser::ParseFontFaceDescriptor(
    CSSPropertyID resolved_property) {
  // TODO(meade): This function should eventually take an AtRuleDescriptorID.
  const AtRuleDescriptorID id =
      CSSPropertyIDAsAtRuleDescriptor(resolved_property);
  if (id == AtRuleDescriptorID::Invalid)
    return false;
  CSSValue* parsed_value =
      AtRuleDescriptorParser::ParseFontFaceDescriptor(id, range_, *context_);
  if (!parsed_value)
    return false;

  AddProperty(resolved_property,
              CSSPropertyID::kInvalid /* current_shorthand */, *parsed_value,
              false /* important */, IsImplicitProperty::kNotImplicit,
              *parsed_properties_);
  return true;
}

}  // namespace blink
