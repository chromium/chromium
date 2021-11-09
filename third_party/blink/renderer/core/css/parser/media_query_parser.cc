// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/media_type_names.h"

namespace blink {

using css_parsing_utils::AtIdent;
using css_parsing_utils::ConsumeIfIdent;

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

MediaQueryParser::MediaQueryParser(ParserType parser_type,
                                   CSSParserMode mode,
                                   const ExecutionContext* execution_context)
    : parser_type_(parser_type),
      query_set_(MediaQuerySet::Create()),
      mode_(mode),
      execution_context_(execution_context) {}

MediaQueryParser::~MediaQueryParser() = default;

namespace {

bool IsRestrictorOrLogicalOperator(const CSSParserToken& token) {
  // FIXME: it would be more efficient to use lower-case always for tokenValue.
  return EqualIgnoringASCIICase(token.Value(), "not") ||
         EqualIgnoringASCIICase(token.Value(), "and") ||
         EqualIgnoringASCIICase(token.Value(), "or") ||
         EqualIgnoringASCIICase(token.Value(), "only");
}

bool ConsumeUntilCommaInclusive(CSSParserTokenRange& range) {
  while (!range.AtEnd()) {
    if (range.Peek().GetType() == kCommaToken) {
      range.ConsumeIncludingWhitespace();
      return true;
    }
    range.ConsumeComponentValue();
  }
  return false;
}

}  // namespace

MediaQuery::RestrictorType MediaQueryParser::ConsumeRestrictor(
    CSSParserTokenRange& range) {
  if (ConsumeIfIdent(range, "not"))
    return MediaQuery::kNot;
  if (ConsumeIfIdent(range, "only"))
    return MediaQuery::kOnly;
  return MediaQuery::kNone;
}

String MediaQueryParser::ConsumeType(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken)
    return g_null_atom;
  if (IsRestrictorOrLogicalOperator(range.Peek()))
    return g_null_atom;
  return range.ConsumeIncludingWhitespace().Value().ToString();
}

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

bool MediaQueryParser::ConsumeAnd(CSSParserTokenRange& range) {
  while (ConsumeIfIdent(range, "and")) {
    if (!ConsumeFeature(range))
      return false;
  }
  return true;
}

scoped_refptr<MediaQuerySet> MediaQueryParser::ConsumeSingleCondition(
    CSSParserTokenRange range) {
  DCHECK_EQ(parser_type_, kMediaConditionParser);
  DCHECK(!range.AtEnd());

  if (ConsumeIfIdent(range, "not"))
    media_query_data_.SetRestrictor(MediaQuery::kNot);
  if (!ConsumeFeature(range) || !ConsumeAnd(range) || !range.AtEnd())
    query_set_->AddMediaQuery(MediaQuery::CreateNotAll());
  else
    query_set_->AddMediaQuery(media_query_data_.TakeMediaQuery());

  return query_set_;
}

bool MediaQueryParser::ConsumeQuery(CSSParserTokenRange& range) {
  DCHECK_EQ(parser_type_, kMediaQuerySetParser);
  media_query_data_.Clear();

  if (range.Peek().GetType() == kLeftParenthesisToken) {
    if (media_query_data_.Restrictor() != MediaQuery::kNone) {
      return false;
    } else {
      return ConsumeFeature(range) && ConsumeAnd(range);
    }
  } else if (range.Peek().GetType() == kIdentToken) {
    media_query_data_.SetRestrictor(ConsumeRestrictor(range));
    String type = ConsumeType(range);

    if (!type.IsNull()) {
      media_query_data_.SetMediaType(type);
      return ConsumeAnd(range);
    } else {
      return false;
    }
  } else {
    return false;
  }
}

scoped_refptr<MediaQuerySet> MediaQueryParser::ParseImpl(
    CSSParserTokenRange range) {
  range.ConsumeWhitespace();

  // Note that we currently expect an empty input to evaluate to an empty
  // MediaQuerySet, rather than "not all".
  if (range.AtEnd())
    return query_set_;

  if (parser_type_ == kMediaConditionParser)
    return ConsumeSingleCondition(range);

  DCHECK_EQ(parser_type_, kMediaQuerySetParser);

  do {
    bool ok = ConsumeQuery(range) &&
              (range.AtEnd() || range.Peek().GetType() == kCommaToken);
    if (!ok)
      query_set_->AddMediaQuery(MediaQuery::CreateNotAll());
    else
      query_set_->AddMediaQuery(media_query_data_.TakeMediaQuery());
  } while (!range.AtEnd() && ConsumeUntilCommaInclusive(range));

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
