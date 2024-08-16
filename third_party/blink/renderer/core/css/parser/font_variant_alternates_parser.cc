// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/font_variant_alternates_parser.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_save_point.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

namespace blink {

using css_parsing_utils::ConsumeCommaSeparatedList;
using css_parsing_utils::ConsumeCustomIdent;

FontVariantAlternatesParser::FontVariantAlternatesParser() = default;

FontVariantAlternatesParser::ParseResult
FontVariantAlternatesParser::ConsumeAlternates(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  // Handled in longhand parsing imstream.
  DCHECK(stream.Peek().Id() != CSSValueID::kNormal);
  if (!ConsumeHistoricalForms(stream) && !ConsumeAlternate(stream, context)) {
    return ParseResult::kUnknownValue;
  }
  return ParseResult::kConsumedValue;
}

bool FontVariantAlternatesParser::ConsumeAlternate(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  auto peek = stream.Peek().FunctionId();
  cssvalue::CSSAlternateValue** value_to_set = nullptr;
  switch (peek) {
    case CSSValueID::kStylistic:
      if (!stylistic_) {
        value_to_set = &stylistic_;
      }
      break;
    case CSSValueID::kStyleset:
      if (!styleset_) {
        value_to_set = &styleset_;
      }
      break;
    case CSSValueID::kCharacterVariant:
      if (!character_variant_) {
        value_to_set = &character_variant_;
      }
      break;
    case CSSValueID::kSwash:
      if (!swash_) {
        value_to_set = &swash_;
      }
      break;
    case CSSValueID::kOrnaments:
      if (!ornaments_) {
        value_to_set = &ornaments_;
      }
      break;
    case CSSValueID::kAnnotation:
      if (!annotation_) {
        value_to_set = &annotation_;
      }
      break;
    default:
      break;
  }
  if (!value_to_set) {
    return false;
  }

  bool multiple_idents_allowed =
      peek == CSSValueID::kStyleset || peek == CSSValueID::kCharacterVariant;
  CSSFunctionValue* function_value =
      MakeGarbageCollected<CSSFunctionValue>(peek);
  CSSValueList* aliases;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    aliases = ConsumeCommaSeparatedList<CSSCustomIdentValue*(
        CSSParserTokenStream&, const CSSParserContext&)>(ConsumeCustomIdent,
                                                         stream, context);
    // At least one argument is required:
    // https://drafts.csswg.org/css-fonts-4/#font-variant-alternates-prop
    if (!aliases || !stream.AtEnd()) {
      return false;
    }
    if (aliases->length() > 1 && !multiple_idents_allowed) {
      return false;
    }
    guard.Release();
  }
  stream.ConsumeWhitespace();
  *value_to_set = MakeGarbageCollected<cssvalue::CSSAlternateValue>(
      *function_value, *aliases);
  return true;
}

bool FontVariantAlternatesParser::ConsumeHistoricalForms(
    CSSParserTokenStream& stream) {
  if (stream.Peek().Id() != CSSValueID::kHistoricalForms) {
    return false;
  }
  historical_forms_ =
      css_parsing_utils::ConsumeIdent<CSSValueID::kHistoricalForms>(stream);
  return true;
}

CSSValue* FontVariantAlternatesParser::FinalizeValue() {
  alternates_list_ = CSSValueList::CreateSpaceSeparated();
  if (stylistic_) {
    alternates_list_->Append(*stylistic_);
  }
  if (historical_forms_) {
    alternates_list_->Append(*historical_forms_);
  }
  if (styleset_) {
    alternates_list_->Append(*styleset_);
  }
  if (character_variant_) {
    alternates_list_->Append(*character_variant_);
  }
  if (swash_) {
    alternates_list_->Append(*swash_);
  }
  if (ornaments_) {
    alternates_list_->Append(*ornaments_);
  }
  if (annotation_) {
    alternates_list_->Append(*annotation_);
  }

  if (alternates_list_->length()) {
    return alternates_list_;
  }
  return CSSIdentifierValue::Create(CSSValueID::kNormal);
}

}  // namespace blink
