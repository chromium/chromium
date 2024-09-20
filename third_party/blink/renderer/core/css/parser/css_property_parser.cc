// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"

#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_unicode_range_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/hash_tools.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_save_point.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/shorthand.h"
#include "third_party/blink/renderer/core/css/property_bitsets.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"

namespace blink {

using css_parsing_utils::ConsumeIdent;
using css_parsing_utils::IsImplicitProperty;
using css_parsing_utils::ParseLonghand;

class CSSIdentifierValue;

namespace {

bool IsPropertyAllowedInRule(const CSSProperty& property,
                             StyleRule::RuleType rule_type) {
  // This function should be called only when parsing a property. Shouldn't
  // reach here with a descriptor.
  DCHECK(property.IsProperty());
  switch (rule_type) {
    case StyleRule::kStyle:
      return true;
    case StyleRule::kPage:
    case StyleRule::kPageMargin:
      // TODO(sesse): Limit the allowed properties here.
      // https://www.w3.org/TR/css-page-3/#page-property-list
      // https://www.w3.org/TR/css-page-3/#margin-property-list
      return true;
    case StyleRule::kKeyframe:
      return property.IsValidForKeyframe();
    case StyleRule::kPositionTry:
      return property.IsValidForPositionTry();
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

}  // namespace

CSSPropertyParser::CSSPropertyParser(
    CSSParserTokenStream& stream,
    const CSSParserContext* context,
    HeapVector<CSSPropertyValue, 64>* parsed_properties)
    : stream_(stream),
      context_(context),
      parsed_properties_(parsed_properties) {
  // Strip initial whitespace/comments from stream_.
  stream_.ConsumeWhitespace();
}

bool CSSPropertyParser::ParseValue(
    CSSPropertyID unresolved_property,
    bool allow_important_annotation,
    CSSParserTokenStream& stream,
    const CSSParserContext* context,
    HeapVector<CSSPropertyValue, 64>& parsed_properties,
    StyleRule::RuleType rule_type) {
  CSSPropertyParser parser(stream, context, &parsed_properties);
  CSSPropertyID resolved_property = ResolveCSSPropertyID(unresolved_property);
  bool parse_success;
  if (rule_type == StyleRule::kFontFace) {
    parse_success = parser.ParseFontFaceDescriptor(resolved_property);
  } else {
    parse_success = parser.ParseValueStart(
        unresolved_property, allow_important_annotation, rule_type);
  }

  // This doesn't count UA style sheets
  if (parse_success) {
    context->Count(context->Mode(), unresolved_property);
  }

  return parse_success;
}

// NOTE: “stream” cannot include !important; this is for setting properties
// from CSSOM or similar.
const CSSValue* CSSPropertyParser::ParseSingleValue(
    CSSPropertyID property,
    CSSParserTokenStream& stream,
    const CSSParserContext* context) {
  DCHECK(context);
  stream.ConsumeWhitespace();

  const CSSValue* value = css_parsing_utils::ConsumeCSSWideKeyword(stream);
  if (!value) {
    value = ParseLonghand(property, CSSPropertyID::kInvalid, *context, stream);
  }
  if (!value || !stream.AtEnd()) {
    return nullptr;
  }
  return value;
}

StringView StripInitialWhitespace(StringView value) {
  wtf_size_t initial_whitespace_len = 0;
  while (initial_whitespace_len < value.length() &&
         IsHTMLSpace(value[initial_whitespace_len])) {
    ++initial_whitespace_len;
  }
  return StringView(value, initial_whitespace_len);
}

bool CSSPropertyParser::ParseValueStart(CSSPropertyID unresolved_property,
                                        bool allow_important_annotation,
                                        StyleRule::RuleType rule_type) {
  if (ParseCSSWideKeyword(unresolved_property, rule_type)) {
    return true;
  }

  CSSParserTokenStream::State savepoint = stream_.Save();

  CSSPropertyID property_id = ResolveCSSPropertyID(unresolved_property);
  const CSSProperty& property = CSSProperty::Get(property_id);
  // If a CSSPropertyID is only a known descriptor (@fontface, @property), not a
  // style property, it will not be a valid declaration.
  if (!property.IsProperty()) {
    return false;
  }
  if (!IsPropertyAllowedInRule(property, rule_type)) {
    return false;
  }
  int parsed_properties_size = parsed_properties_->size();

  bool is_shorthand = property.IsShorthand();
  DCHECK(context_);

  // NOTE: The first branch of the if here uses the tokenized form,
  // and the second uses the streaming parser. This is only allowed
  // since they start from the same place and we reset both below,
  // so they cannot go out of sync.
  if (is_shorthand) {
    const auto local_context =
        CSSParserLocalContext()
            .WithAliasParsing(IsPropertyAlias(unresolved_property))
            .WithCurrentShorthand(property_id);
    // Variable references will fail to parse here and will fall out to the
    // variable ref parser below.
    //
    // NOTE: We call ParseShorthand() with important=false, since we don't know
    // yet whether we have !important or not. We'll change the flag for all
    // added properties below (ParseShorthand() makes its own calls to
    // AddProperty(), since there may be more than one of them).
    if (To<Shorthand>(property).ParseShorthand(
            /*important=*/false, stream_, *context_, local_context,
            *parsed_properties_)) {
      bool important = css_parsing_utils::MaybeConsumeImportant(
          stream_, allow_important_annotation);
      if (stream_.AtEnd()) {
        if (important) {
          for (wtf_size_t property_idx = parsed_properties_size;
               property_idx < parsed_properties_->size(); ++property_idx) {
            (*parsed_properties_)[property_idx].SetImportant();
          }
        }
        return true;
      }
    }

    // Remove any properties that may have been added by ParseShorthand()
    // during a failing parse earlier.
    parsed_properties_->Shrink(parsed_properties_size);
  } else {
    if (const CSSValue* parsed_value = ParseLonghand(
            unresolved_property, CSSPropertyID::kInvalid, *context_, stream_)) {
      bool important = css_parsing_utils::MaybeConsumeImportant(
          stream_, allow_important_annotation);
      if (stream_.AtEnd()) {
        AddProperty(property_id, CSSPropertyID::kInvalid, *parsed_value,
                    important, IsImplicitProperty::kNotImplicit,
                    *parsed_properties_);
        return true;
      }
    }
  }

  // We did not parse properly without variable substitution,
  // so rewind the stream, and see if parsing it as something
  // containing variables will help.
  //
  // Note that if so, this needs the original text, so we need to take
  // note of the original offsets so that we can see what we tokenized.
  stream_.EnsureLookAhead();
  stream_.Restore(savepoint);

  bool important = false;
  CSSVariableData* variable_data =
      CSSVariableParser::ConsumeUnparsedDeclaration(
          stream_,
          /*allow_important_annotation=*/true,
          /*is_animation_tainted=*/false,
          /*must_contain_variable_reference=*/true,
          /*restricted_value=*/true, /*comma_ends_declaration=*/false,
          important, context_->GetExecutionContext());
  if (!variable_data) {
    return false;
  }

  auto* variable = MakeGarbageCollected<CSSUnparsedDeclarationValue>(
      variable_data, context_);
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

static inline bool IsExposedInMode(const ExecutionContext* execution_context,
                                   const CSSUnresolvedProperty& property,
                                   CSSParserMode mode) {
  return mode == kUASheetMode ? property.IsUAExposed(execution_context)
                              : property.IsWebExposed(execution_context);
}

// Take the given string, lowercase it (with possible caveats;
// see comments on the LChar version), convert it to ASCII and store it into
// the buffer together with a zero terminator. The string and zero terminator
// is assumed to fit.
//
// Returns false if the string is outside the allowed range of ASCII, so that
// it could never match any CSS properties or values.
static inline bool QuasiLowercaseIntoBuffer(const UChar* src,
                                            unsigned length,
                                            char* dst) {
  for (unsigned i = 0; i < length; ++i) {
    UChar c = src[i];
    if (c == 0 || c >= 0x7F) {  // illegal character
      return false;
    }
    dst[i] = ToASCIILower(c);
  }
  dst[length] = '\0';
  return true;
}

// Fast-path version for LChar strings. This uses the fact that all
// CSS properties and values are restricted to [a-zA-Z0-9-]. Crucially,
// this means we can do whatever we want to the six characters @[\]^_,
// because they cannot match any known values anyway. We use this to
// get a faster lowercasing than ToASCIILower() (which uses a table)
// can give us; we take anything in the range [0x40, 0x7f] and just
// set the 0x20 bit. This converts A-Z to a-z and messes up @[\]^_
// (so that they become `{|}~<DEL>, respectively). Things outside this
// range, such as 0-9 and -, are unchanged.
//
// This version never returns false, since the [0x80, 0xff] range
// won't match anything anyway (it is really only needed for UChar,
// since otherwise we could have e.g. U+0161 be downcasted to 0x61).
static inline bool QuasiLowercaseIntoBuffer(const LChar* src,
                                            unsigned length,
                                            char* dst) {
  unsigned i;
  for (i = 0; i < (length & ~3); i += 4) {
    uint32_t x;
    memcpy(&x, src + i, sizeof(x));
    x |= (x & 0x40404040) >> 1;
    memcpy(dst + i, &x, sizeof(x));
  }
  for (; i < length; ++i) {
    LChar c = src[i];
    dst[i] = c | ((c & 0x40) >> 1);
  }
  dst[length] = '\0';
  return true;
}

// The "exposed" property is different from the incoming property in the
// following cases:
//
//  - The property has an alternative property [1] which is enabled. Note that
//    alternative properties also can have alternative properties.
//  - The property is not enabled. This is represented by
//    CSSPropertyID::kInvalid.
//
// [1] See documentation near "alternative_of" in css_properties.json5.
static CSSPropertyID ExposedProperty(CSSPropertyID property_id,
                                     const ExecutionContext* execution_context,
                                     CSSParserMode mode) {
  const CSSUnresolvedProperty& property =
      CSSUnresolvedProperty::Get(property_id);
  CSSPropertyID alternative_id = property.GetAlternative();
  if (alternative_id != CSSPropertyID::kInvalid) {
    if (CSSPropertyID exposed_id =
            ExposedProperty(alternative_id, execution_context, mode);
        exposed_id != CSSPropertyID::kInvalid) {
      return exposed_id;
    }
  }
  return IsExposedInMode(execution_context, property, mode)
             ? property_id
             : CSSPropertyID::kInvalid;
}

template <typename CharacterType>
static CSSPropertyID UnresolvedCSSPropertyID(
    const ExecutionContext* execution_context,
    const CharacterType* property_name,
    unsigned length,
    CSSParserMode mode) {
  if (length == 0) {
    return CSSPropertyID::kInvalid;
  }
  if (length >= 3 && property_name[0] == '-' && property_name[1] == '-') {
    return CSSPropertyID::kVariable;
  }
  if (length > kMaxCSSPropertyNameLength) {
    return CSSPropertyID::kInvalid;
  }

  char buffer[kMaxCSSPropertyNameLength + 1];  // 1 for null character
  if (!QuasiLowercaseIntoBuffer(property_name, length, buffer)) {
    return CSSPropertyID::kInvalid;
  }

  const char* name = buffer;
  const Property* hash_table_entry = FindProperty(name, length);
#if DCHECK_IS_ON()
  // Verify that we get the same answer with standard lowercasing.
  for (unsigned i = 0; i < length; ++i) {
    buffer[i] = ToASCIILower(property_name[i]);
  }
  DCHECK_EQ(hash_table_entry, FindProperty(buffer, length));
#endif
  if (!hash_table_entry) {
    return CSSPropertyID::kInvalid;
  }

  CSSPropertyID property_id = static_cast<CSSPropertyID>(hash_table_entry->id);
  if (kKnownExposedProperties.Has(property_id)) {
    DCHECK_EQ(property_id,
              ExposedProperty(property_id, execution_context, mode));
    return property_id;
  }

  // The property is behind a runtime flag, so we need to go ahead
  // and actually do the resolution to see if that flag is on or not.
  // This should happen only occasionally.
  return ExposedProperty(property_id, execution_context, mode);
}

CSSPropertyID UnresolvedCSSPropertyID(const ExecutionContext* execution_context,
                                      StringView string,
                                      CSSParserMode mode) {
  return WTF::VisitCharacters(string, [&](auto chars) {
    return UnresolvedCSSPropertyID(execution_context, chars.data(),
                                   chars.size(), mode);
  });
}

template <typename CharacterType>
static CSSValueID CssValueKeywordID(const CharacterType* value_keyword,
                                    unsigned length) {
  char buffer[maxCSSValueKeywordLength + 1];  // 1 for null character
  if (!QuasiLowercaseIntoBuffer(value_keyword, length, buffer)) {
    return CSSValueID::kInvalid;
  }

  const Value* hash_table_entry = FindValue(buffer, length);
#if DCHECK_IS_ON()
  // Verify that we get the same answer with standard lowercasing.
  for (unsigned i = 0; i < length; ++i) {
    buffer[i] = ToASCIILower(value_keyword[i]);
  }
  DCHECK_EQ(hash_table_entry, FindValue(buffer, length));
#endif
  return hash_table_entry ? static_cast<CSSValueID>(hash_table_entry->id)
                          : CSSValueID::kInvalid;
}

CSSValueID CssValueKeywordID(StringView string) {
  unsigned length = string.length();
  if (!length) {
    return CSSValueID::kInvalid;
  }
  if (length > maxCSSValueKeywordLength) {
    return CSSValueID::kInvalid;
  }

  return string.Is8Bit() ? CssValueKeywordID(string.Characters8(), length)
                         : CssValueKeywordID(string.Characters16(), length);
}

const CSSValue* CSSPropertyParser::ConsumeCSSWideKeyword(
    CSSParserTokenStream& stream,
    bool allow_important_annotation,
    bool& important) {
  CSSParserTokenStream::State savepoint = stream.Save();

  const CSSValue* value = css_parsing_utils::ConsumeCSSWideKeyword(stream);
  if (!value) {
    // No need to Restore(), we are at the right spot anyway.
    // (We do this instead of relying on CSSParserTokenStream's
    // Restore() optimization, as this path is so hot.)
    return nullptr;
  }

  important = css_parsing_utils::MaybeConsumeImportant(
      stream, allow_important_annotation);
  if (!stream.AtEnd()) {
    stream.Restore(savepoint);
    return nullptr;
  }

  return value;
}

bool CSSPropertyParser::ParseCSSWideKeyword(CSSPropertyID unresolved_property,
                                            bool allow_important_annotation) {
  bool important;
  const CSSValue* value =
      ConsumeCSSWideKeyword(stream_, allow_important_annotation, important);
  if (!value) {
    return false;
  }

  CSSPropertyID property = ResolveCSSPropertyID(unresolved_property);
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  if (!shorthand.length()) {
    if (!CSSProperty::Get(property).IsProperty()) {
      return false;
    }
    AddProperty(property, CSSPropertyID::kInvalid, *value, important,
                IsImplicitProperty::kNotImplicit, *parsed_properties_);
  } else {
    css_parsing_utils::AddExpandedPropertyForValue(property, *value, important,
                                                   *parsed_properties_);
  }
  return true;
}

bool CSSPropertyParser::ParseFontFaceDescriptor(
    CSSPropertyID resolved_property) {
  // TODO(meade): This function should eventually take an AtRuleDescriptorID.
  const AtRuleDescriptorID id =
      CSSPropertyIDAsAtRuleDescriptor(resolved_property);
  if (id == AtRuleDescriptorID::Invalid) {
    return false;
  }

  CSSValue* parsed_value =
      AtRuleDescriptorParser::ParseFontFaceDescriptor(id, stream_, *context_);
  if (!parsed_value) {
    return false;
  }

  AddProperty(resolved_property,
              CSSPropertyID::kInvalid /* current_shorthand */, *parsed_value,
              false /* important */, IsImplicitProperty::kNotImplicit,
              *parsed_properties_);
  return true;
}

}  // namespace blink
