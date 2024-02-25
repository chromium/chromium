// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/sizes_attribute_parser.h"

#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/sizes_math_function_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/media_type_names.h"

namespace blink {

SizesAttributeParser::SizesAttributeParser(
    MediaValues* media_values,
    const String& attribute,
    const ExecutionContext* execution_context,
    const HTMLImageElement* img)
    : media_values_(media_values),
      execution_context_(execution_context),
      img_(img) {
  DCHECK(media_values_);
  DCHECK(media_values_->Width().has_value());
  DCHECK(media_values_->Height().has_value());

  CSSTokenizer tokenizer(attribute);
  auto [tokens, offsets] = tokenizer.TokenizeToEOFWithOffsets();
  is_valid_ =
      Parse(CSSParserTokenRange(tokens),
            CSSParserTokenOffsets(tokens, std::move(offsets), attribute));
}

bool SizesAttributeParser::Parse(CSSParserTokenRange range,
                                 const CSSParserTokenOffsets& offsets) {
  // Split on a comma token and parse the result tokens as (media-condition,
  // length) pairs
  while (!range.AtEnd()) {
    if (RuntimeEnabledFeatures::AutoSizeLazyLoadedImagesEnabled() &&
        css_parsing_utils::AtIdent(range.Peek(), "auto")) {
      // Spec: "For better backwards-compatibility with legacy user
      // agents that don't support the auto keyword, fallback sizes
      // can be specified if desired."
      // For example: sizes="auto, (max-width: 30em) 100vw, ..."
      is_auto_ = true;
      return true;
    }

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
            range.MakeSubRange(length_token_start, length_token_end), length)) {
      continue;
    }

    MediaQuerySet* media_condition = MediaQueryParser::ParseMediaCondition(
        range.MakeSubRange(media_condition_start, length_token_start), offsets,
        execution_context_);
    if (!media_condition || !MediaConditionMatches(*media_condition)) {
      continue;
    }

    size_ = length;
    size_was_set_ = true;
    return true;
  }

  return false;
}

bool SizesAttributeParser::CalculateLengthInPixels(CSSParserTokenRange range,
                                                   float& result) {
  const CSSParserToken& start_token = range.Peek();
  CSSParserTokenType type = start_token.GetType();
  if (type == kDimensionToken) {
    double length;
    if (!CSSPrimitiveValue::IsLength(start_token.GetUnitType())) {
      return false;
    }

    if ((media_values_->ComputeLength(start_token.NumericValue(),
                                      start_token.GetUnitType(), length)) &&
        (length >= 0)) {
      result = ClampTo<float>(length);
      return true;
    }
  } else if (type == kFunctionToken) {
    SizesMathFunctionParser calc_parser(range, media_values_);
    if (!calc_parser.IsValid()) {
      return false;
    }

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
  MediaQueryEvaluator media_query_evaluator(media_values_);

  return media_query_evaluator.Eval(media_condition);
}

float SizesAttributeParser::Size() {
  if (is_valid_) {
    return EffectiveSize();
  }

  return EffectiveSizeDefaultValue();
}

float SizesAttributeParser::EffectiveSize() {
  // Spec:
  // https://html.spec.whatwg.org/#parsing-a-sizes-attribute

  // 3.6 If size is not auto, then return size.
  if (size_was_set_) {
    return size_;
  }

  // 3.3 If size is auto, and img is not null, and img is being rendered, and
  // img allows auto-sizes, then set size to the concrete object size width of
  // img, in CSS pixels.
  if (is_auto_ && img_ && img_->IsBeingRendered() && img_->AllowAutoSizes()) {
    return img_->LayoutBoxWidth();
  }

  // 4. Return 100vw.
  return EffectiveSizeDefaultValue();
}

float SizesAttributeParser::EffectiveSizeDefaultValue() {
  // Returning the equivalent of "100vw"
  return ClampTo<float>(*media_values_->Width());
}

bool SizesAttributeParser::IsAuto() {
  return is_auto_;
}

}  // namespace blink
