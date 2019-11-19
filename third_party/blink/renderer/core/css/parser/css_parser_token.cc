// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"

#include <limits.h>
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// Just a helper used for Delimiter tokens.
CSSParserToken::CSSParserToken(CSSParserTokenType type, UChar c)
    : type_(type), block_type_(kNotBlock), delimiter_(c) {
  DCHECK_EQ(type_, static_cast<unsigned>(kDelimiterToken));
}

CSSParserToken::CSSParserToken(CSSParserTokenType type,
                               double numeric_value,
                               NumericValueType numeric_value_type,
                               NumericSign sign)
    : type_(type),
      block_type_(kNotBlock),
      numeric_value_type_(numeric_value_type),
      numeric_sign_(sign),
      unit_(static_cast<unsigned>(CSSPrimitiveValue::UnitType::kNumber)) {
  DCHECK_EQ(type, kNumberToken);
  numeric_value_ =
      clampTo<double>(numeric_value, -std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());
}

CSSParserToken::CSSParserToken(CSSParserTokenType type,
                               UChar32 start,
                               UChar32 end)
    : type_(kUnicodeRangeToken), block_type_(kNotBlock) {
  DCHECK_EQ(type, kUnicodeRangeToken);
  unicode_range_.start = start;
  unicode_range_.end = end;
}

CSSParserToken::CSSParserToken(HashTokenType type, StringView value)
    : type_(kHashToken), block_type_(kNotBlock), hash_token_type_(type) {
  InitValueFromStringView(value);
}

void CSSParserToken::ConvertToDimensionWithUnit(StringView unit) {
  DCHECK_EQ(type_, static_cast<unsigned>(kNumberToken));
  type_ = kDimensionToken;
  InitValueFromStringView(unit);
  unit_ = static_cast<unsigned>(CSSPrimitiveValue::StringToUnitType(unit));
}

void CSSParserToken::ConvertToPercentage() {
  DCHECK_EQ(type_, static_cast<unsigned>(kNumberToken));
  type_ = kPercentageToken;
  unit_ = static_cast<unsigned>(CSSPrimitiveValue::UnitType::kPercentage);
}

UChar CSSParserToken::Delimiter() const {
  DCHECK_EQ(type_, static_cast<unsigned>(kDelimiterToken));
  return delimiter_;
}

NumericSign CSSParserToken::GetNumericSign() const {
  // This is valid for DimensionToken and PercentageToken, but only used
  // in <an+b> parsing on NumberTokens.
  DCHECK_EQ(type_, static_cast<unsigned>(kNumberToken));
  return static_cast<NumericSign>(numeric_sign_);
}

NumericValueType CSSParserToken::GetNumericValueType() const {
  DCHECK(type_ == kNumberToken || type_ == kPercentageToken ||
         type_ == kDimensionToken);
  return static_cast<NumericValueType>(numeric_value_type_);
}

double CSSParserToken::NumericValue() const {
  DCHECK(type_ == kNumberToken || type_ == kPercentageToken ||
         type_ == kDimensionToken);
  return numeric_value_;
}

CSSPropertyID CSSParserToken::ParseAsUnresolvedCSSPropertyID(
    CSSParserMode mode) const {
  DCHECK_EQ(type_, static_cast<unsigned>(kIdentToken));
  return UnresolvedCSSPropertyID(Value(), mode);
}

AtRuleDescriptorID CSSParserToken::ParseAsAtRuleDescriptorID() const {
  DCHECK_EQ(type_, static_cast<unsigned>(kIdentToken));
  return AsAtRuleDescriptorID(Value());
}

CSSValueID CSSParserToken::Id() const {
  if (type_ != kIdentToken)
    return CSSValueID::kInvalid;
  if (id_ < 0)
    id_ = static_cast<int>(CssValueKeywordID(Value()));
  return static_cast<CSSValueID>(id_);
}

CSSValueID CSSParserToken::FunctionId() const {
  if (type_ != kFunctionToken)
    return CSSValueID::kInvalid;
  if (id_ < 0)
    id_ = static_cast<int>(CssValueKeywordID(Value()));
  return static_cast<CSSValueID>(id_);
}

bool CSSParserToken::HasStringBacking() const {
  CSSParserTokenType token_type = GetType();
  return token_type == kIdentToken || token_type == kFunctionToken ||
         token_type == kAtKeywordToken || token_type == kHashToken ||
         token_type == kUrlToken || token_type == kDimensionToken ||
         token_type == kStringToken;
}

CSSParserToken CSSParserToken::CopyWithUpdatedString(
    const StringView& string) const {
  CSSParserToken copy(*this);
  copy.InitValueFromStringView(string);
  return copy;
}

bool CSSParserToken::ValueDataCharRawEqual(const CSSParserToken& other) const {
  if (value_length_ != other.value_length_)
    return false;

  if (value_data_char_raw_ == other.value_data_char_raw_ &&
      value_is_8bit_ == other.value_is_8bit_)
    return true;

  if (value_is_8bit_) {
    return other.value_is_8bit_
               ? Equal(static_cast<const LChar*>(value_data_char_raw_),
                       static_cast<const LChar*>(other.value_data_char_raw_),
                       value_length_)
               : Equal(static_cast<const LChar*>(value_data_char_raw_),
                       static_cast<const UChar*>(other.value_data_char_raw_),
                       value_length_);
  } else {
    return other.value_is_8bit_
               ? Equal(static_cast<const UChar*>(value_data_char_raw_),
                       static_cast<const LChar*>(other.value_data_char_raw_),
                       value_length_)
               : Equal(static_cast<const UChar*>(value_data_char_raw_),
                       static_cast<const UChar*>(other.value_data_char_raw_),
                       value_length_);
  }
}

bool CSSParserToken::operator==(const CSSParserToken& other) const {
  if (type_ != other.type_)
    return false;
  switch (type_) {
    case kDelimiterToken:
      return Delimiter() == other.Delimiter();
    case kHashToken:
      if (hash_token_type_ != other.hash_token_type_)
        return false;
      FALLTHROUGH;
    case kIdentToken:
    case kFunctionToken:
    case kStringToken:
    case kUrlToken:
      return ValueDataCharRawEqual(other);
    case kDimensionToken:
      if (!ValueDataCharRawEqual(other))
        return false;
      FALLTHROUGH;
    case kNumberToken:
    case kPercentageToken:
      return numeric_sign_ == other.numeric_sign_ &&
             numeric_value_ == other.numeric_value_ &&
             numeric_value_type_ == other.numeric_value_type_;
    case kUnicodeRangeToken:
      return unicode_range_.start == other.unicode_range_.start &&
             unicode_range_.end == other.unicode_range_.end;
    default:
      return true;
  }
}

void CSSParserToken::Serialize(StringBuilder& builder) const {
  // This is currently only used for @supports CSSOM. To keep our implementation
  // simple we handle some of the edge cases incorrectly (see comments below).
  switch (GetType()) {
    case kIdentToken:
      SerializeIdentifier(Value().ToString(), builder);
      break;
    case kFunctionToken:
      SerializeIdentifier(Value().ToString(), builder);
      return builder.Append('(');
    case kAtKeywordToken:
      builder.Append('@');
      SerializeIdentifier(Value().ToString(), builder);
      break;
    case kHashToken:
      builder.Append('#');
      SerializeIdentifier(Value().ToString(), builder,
                          (GetHashTokenType() == kHashTokenUnrestricted));
      break;
    case kUrlToken:
      builder.Append("url(");
      SerializeIdentifier(Value().ToString(), builder);
      return builder.Append(')');
    case kDelimiterToken:
      if (Delimiter() == '\\')
        return builder.Append("\\\n");
      return builder.Append(Delimiter());
    case kNumberToken:
      // These won't properly preserve the NumericValueType flag
      return builder.AppendNumber(NumericValue());
    case kPercentageToken:
      builder.AppendNumber(NumericValue());
      return builder.Append('%');
    case kDimensionToken:
      // This will incorrectly serialize e.g. 4e3e2 as 4000e2
      builder.AppendNumber(NumericValue());
      SerializeIdentifier(Value().ToString(), builder);
      break;
    case kUnicodeRangeToken:
      return builder.Append(
          String::Format("U+%X-%X", UnicodeRangeStart(), UnicodeRangeEnd()));
    case kStringToken:
      return SerializeString(Value().ToString(), builder);

    case kIncludeMatchToken:
      return builder.Append("~=");
    case kDashMatchToken:
      return builder.Append("|=");
    case kPrefixMatchToken:
      return builder.Append("^=");
    case kSuffixMatchToken:
      return builder.Append("$=");
    case kSubstringMatchToken:
      return builder.Append("*=");
    case kColumnToken:
      return builder.Append("||");
    case kCDOToken:
      return builder.Append("<!--");
    case kCDCToken:
      return builder.Append("-->");
    case kBadStringToken:
      return builder.Append("'\n");
    case kBadUrlToken:
      return builder.Append("url(()");
    case kWhitespaceToken:
      return builder.Append(' ');
    case kColonToken:
      return builder.Append(':');
    case kSemicolonToken:
      return builder.Append(';');
    case kCommaToken:
      return builder.Append(',');
    case kLeftParenthesisToken:
      return builder.Append('(');
    case kRightParenthesisToken:
      return builder.Append(')');
    case kLeftBracketToken:
      return builder.Append('[');
    case kRightBracketToken:
      return builder.Append(']');
    case kLeftBraceToken:
      return builder.Append('{');
    case kRightBraceToken:
      return builder.Append('}');

    case kEOFToken:
    case kCommentToken:
      NOTREACHED();
      return;
  }
}

}  // namespace blink
