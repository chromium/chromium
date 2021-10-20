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
const MediaQueryParser::State MediaQueryParser::kReadFeature =
    &MediaQueryParser::ReadFeature;
const MediaQueryParser::State MediaQueryParser::kReadFeatureColon =
    &MediaQueryParser::ReadFeatureColon;
const MediaQueryParser::State MediaQueryParser::kReadFeatureValue =
    &MediaQueryParser::ReadFeatureValue;
const MediaQueryParser::State MediaQueryParser::kReadFeatureEnd =
    &MediaQueryParser::ReadFeatureEnd;
const MediaQueryParser::State MediaQueryParser::kSkipUntilComma =
    &MediaQueryParser::SkipUntilComma;
const MediaQueryParser::State MediaQueryParser::kSkipUntilBlockEnd =
    &MediaQueryParser::SkipUntilBlockEnd;
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
    ConsumeToken(range);
    if (media_query_data_.Restrictor() != MediaQuery::kNone)
      state_ = kSkipUntilComma;
    else
      state_ = kReadFeature;
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
      ConsumeToken(range);
  }
}

void MediaQueryParser::ReadAnd(CSSParserTokenRange& range) {
  CSSParserToken token = ConsumeToken(range);

  if (token.GetType() == kIdentToken &&
      EqualIgnoringASCIICase(token.Value(), "and")) {
    state_ = kReadFeatureStart;
  } else if (token.GetType() == kCommaToken &&
             parser_type_ != kMediaConditionParser) {
    query_set_->AddMediaQuery(media_query_data_.TakeMediaQuery());
    state_ = kReadRestrictor;
  } else if (token.GetType() == kEOFToken) {
    state_ = kDone;
  } else {
    state_ = kSkipUntilComma;
  }
}

void MediaQueryParser::ReadFeatureStart(CSSParserTokenRange& range) {
  CSSParserToken token = ConsumeToken(range);

  if (token.GetType() == kLeftParenthesisToken)
    state_ = kReadFeature;
  else
    state_ = kSkipUntilComma;
}

void MediaQueryParser::ReadFeature(CSSParserTokenRange& range) {
  CSSParserToken token = ConsumeToken(range);

  if (token.GetType() == kIdentToken) {
    String media_feature = token.Value().ToString();
    if (IsMediaFeatureAllowedInMode(media_feature)) {
      media_query_data_.SetMediaFeature(media_feature);
      state_ = kReadFeatureColon;
    } else {
      state_ = kSkipUntilComma;
    }
  } else {
    state_ = kSkipUntilComma;
  }
}

void MediaQueryParser::ReadFeatureColon(CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kColonToken) {
    CSSParserToken token = ConsumeToken(range);
    while (range.Peek().GetType() == kWhitespaceToken)
      range.Consume();
    if (range.Peek().GetType() == kRightParenthesisToken ||
        token.GetType() == kEOFToken)
      state_ = kSkipUntilBlockEnd;
    else
      state_ = kReadFeatureValue;
  } else if (range.Peek().GetType() == kRightParenthesisToken ||
             range.AtEnd()) {
    media_query_data_.AddExpression(range, execution_context_);
    ReadFeatureEnd(range);
  } else {
    ConsumeToken(range);
    state_ = kSkipUntilBlockEnd;
  }
}

void MediaQueryParser::ReadFeatureValue(CSSParserTokenRange& range) {
  media_query_data_.AddExpression(range, execution_context_);
  state_ = kReadFeatureEnd;
}

void MediaQueryParser::ReadFeatureEnd(CSSParserTokenRange& range) {
  CSSParserToken token = ConsumeToken(range);

  if (token.GetType() == kRightParenthesisToken ||
      token.GetType() == kEOFToken) {
    if (media_query_data_.LastExpressionValid())
      state_ = kReadAnd;
    else
      state_ = kSkipUntilComma;
  } else {
    media_query_data_.RemoveLastExpression();
    state_ = kSkipUntilBlockEnd;
  }
}

void MediaQueryParser::SkipUntilComma(CSSParserTokenRange& range) {
  CSSParserToken token = ConsumeToken(range);

  if ((token.GetType() == kCommaToken && !block_watcher_.BlockLevel()) ||
      token.GetType() == kEOFToken) {
    if (parser_type_ == kMediaQuerySetParser) {
      state_ = kReadRestrictor;
      media_query_data_.Clear();
      query_set_->AddMediaQuery(MediaQuery::CreateNotAll());
    }
  }
}

void MediaQueryParser::SkipUntilBlockEnd(CSSParserTokenRange& range) {
  CSSParserToken token = ConsumeToken(range);

  if (token.GetBlockType() == CSSParserToken::kBlockEnd &&
      !block_watcher_.BlockLevel())
    state_ = kSkipUntilComma;
}

void MediaQueryParser::Done(CSSParserTokenRange& range) {
  DCHECK(range.AtEnd());
}

CSSParserToken MediaQueryParser::ConsumeToken(CSSParserTokenRange& range) {
  CSSParserToken token = range.Consume();
  block_watcher_.HandleToken(token);
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
