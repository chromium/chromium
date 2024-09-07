// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"

#include <limits.h>
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// Just a helper used for Delimiter tokens.
CSSParserToken::CSSParserToken(CSSParserTokenType type, UChar c)
    : type_(type),
      block_type_(kNotBlock),
      value_is_inline_(false),
      delimiter_(c) {
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
      unit_(static_cast<unsigned>(CSSPrimitiveValue::UnitType::kNumber)),
      value_is_inline_(false) {
  DCHECK_EQ(type, kNumberToken);
  numeric_value_ =
      ClampTo<double>(numeric_value, -std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());
}

CSSParserToken::CSSParserToken(CSSParserTokenType type,
                               UChar32 start,
                               UChar32 end)
    : type_(kUnicodeRangeToken),
      block_type_(kNotBlock),
      value_is_inline_(false) {
  DCHECK_EQ(type, kUnicodeRangeToken);
  unicode_range_.start = start;
  unicode_range_.end = end;
}

CSSParserToken::CSSParserToken(HashTokenType type, StringView value)
    : type_(kHashToken),
      block_type_(kNotBlock),
      value_is_inline_(false),
      hash_token_type_(type) {
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
    const ExecutionContext* execution_context,
    CSSParserMode mode) const {
  DCHECK_EQ(type_, static_cast<unsigned>(kIdentToken));
  return UnresolvedCSSPropertyID(execution_context, Value(), mode);
}

AtRuleDescriptorID CSSParserToken::ParseAsAtRuleDescriptorID() const {
  DCHECK_EQ(type_, static_cast<unsigned>(kIdentToken));
  return AsAtRuleDescriptorID(Value());
}

CSSValueID CSSParserToken::Id() const {
  if (type_ != kIdentToken) {
    return CSSValueID::kInvalid;
  }
  if (id_ < 0) {
    id_ = static_cast<int>(CssValueKeywordID(Value()));
  }
  return static_cast<CSSValueID>(id_);
}

CSSValueID CSSParserToken::FunctionId() const {
  if (type_ != kFunctionToken) {
    return CSSValueID::kInvalid;
  }
  if (id_ < 0) {
    id_ = static_cast<int>(CssValueKeywordID(Value()));
  }
  return static_cast<CSSValueID>(id_);
}

bool CSSParserToken::HasStringBacking() const {
  CSSParserTokenType token_type = GetType();
  if (value_is_inline_) {
    return false;
  }
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
  if (value_length_ != other.value_length_) {
    return false;
  }

  if (ValueDataCharRaw() == other.ValueDataCharRaw() &&
      value_is_8bit_ == other.value_is_8bit_) {
    return true;
  }

  if (value_is_8bit_) {
    return other.value_is_8bit_
               ? Equal(static_cast<const LChar*>(ValueDataCharRaw()),
                       static_cast<const LChar*>(other.ValueDataCharRaw()),
                       value_length_)
               : Equal(static_cast<const LChar*>(ValueDataCharRaw()),
                       static_cast<const UChar*>(other.ValueDataCharRaw()),
                       value_length_);
  } else {
    return other.value_is_8bit_
               ? Equal(static_cast<const UChar*>(ValueDataCharRaw()),
                       static_cast<const LChar*>(other.ValueDataCharRaw()),
                       value_length_)
               : Equal(static_cast<const UChar*>(ValueDataCharRaw()),
                       static_cast<const UChar*>(other.ValueDataCharRaw()),
                       value_length_);
  }
}

bool CSSParserToken::operator==(const CSSParserToken& other) const {
  if (type_ != other.type_) {
    return false;
  }
  switch (type_) {
    case kDelimiterToken:
      return Delimiter() == other.Delimiter();
    case kHashToken:
      if (hash_token_type_ != other.hash_token_type_) {
        return false;
      }
      [[fallthrough]];
    case kIdentToken:
    case kFunctionToken:
    case kStringToken:
    case kUrlToken:
      return ValueDataCharRawEqual(other);
    case kDimensionToken:
      if (!ValueDataCharRawEqual(other)) {
        return false;
      }
      [[fallthrough]];
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
      if (Delimiter() == '\\') {
        return builder.Append("\\\n");
      }
      return builder.Append(Delimiter());
    case kNumberToken:
      if (numeric_value_type_ == kIntegerValueType) {
        return builder.AppendNumber(ClampTo<int64_t>(NumericValue()));
      } else {
        NumberToStringBuffer buffer;
        const char* str = NumberToString(NumericValue(), buffer);
        builder.Append(str);
        // This wasn't parsed as an integer, so when we serialize it back,
        // it cannot be an integer. Otherwise, we would round-trip e.g.
        // “2.0” to “2”, which could make an invalid value suddenly valid.
        if (strchr(str, '.') == nullptr && strchr(str, 'e') == nullptr) {
          builder.Append(".0");
        }
        return;
      }
    case kPercentageToken:
      builder.AppendNumber(NumericValue());
      return builder.Append('%');
    case kDimensionToken: {
      // This will incorrectly serialize e.g. 4e3e2 as 4000e2
      NumberToStringBuffer buffer;
      const char* str = NumberToString(NumericValue(), buffer);
      builder.Append(str);
      // NOTE: We don't need the same “.0” treatment as we did for
      // kNumberToken, as there are no situations where e.g. 2deg
      // would be valid but 2.0deg not.
      SerializeIdentifier(Value().ToString(), builder);
      break;
    }
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
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

// https://www.w3.org/TR/css-syntax-3/#serialization
bool NeedsInsertedComment(const CSSParserToken& a, const CSSParserToken& b) {
  CSSParserTokenType at = a.GetType();
  CSSParserTokenType bt = b.GetType();

  // Row 1–7 of the table.
  if (at == kIdentToken || at == kAtKeywordToken || at == kHashToken ||
      at == kDimensionToken || at == kNumberToken ||
      (at == kDelimiterToken &&
       (a.Delimiter() == '#' || a.Delimiter() == '-'))) {
    if (at == kIdentToken && bt == kLeftParenthesisToken) {
      return true;
    }
    if (at == kNumberToken && bt == kDelimiterToken) {
      if (b.Delimiter() == '-') {
        return false;
      }
      if (b.Delimiter() == '%') {
        return true;
      }
    }
    return bt == kIdentToken || bt == kFunctionToken || bt == kUrlToken ||
           bt == kBadUrlToken || bt == kNumberToken || bt == kPercentageToken ||
           bt == kDimensionToken || bt == kCDCToken ||
           (bt == kDelimiterToken && b.Delimiter() == '-');
  }

  // Row 8.
  if (at == kDelimiterToken && a.Delimiter() == '@') {
    return bt == kIdentToken || bt == kFunctionToken || bt == kUrlToken ||
           bt == kBadUrlToken || bt == kCDCToken ||
           (bt == kDelimiterToken && b.Delimiter() == '-');
  }

  // Rows 9 and 10.
  if (at == kDelimiterToken && (a.Delimiter() == '.' || a.Delimiter() == '+')) {
    return bt == kNumberToken || bt == kPercentageToken ||
           bt == kDimensionToken;
  }

  // Final row (all other cases are false).
  return at == kDelimiterToken && bt == kDelimiterToken &&
         a.Delimiter() == '/' && b.Delimiter() == '*';
}

}  // namespace blink
