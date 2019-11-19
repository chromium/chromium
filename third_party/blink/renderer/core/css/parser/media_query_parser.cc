// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"

#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/media_type_names.h"

namespace blink {

scoped_refptr<MediaQuerySet> MediaQueryParser::ParseMediaQuerySet(
    const String& query_string) {
  return ParseMediaQuerySet(
      CSSParserTokenRange(CSSTokenizer(query_string).TokenizeToEOF()));
}

scoped_refptr<MediaQuerySet> MediaQueryParser::ParseMediaQuerySet(
    CSSParserTokenRange range) {
  return MediaQueryParser(kMediaQuerySetParser, kHTMLStandardMode)
      .ParseImpl(range);
}

scoped_refptr<MediaQuerySet> MediaQueryParser::ParseMediaQuerySetInMode(
    CSSParserTokenRange range,
    CSSParserMode mode) {
  return MediaQueryParser(kMediaQuerySetParser, mode).ParseImpl(range);
}

scoped_refptr<MediaQuerySet> MediaQueryParser::ParseMediaCondition(
    CSSParserTokenRange range) {
  return MediaQueryParser(kMediaConditionParser, kHTMLStandardMode)
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

MediaQueryParser::MediaQueryParser(ParserType parser_type, CSSParserMode mode)
    : parser_type_(parser_type),
      query_set_(MediaQuerySet::Create()),
      mode_(mode) {
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
void MediaQueryParser::ReadRestrictor(CSSParserTokenType type,
                                      const CSSParserToken& token,
                                      CSSParserTokenRange& range) {
  ReadMediaType(type, token, range);
}

void MediaQueryParser::ReadMediaNot(CSSParserTokenType type,
                                    const CSSParserToken& token,
                                    CSSParserTokenRange& range) {
  if (type == kIdentToken && EqualIgnoringASCIICase(token.Value(), "not"))
    SetStateAndRestrict(kReadFeatureStart, MediaQuery::kNot);
  else
    ReadFeatureStart(type, token, range);
}

static bool IsRestrictorOrLogicalOperator(const CSSParserToken& token) {
  // FIXME: it would be more efficient to use lower-case always for tokenValue.
  return EqualIgnoringASCIICase(token.Value(), "not") ||
         EqualIgnoringASCIICase(token.Value(), "and") ||
         EqualIgnoringASCIICase(token.Value(), "or") ||
         EqualIgnoringASCIICase(token.Value(), "only");
}

void MediaQueryParser::ReadMediaType(CSSParserTokenType type,
                                     const CSSParserToken& token,
                                     CSSParserTokenRange& range) {
  if (type == kLeftParenthesisToken) {
    if (media_query_data_.Restrictor() != MediaQuery::kNone)
      state_ = kSkipUntilComma;
    else
      state_ = kReadFeature;
  } else if (type == kIdentToken) {
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
  } else if (type == kEOFToken &&
             (!query_set_->QueryVector().size() || state_ != kReadRestrictor)) {
    state_ = kDone;
  } else {
    state_ = kSkipUntilComma;
    if (type == kCommaToken)
      SkipUntilComma(type, token, range);
  }
}

void MediaQueryParser::ReadAnd(CSSParserTokenType type,
                               const CSSParserToken& token,
                               CSSParserTokenRange& range) {
  if (type == kIdentToken && EqualIgnoringASCIICase(token.Value(), "and")) {
    state_ = kReadFeatureStart;
  } else if (type == kCommaToken && parser_type_ != kMediaConditionParser) {
    query_set_->AddMediaQuery(media_query_data_.TakeMediaQuery());
    state_ = kReadRestrictor;
  } else if (type == kEOFToken) {
    state_ = kDone;
  } else {
    state_ = kSkipUntilComma;
  }
}

void MediaQueryParser::ReadFeatureStart(CSSParserTokenType type,
                                        const CSSParserToken& token,
                                        CSSParserTokenRange& range) {
  if (type == kLeftParenthesisToken)
    state_ = kReadFeature;
  else
    state_ = kSkipUntilComma;
}

void MediaQueryParser::ReadFeature(CSSParserTokenType type,
                                   const CSSParserToken& token,
                                   CSSParserTokenRange& range) {
  if (type == kIdentToken) {
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

void MediaQueryParser::ReadFeatureColon(CSSParserTokenType type,
                                        const CSSParserToken& token,
                                        CSSParserTokenRange& range) {
  if (type == kColonToken) {
    while (range.Peek().GetType() == kWhitespaceToken)
      range.Consume();
    if (range.Peek().GetType() == kRightParenthesisToken || type == kEOFToken)
      state_ = kSkipUntilBlockEnd;
    else
      state_ = kReadFeatureValue;
  } else if (type == kRightParenthesisToken || type == kEOFToken) {
    media_query_data_.AddExpression(range);
    ReadFeatureEnd(type, token, range);
  } else {
    state_ = kSkipUntilBlockEnd;
  }
}

void MediaQueryParser::ReadFeatureValue(CSSParserTokenType type,
                                        const CSSParserToken& token,
                                        CSSParserTokenRange& range) {
  if (type == kDimensionToken &&
      token.GetUnitType() == CSSPrimitiveValue::UnitType::kUnknown) {
    range.Consume();
    state_ = kSkipUntilComma;
  } else {
    media_query_data_.AddExpression(range);
    state_ = kReadFeatureEnd;
  }
}

void MediaQueryParser::ReadFeatureEnd(CSSParserTokenType type,
                                      const CSSParserToken& token,
                                      CSSParserTokenRange& range) {
  if (type == kRightParenthesisToken || type == kEOFToken) {
    if (media_query_data_.LastExpressionValid())
      state_ = kReadAnd;
    else
      state_ = kSkipUntilComma;
  } else {
    media_query_data_.RemoveLastExpression();
    state_ = kSkipUntilBlockEnd;
  }
}

void MediaQueryParser::SkipUntilComma(CSSParserTokenType type,
                                      const CSSParserToken& token,
                                      CSSParserTokenRange& range) {
  if ((type == kCommaToken && !block_watcher_.BlockLevel()) ||
      type == kEOFToken) {
    state_ = kReadRestrictor;
    media_query_data_.Clear();
    query_set_->AddMediaQuery(MediaQuery::CreateNotAll());
  }
}

void MediaQueryParser::SkipUntilBlockEnd(CSSParserTokenType type,
                                         const CSSParserToken& token,
                                         CSSParserTokenRange& range) {
  if (token.GetBlockType() == CSSParserToken::kBlockEnd &&
      !block_watcher_.BlockLevel())
    state_ = kSkipUntilComma;
}

void MediaQueryParser::Done(CSSParserTokenType type,
                            const CSSParserToken& token,
                            CSSParserTokenRange& range) {}

void MediaQueryParser::HandleBlocks(const CSSParserToken& token) {
  if (token.GetBlockType() == CSSParserToken::kBlockStart &&
      (token.GetType() != kLeftParenthesisToken || block_watcher_.BlockLevel()))
    state_ = kSkipUntilBlockEnd;
}

void MediaQueryParser::ProcessToken(const CSSParserToken& token,
                                    CSSParserTokenRange& range) {
  CSSParserTokenType type = token.GetType();

  if (state_ != kReadFeatureValue || type == kWhitespaceToken) {
    HandleBlocks(token);
    block_watcher_.HandleToken(token);
    range.Consume();
  }

  // Call the function that handles current state
  if (type != kWhitespaceToken)
    ((this)->*(state_))(type, token, range);
}

// The state machine loop
scoped_refptr<MediaQuerySet> MediaQueryParser::ParseImpl(
    CSSParserTokenRange range) {
  while (!range.AtEnd())
    ProcessToken(range.Peek(), range);

  // FIXME: Can we get rid of this special case?
  if (parser_type_ == kMediaQuerySetParser)
    ProcessToken(CSSParserToken(kEOFToken), range);

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
      media_type_set_(false) {}

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

void MediaQueryData::AddExpression(CSSParserTokenRange& range) {
  expressions_.push_back(MediaQueryExp::Create(media_feature_, range));
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
