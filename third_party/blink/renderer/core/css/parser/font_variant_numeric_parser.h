// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_FONT_VARIANT_NUMERIC_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_FONT_VARIANT_NUMERIC_PARSER_H_

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"

namespace blink {

class FontVariantNumericParser {
  STACK_ALLOCATED();

 public:
  FontVariantNumericParser() {}

  enum class ParseResult { kConsumedValue, kDisallowedValue, kUnknownValue };

  ParseResult ConsumeNumeric(CSSParserTokenRange& range) {
    CSSValueID value_id = range.Peek().Id();
    switch (value_id) {
      case CSSValueID::kLiningNums:
      case CSSValueID::kOldstyleNums:
        if (numeric_figure_)
          return ParseResult::kDisallowedValue;
        numeric_figure_ = css_property_parser_helpers::ConsumeIdent(range);
        return ParseResult::kConsumedValue;
      case CSSValueID::kProportionalNums:
      case CSSValueID::kTabularNums:
        if (numeric_spacing_)
          return ParseResult::kDisallowedValue;
        numeric_spacing_ = css_property_parser_helpers::ConsumeIdent(range);
        return ParseResult::kConsumedValue;
      case CSSValueID::kDiagonalFractions:
      case CSSValueID::kStackedFractions:
        if (numeric_fraction_)
          return ParseResult::kDisallowedValue;
        numeric_fraction_ = css_property_parser_helpers::ConsumeIdent(range);
        return ParseResult::kConsumedValue;
      case CSSValueID::kOrdinal:
        if (ordinal_)
          return ParseResult::kDisallowedValue;
        ordinal_ = css_property_parser_helpers::ConsumeIdent(range);
        return ParseResult::kConsumedValue;
      case CSSValueID::kSlashedZero:
        if (slashed_zero_)
          return ParseResult::kDisallowedValue;
        slashed_zero_ = css_property_parser_helpers::ConsumeIdent(range);
        return ParseResult::kConsumedValue;
      default:
        return ParseResult::kUnknownValue;
    }
  }

  CSSValue* FinalizeValue() {
    CSSValueList* result = CSSValueList::CreateSpaceSeparated();
    if (numeric_figure_)
      result->Append(*numeric_figure_);
    if (numeric_spacing_)
      result->Append(*numeric_spacing_);
    if (numeric_fraction_)
      result->Append(*numeric_fraction_);
    if (ordinal_)
      result->Append(*ordinal_);
    if (slashed_zero_)
      result->Append(*slashed_zero_);
    if (result->length() > 0)
      return result;
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

 private:
  Member<CSSIdentifierValue> numeric_figure_;
  Member<CSSIdentifierValue> numeric_spacing_;
  Member<CSSIdentifierValue> numeric_fraction_;
  Member<CSSIdentifierValue> ordinal_;
  Member<CSSIdentifierValue> slashed_zero_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_FONT_VARIANT_NUMERIC_PARSER_H_
