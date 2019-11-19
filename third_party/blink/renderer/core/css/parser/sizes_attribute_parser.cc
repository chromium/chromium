// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/sizes_attribute_parser.h"

#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/sizes_math_function_parser.h"
#include "third_party/blink/renderer/core/media_type_names.h"

namespace blink {

SizesAttributeParser::SizesAttributeParser(MediaValues* media_values,
                                           const String& attribute)
    : media_values_(media_values), length_(0), length_was_set_(false) {
  DCHECK(media_values_.Get());
  is_valid_ =
      Parse(CSSParserTokenRange(CSSTokenizer(attribute).TokenizeToEOF()));
}

float SizesAttributeParser::length() {
  if (is_valid_)
    return EffectiveSize();
  return EffectiveSizeDefaultValue();
}

bool SizesAttributeParser::CalculateLengthInPixels(CSSParserTokenRange range,
                                                   float& result) {
  const CSSParserToken& start_token = range.Peek();
  CSSParserTokenType type = start_token.GetType();
  if (type == kDimensionToken) {
    double length;
    if (!CSSPrimitiveValue::IsLength(start_token.GetUnitType()))
      return false;
    if ((media_values_->ComputeLength(start_token.NumericValue(),
                                      start_token.GetUnitType(), length)) &&
        (length >= 0)) {
      result = clampTo<float>(length);
      return true;
    }
  } else if (type == kFunctionToken) {
    SizesMathFunctionParser calc_parser(range, media_values_);
    if (!calc_parser.IsValid())
      return false;
    result = calc_parser.Result();
    return true;
  } else if (type == kNumberToken && !start_token.NumericValue()) {
    result = 0;
    return true;
  }

  return false;
}

bool SizesAttributeParser::MediaConditionMatches(
    const MediaQuerySet& media_condition) {
  // A Media Condition cannot have a media type other then screen.
  MediaQueryEvaluator media_query_evaluator(*media_values_);
  return media_query_evaluator.Eval(media_condition);
}

bool SizesAttributeParser::Parse(CSSParserTokenRange range) {
  // Split on a comma token and parse the result tokens as (media-condition,
  // length) pairs
  while (!range.AtEnd()) {
    const CSSParserToken* media_condition_start = &range.Peek();
    // The length is the last component value before the comma which isn't
    // whitespace or a comment
    const CSSParserToken* length_token_start = &range.Peek();
    const CSSParserToken* length_token_end = &range.Peek();
    while (!range.AtEnd() && range.Peek().GetType() != kCommaToken) {
      length_token_start = &range.Peek();
      range.ConsumeComponentValue();
      length_token_end = &range.Peek();
      range.ConsumeWhitespace();
    }
    range.Consume();

    float length;
    if (!CalculateLengthInPixels(
            range.MakeSubRange(length_token_start, length_token_end), length))
      continue;
    scoped_refptr<MediaQuerySet> media_condition =
        MediaQueryParser::ParseMediaCondition(
            range.MakeSubRange(media_condition_start, length_token_start));
    if (!media_condition || !MediaConditionMatches(*media_condition))
      continue;
    length_ = length;
    length_was_set_ = true;
    return true;
  }
  return false;
}

float SizesAttributeParser::EffectiveSize() {
  if (length_was_set_)
    return length_;
  return EffectiveSizeDefaultValue();
}

float SizesAttributeParser::EffectiveSizeDefaultValue() {
  // Returning the equivalent of "100vw"
  return clampTo<float>(media_values_->ViewportWidth());
}

}  // namespace blink
