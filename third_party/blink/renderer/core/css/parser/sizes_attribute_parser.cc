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

  CSSParserTokenStream stream(attribute);
  is_valid_ = Parse(stream);
}

bool SizesAttributeParser::Parse(CSSParserTokenStream& stream) {
  while (!stream.AtEnd()) {
    stream.ConsumeWhitespace();

    if (css_parsing_utils::AtIdent(stream.Peek(), "auto")) {
      // Spec: "For better backwards-compatibility with legacy user
      // agents that don't support the auto keyword, fallback sizes
      // can be specified if desired."
      // For example: sizes="auto, (max-width: 30em) 100vw, ..."
      is_auto_ = true;
      return true;
    }

    CSSParserTokenStream::State savepoint = stream.Save();
    MediaQuerySet* media_condition =
        MediaQueryParser::ParseMediaCondition(stream, execution_context_);
    if (!media_condition || !MediaConditionMatches(*media_condition)) {
      // If we failed to parse a media condition, most likely there
      // simply wasn't any and we won't have moved in the stream.
      // However, there are certain edge cases where we _thought_
      // we would have parsed a media condition but it was actually
      // meant as a size; in particular, a calc() expression would
      // count as <general-enclosed> and thus be parsed as a media
      // condition, then promptly fail, whereas we should really
      // parse it as a size. Thus, we need to rewind in this case.
      // If it really were a valid but failing media condition,
      // this rewinding is harmless; we'd try parsing the media
      // condition as a size and then fail (if nothing else, because
      // the comma is not immediately after it).
      stream.EnsureLookAhead();
      stream.Restore(savepoint);
    }

    if (stream.Peek().GetType() != kCommaToken) {
      float length;
      if (CalculateLengthInPixels(stream, length)) {
        stream.ConsumeWhitespace();
        if (stream.AtEnd() || stream.Peek().GetType() == kCommaToken) {
          size_ = length;
          size_was_set_ = true;
          return true;
        }
      }
    }

    stream.SkipUntilPeekedTypeIs<kCommaToken>();
    if (!stream.AtEnd()) {
      stream.Consume();
    }
  }

  return false;
}

bool SizesAttributeParser::CalculateLengthInPixels(CSSParserTokenStream& stream,
                                                   float& result) {
  const CSSParserToken& start_token = stream.Peek();
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
      stream.Consume();
      return true;
    }
  } else if (type == kFunctionToken) {
    SizesMathFunctionParser calc_parser(stream, media_values_);
    if (!calc_parser.IsValid()) {
      return false;
    }

    result = calc_parser.Result();
    return true;
  } else if (type == kNumberToken && !start_token.NumericValue()) {
    stream.Consume();
    result = 0;
    return true;
  }

  return false;
}

bool SizesAttributeParser::MediaConditionMatches(
    const MediaQuerySet& media_condition) {
  // A Media Condition cannot have a media type other then screen.
  MediaQueryEvaluator* media_query_evaluator =
      MakeGarbageCollected<MediaQueryEvaluator>(media_values_);

  return media_query_evaluator->Eval(media_condition);
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
