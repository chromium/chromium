// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"

#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_unicode_range_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/hash_tools.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/shorthand.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using css_property_parser_helpers::ConsumeIdent;
using css_property_parser_helpers::IsImplicitProperty;
using css_property_parser_helpers::ParseLonghand;

class CSSIdentifierValue;

namespace {

const CSSValue* MaybeConsumeCSSWideKeyword(CSSParserTokenRange& range) {
  CSSParserTokenRange local_range = range;

  CSSValueID id = local_range.ConsumeIncludingWhitespace().Id();
  if (!local_range.AtEnd())
    return nullptr;

  const CSSValue* value = nullptr;
  if (id == CSSValueID::kInitial)
    value = CSSInitialValue::Create();
  if (id == CSSValueID::kInherit)
    value = CSSInheritedValue::Create();
  if (id == CSSValueID::kUnset)
    value = cssvalue::CSSUnsetValue::Create();

  if (value)
    range = local_range;

  return value;
}

}  // namespace

CSSPropertyParser::CSSPropertyParser(
    const CSSParserTokenRange& range,
    const CSSParserContext* context,
    HeapVector<CSSPropertyValue, 256>* parsed_properties)
    : range_(range), context_(context), parsed_properties_(parsed_properties) {
  range_.ConsumeWhitespace();
}

bool CSSPropertyParser::ParseValue(
    CSSPropertyID unresolved_property,
    bool important,
    const CSSParserTokenRange& range,
    const CSSParserContext* context,
    HeapVector<CSSPropertyValue, 256>& parsed_properties,
    StyleRule::RuleType rule_type) {
  int parsed_properties_size = parsed_properties.size();

  CSSPropertyParser parser(range, context, &parsed_properties);
  CSSPropertyID resolved_property = resolveCSSPropertyID(unresolved_property);
  bool parse_success;
  if (rule_type == StyleRule::kViewport) {
    parse_success =
        (RuntimeEnabledFeatures::CSSViewportEnabled() ||
         IsUASheetBehavior(context->Mode())) &&
        parser.ParseViewportDescriptor(resolved_property, important);
  } else if (rule_type == StyleRule::kFontFace) {
    parse_success = parser.ParseFontFaceDescriptor(resolved_property);
  } else {
    parse_success = parser.ParseValueStart(unresolved_property, important);
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
                                        bool important) {
  if (ConsumeCSSWideKeyword(unresolved_property, important))
    return true;

  CSSParserTokenRange original_range = range_;
  CSSPropertyID property_id = resolveCSSPropertyID(unresolved_property);
  const CSSProperty& property = CSSProperty::Get(property_id);
  // If a CSSPropertyID is only a known descriptor (@fontface, @viewport), not a
  // style property, it will not be a valid declaration.
  if (!property.IsProperty())
    return false;
  bool is_shorthand = property.IsShorthand();
  DCHECK(context_);
  if (is_shorthand) {
    const auto local_context =
        CSSParserLocalContext()
            .WithAliasParsing(isPropertyAlias(unresolved_property))
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
        CSSVariableData::Create(original_range, is_animation_tainted, true,
                                context_->BaseURL(), context_->Charset()),
        *context_);

    if (is_shorthand) {
      const cssvalue::CSSPendingSubstitutionValue& pending_value =
          *cssvalue::CSSPendingSubstitutionValue::Create(property_id, variable);
      css_property_parser_helpers::AddExpandedPropertyForValue(
          property_id, pending_value, important, *parsed_properties_);
    } else {
      AddProperty(property_id, CSSPropertyID::kInvalid, *variable, important,
                  IsImplicitProperty::kNotImplicit, *parsed_properties_);
    }
    return true;
  }

  return false;
}

static inline bool IsExposedInMode(const CSSProperty& property,
                                   CSSParserMode mode) {
  return mode == kUASheetMode ? property.IsUAExposed()
                              : property.IsWebExposed();
}

template <typename CharacterType>
static CSSPropertyID UnresolvedCSSPropertyID(const CharacterType* property_name,
                                             unsigned length,
                                             CSSParserMode mode) {
  if (length == 0)
    return CSSPropertyID::kInvalid;
  if (length >= 2 && property_name[0] == '-' && property_name[1] == '-')
    return CSSPropertyID::kVariable;
  if (length > maxCSSPropertyNameLength)
    return CSSPropertyID::kInvalid;

  char buffer[maxCSSPropertyNameLength + 1];  // 1 for null character

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
  const CSSProperty& property =
      CSSProperty::Get(resolveCSSPropertyID(property_id));
  bool exposed = IsExposedInMode(property, mode);
  return exposed ? property_id : CSSPropertyID::kInvalid;
}

CSSPropertyID unresolvedCSSPropertyID(const String& string) {
  unsigned length = string.length();
  CSSParserMode mode = kHTMLStandardMode;
  return string.Is8Bit()
             ? UnresolvedCSSPropertyID(string.Characters8(), length, mode)
             : UnresolvedCSSPropertyID(string.Characters16(), length, mode);
}

CSSPropertyID UnresolvedCSSPropertyID(StringView string, CSSParserMode mode) {
  unsigned length = string.length();
  return string.Is8Bit()
             ? UnresolvedCSSPropertyID(string.Characters8(), length, mode)
             : UnresolvedCSSPropertyID(string.Characters16(), length, mode);
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
                                              bool important) {
  CSSParserTokenRange range_copy = range_;

  const CSSValue* value = MaybeConsumeCSSWideKeyword(range_copy);
  if (!value)
    return false;

  CSSPropertyID property = resolveCSSPropertyID(unresolved_property);
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  if (!shorthand.length()) {
    if (!CSSProperty::Get(property).IsProperty())
      return false;
    AddProperty(property, CSSPropertyID::kInvalid, *value, important,
                IsImplicitProperty::kNotImplicit, *parsed_properties_);
  } else {
    css_property_parser_helpers::AddExpandedPropertyForValue(
        property, *value, important, *parsed_properties_);
  }
  range_ = range_copy;
  return true;
}

static CSSValue* ConsumeSingleViewportDescriptor(
    CSSParserTokenRange& range,
    CSSPropertyID prop_id,
    CSSParserMode css_parser_mode) {
  CSSValueID id = range.Peek().Id();
  switch (prop_id) {
    case CSSPropertyID::kMinWidth:
    case CSSPropertyID::kMaxWidth:
    case CSSPropertyID::kMinHeight:
    case CSSPropertyID::kMaxHeight:
      if (id == CSSValueID::kAuto || id == CSSValueID::kInternalExtendToZoom)
        return ConsumeIdent(range);
      return css_property_parser_helpers::ConsumeLengthOrPercent(
          range, css_parser_mode, kValueRangeNonNegative);
    case CSSPropertyID::kMinZoom:
    case CSSPropertyID::kMaxZoom:
    case CSSPropertyID::kZoom: {
      if (id == CSSValueID::kAuto)
        return ConsumeIdent(range);
      CSSValue* parsed_value = css_property_parser_helpers::ConsumeNumber(
          range, kValueRangeNonNegative);
      if (parsed_value)
        return parsed_value;
      return css_property_parser_helpers::ConsumePercent(
          range, kValueRangeNonNegative);
    }
    case CSSPropertyID::kUserZoom:
      return ConsumeIdent<CSSValueID::kZoom, CSSValueID::kFixed>(range);
    case CSSPropertyID::kOrientation:
      return ConsumeIdent<CSSValueID::kAuto, CSSValueID::kPortrait,
                          CSSValueID::kLandscape>(range);
    case CSSPropertyID::kViewportFit:
      return ConsumeIdent<CSSValueID::kAuto, CSSValueID::kContain,
                          CSSValueID::kCover>(range);
    default:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return nullptr;
}

bool CSSPropertyParser::ParseViewportDescriptor(CSSPropertyID prop_id,
                                                bool important) {
  DCHECK(RuntimeEnabledFeatures::CSSViewportEnabled() ||
         IsUASheetBehavior(context_->Mode()));

  switch (prop_id) {
    case CSSPropertyID::kWidth: {
      CSSValue* min_width = ConsumeSingleViewportDescriptor(
          range_, CSSPropertyID::kMinWidth, context_->Mode());
      if (!min_width)
        return false;
      CSSValue* max_width = min_width;
      if (!range_.AtEnd()) {
        max_width = ConsumeSingleViewportDescriptor(
            range_, CSSPropertyID::kMaxWidth, context_->Mode());
      }
      if (!max_width || !range_.AtEnd())
        return false;
      AddProperty(CSSPropertyID::kMinWidth, CSSPropertyID::kInvalid, *min_width,
                  important, IsImplicitProperty::kNotImplicit,
                  *parsed_properties_);
      AddProperty(CSSPropertyID::kMaxWidth, CSSPropertyID::kInvalid, *max_width,
                  important, IsImplicitProperty::kNotImplicit,
                  *parsed_properties_);
      return true;
    }
    case CSSPropertyID::kHeight: {
      CSSValue* min_height = ConsumeSingleViewportDescriptor(
          range_, CSSPropertyID::kMinHeight, context_->Mode());
      if (!min_height)
        return false;
      CSSValue* max_height = min_height;
      if (!range_.AtEnd()) {
        max_height = ConsumeSingleViewportDescriptor(
            range_, CSSPropertyID::kMaxHeight, context_->Mode());
      }
      if (!max_height || !range_.AtEnd())
        return false;
      AddProperty(CSSPropertyID::kMinHeight, CSSPropertyID::kInvalid,
                  *min_height, important, IsImplicitProperty::kNotImplicit,
                  *parsed_properties_);
      AddProperty(CSSPropertyID::kMaxHeight, CSSPropertyID::kInvalid,
                  *max_height, important, IsImplicitProperty::kNotImplicit,
                  *parsed_properties_);
      return true;
    }
    case CSSPropertyID::kViewportFit:
    case CSSPropertyID::kMinWidth:
    case CSSPropertyID::kMaxWidth:
    case CSSPropertyID::kMinHeight:
    case CSSPropertyID::kMaxHeight:
    case CSSPropertyID::kMinZoom:
    case CSSPropertyID::kMaxZoom:
    case CSSPropertyID::kZoom:
    case CSSPropertyID::kUserZoom:
    case CSSPropertyID::kOrientation: {
      CSSValue* parsed_value =
          ConsumeSingleViewportDescriptor(range_, prop_id, context_->Mode());
      if (!parsed_value || !range_.AtEnd())
        return false;
      AddProperty(prop_id, CSSPropertyID::kInvalid, *parsed_value, important,
                  IsImplicitProperty::kNotImplicit, *parsed_properties_);
      return true;
    }
    default:
      return false;
  }
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
