// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/media_type_names.h"

namespace blink {

scoped_refptr<MediaQuerySet> MediaQueryParser::ParseMediaQuerySet(
    const String& query_string,
    const ExecutionContext* execution_context) {
  return ParseMediaQuerySet(
      CSSParserTokenRange(CSSTokenizer(query_string).TokenizeToEOF()),
      execution_context);
}

scoped_refptr<MediaQuerySet> MediaQueryParser::ParseMediaQuerySet(
    CSSParserTokenRange range,
    const ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaQuerySetParser, kHTMLStandardMode,
                          execution_context)
      .ParseImpl(range);
}

scoped_refptr<MediaQuerySet> MediaQueryParser::ParseMediaQuerySetInMode(
    CSSParserTokenRange range,
    CSSParserMode mode,
    const ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaQuerySetParser, mode, execution_context)
      .ParseImpl(range);
}

scoped_refptr<MediaQuerySet> MediaQueryParser::ParseMediaCondition(
    CSSParserTokenRange range,
    const ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaConditionParser, kHTMLStandardMode,
                          execution_context)
      .ParseImpl(range);
}

const MediaQueryParser::State MediaQueryParser::kReadRestrictor =
    &MediaQueryParser::ReadRestrictor;
const MediaQueryParser::State MediaQueryParser::kReadMediaNot =
    &MediaQueryParser::ReadMediaNot;
const MediaQueryParser::State MediaQueryParser::kReadMediaType =
    &MediaQueryParser::ReadMediaType;
const MediaQueryParser::State MediaQueryParser::kReadAnd =
    &MediaQueryParser::ReadAnd;
const MediaQueryParser::State MediaQueryParser::kReadFeatureStart =
    &MediaQueryParser::ReadFeatureStart;
const MediaQueryParser::State MediaQueryParser::kSkipUntilComma =
    &MediaQueryParser::SkipUntilComma;
const MediaQueryParser::State MediaQueryParser::kDone = &MediaQueryParser::Done;

MediaQueryParser::MediaQueryParser(ParserType parser_type,
                                   CSSParserMode mode,
                                   const ExecutionContext* execution_context)
    : parser_type_(parser_type),
      query_set_(MediaQuerySet::Create()),
      mode_(mode),
      execution_context_(execution_context) {
  if (parser_type == kMediaQuerySetParser)
    state_ = &MediaQueryParser::ReadRestrictor;
  else  // MediaConditionParser
    state_ = &MediaQueryParser::ReadMediaNot;
}

MediaQueryParser::~MediaQueryParser() = default;

bool MediaQueryParser::ConsumeFeature(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kLeftParenthesisToken)
    return false;

  CSSParserTokenRange block = range.ConsumeBlock();
  block.ConsumeWhitespace();
  range.ConsumeWhitespace();

  if (block.Peek().GetType() != kIdentToken)
    return false;

  String feature_name = block.ConsumeIncludingWhitespace().Value().ToString();

  if (!IsMediaFeatureAllowedInMode(feature_name))
    return false;

  media_query_data_.SetMediaFeature(feature_name);

  // <mf-boolean> = <mf-name>
  if (block.AtEnd()) {
    media_query_data_.AddExpression(block, execution_context_);
    return media_query_data_.LastExpressionValid();
  }

  // <mf-plain> = <mf-name> : <mf-value>
  if (block.Peek().GetType() != kColonToken)
    return false;
  block.ConsumeIncludingWhitespace();

  if (block.AtEnd())
    return false;

  media_query_data_.AddExpression(block, execution_context_);
  return block.AtEnd() && media_query_data_.LastExpressionValid();
}

void MediaQueryParser::SetStateAndRestrict(
    State state,
    MediaQuery::RestrictorType restrictor) {
  media_query_data_.SetRestrictor(restrictor);
  state_ = state;
}

// State machine member functions start here
void MediaQueryParser::ReadRestrictor(CSSParserTokenRange& range) {
  ReadMediaType(range);
}

void MediaQueryParser::ReadMediaNot(CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kIdentToken &&
      EqualIgnoringASCIICase(range.Peek().Value(), "not")) {
    ConsumeToken(range);
    SetStateAndRestrict(kReadFeatureStart, MediaQuery::kNot);
  } else {
    ReadFeatureStart(range);
  }
}

static bool IsRestrictorOrLogicalOperator(const CSSParserToken& token) {
  // FIXME: it would be more efficient to use lower-case always for tokenValue.
  return EqualIgnoringASCIICase(token.Value(), "not") ||
         EqualIgnoringASCIICase(token.Value(), "and") ||
         EqualIgnoringASCIICase(token.Value(), "or") ||
         EqualIgnoringASCIICase(token.Value(), "only");
}

void MediaQueryParser::ReadMediaType(CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kLeftParenthesisToken) {
    if (media_query_data_.Restrictor() != MediaQuery::kNone)
      state_ = kSkipUntilComma;
    else
      state_ = kReadFeatureStart;
  } else if (range.Peek().GetType() == kIdentToken) {
    CSSParserToken token = ConsumeToken(range);
    if (state_ == kReadRestrictor &&
        EqualIgnoringASCIICase(token.Value(), "not")) {
      SetStateAndRestrict(kReadMediaType, MediaQuery::kNot);
    } else if (state_ == kReadRestrictor &&
               EqualIgnoringASCIICase(token.Value(), "only")) {
      SetStateAndRestrict(kReadMediaType, MediaQuery::kOnly);
    } else if (media_query_data_.Restrictor() != MediaQuery::kNone &&
               IsRestrictorOrLogicalOperator(token)) {
      state_ = kSkipUntilComma;
    } else {
      media_query_data_.SetMediaType(token.Value().ToString());
      state_ = kReadAnd;
    }
  } else if (range.AtEnd() &&
             (!query_set_->QueryVector().size() || state_ != kReadRestrictor)) {
    state_ = kDone;
  } else {
    state_ = kSkipUntilComma;
    if (range.Peek().GetType() == kCommaToken)
      SkipUntilComma(range);
    else
      range.ConsumeComponentValue();
  }
}

void MediaQueryParser::ReadAnd(CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kIdentToken &&
      EqualIgnoringASCIICase(range.Peek().Value(), "and")) {
    ConsumeToken(range);
    state_ = kReadFeatureStart;
  } else if (range.Peek().GetType() == kCommaToken &&
             parser_type_ != kMediaConditionParser) {
    ConsumeToken(range);
    query_set_->AddMediaQuery(media_query_data_.TakeMediaQuery());
    state_ = kReadRestrictor;
  } else if (range.AtEnd()) {
    state_ = kDone;
  } else {
    state_ = kSkipUntilComma;
  }
}

void MediaQueryParser::ReadFeatureStart(CSSParserTokenRange& range) {
  if (!ConsumeFeature(range))
    state_ = kSkipUntilComma;
  else
    state_ = kReadAnd;
}

void MediaQueryParser::SkipUntilComma(CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kCommaToken || range.AtEnd()) {
    ConsumeToken(range);
    if (parser_type_ == kMediaQuerySetParser) {
      state_ = kReadRestrictor;
      media_query_data_.Clear();
      query_set_->AddMediaQuery(MediaQuery::CreateNotAll());
    }
  } else {
    range.ConsumeComponentValue();
  }
}

void MediaQueryParser::Done(CSSParserTokenRange& range) {
  DCHECK(range.AtEnd());
}

CSSParserToken MediaQueryParser::ConsumeToken(CSSParserTokenRange& range) {
  CSSParserToken token = range.Consume();
  DCHECK_EQ(token.GetBlockType(), CSSParserToken::kNotBlock);
  return token;
}

void MediaQueryParser::ProcessToken(CSSParserTokenRange& range) {
  // Call the function that handles current state
  if (range.Peek().GetType() != kWhitespaceToken)
    ((this)->*(state_))(range);
  else
    range.Consume();
}

// The state machine loop
scoped_refptr<MediaQuerySet> MediaQueryParser::ParseImpl(
    CSSParserTokenRange range) {
#if DCHECK_IS_ON()
  // Used to detect loops.
  Vector<State> seen_states;
#endif  // DCHECK_IS_ON()

  while (!range.AtEnd()) {
#if DCHECK_IS_ON()
    CSSParserTokenRange original_range = range;
#endif  // DCHECK_IS_ON()
    ProcessToken(range);
#if DCHECK_IS_ON()
    // If we did not advance |range|, then we must switch to a new
    // state. If there's a loop, we'll eventually run out of states.
    if (!range.AtEnd() && &range.Peek() <= &original_range.Peek()) {
      DCHECK(!seen_states.Contains(state_));
      seen_states.push_back(state_);
    } else {
      seen_states.clear();
    }
#endif  // DCHECK_IS_ON()
  }

  // FIXME: Can we get rid of this special case?
  if (parser_type_ == kMediaQuerySetParser) {
    CSSParserTokenRange empty = range.MakeSubRange(range.end(), range.end());
    ProcessToken(empty);
  }

  if (state_ != kReadAnd && state_ != kReadRestrictor && state_ != kDone &&
      state_ != kReadMediaNot)
    query_set_->AddMediaQuery(MediaQuery::CreateNotAll());
  else if (media_query_data_.CurrentMediaQueryChanged())
    query_set_->AddMediaQuery(media_query_data_.TakeMediaQuery());

  return query_set_;
}

bool MediaQueryParser::IsMediaFeatureAllowedInMode(
    const String& media_feature) const {
  return mode_ == kUASheetMode ||
         media_feature != media_feature_names::kImmersiveMediaFeature;
}

MediaQueryData::MediaQueryData()
    : restrictor_(MediaQuery::kNone),
      media_type_(media_type_names::kAll),
      media_type_set_(false),
      fake_context_(*MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode,
          SecureContextMode::kInsecureContext)) {}

void MediaQueryData::Clear() {
  restrictor_ = MediaQuery::kNone;
  media_type_ = media_type_names::kAll;
  media_type_set_ = false;
  media_feature_ = String();
  expressions_.clear();
}

std::unique_ptr<MediaQuery> MediaQueryData::TakeMediaQuery() {
  std::unique_ptr<MediaQuery> media_query = std::make_unique<MediaQuery>(
      restrictor_, std::move(media_type_), std::move(expressions_));
  Clear();
  return media_query;
}

void MediaQueryData::AddExpression(CSSParserTokenRange& range,
                                   const ExecutionContext* execution_context) {
  expressions_.push_back(MediaQueryExp::Create(
      media_feature_, range, fake_context_, execution_context));
}

bool MediaQueryData::LastExpressionValid() {
  return expressions_.back().IsValid();
}

void MediaQueryData::RemoveLastExpression() {
  expressions_.pop_back();
}

void MediaQueryData::SetMediaType(const String& media_type) {
  media_type_ = media_type;
  media_type_set_ = true;
}

}  // namespace blink
