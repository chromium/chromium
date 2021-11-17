// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

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
      execution_context_(execution_context),
      fake_context_(*MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode,
          SecureContextMode::kInsecureContext)) {}

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

std::unique_ptr<MediaQueryExpNode> MediaQueryParser::ConsumeFeature(
    CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kLeftParenthesisToken)
    return nullptr;

  CSSParserTokenRange block = range.ConsumeBlock();
  block.ConsumeWhitespace();
  range.ConsumeWhitespace();

  if (block.Peek().GetType() != kIdentToken)
    return nullptr;

  String feature_name = block.ConsumeIncludingWhitespace().Value().ToString();

  if (!IsMediaFeatureAllowedInMode(feature_name))
    return nullptr;

  // <mf-boolean> = <mf-name>
  if (block.AtEnd()) {
    auto exp = MediaQueryExp::Create(feature_name, block, fake_context_,
                                     execution_context_);
    if (!exp.IsValid())
      return nullptr;
    return std::make_unique<MediaQueryFeatureExpNode>(exp);
  }

  // <mf-plain> = <mf-name> : <mf-value>
  if (block.Peek().GetType() != kColonToken)
    return nullptr;
  block.ConsumeIncludingWhitespace();

  if (block.AtEnd())
    return nullptr;

  auto exp = MediaQueryExp::Create(feature_name, block, fake_context_,
                                   execution_context_);
  if (!exp.IsValid() || !block.AtEnd())
    return nullptr;

  return std::make_unique<MediaQueryFeatureExpNode>(exp);
}

std::unique_ptr<MediaQueryExpNode> MediaQueryParser::ConsumeCondition(
    CSSParserTokenRange& range,
    ConditionMode mode) {
  // <media-not>
  if (IsNotKeywordEnabled() && ConsumeIfIdent(range, "not"))
    return MediaQueryExpNode::Not(ConsumeInParens(range));

  // Otherwise:
  // <media-in-parens> [ <media-and>* | <media-or>* ]

  std::unique_ptr<MediaQueryExpNode> result = ConsumeInParens(range);

  if (AtIdent(range.Peek(), "and")) {
    while (result && ConsumeIfIdent(range, "and")) {
      result =
          MediaQueryExpNode::And(std::move(result), ConsumeInParens(range));
    }
  } else if (result && AtIdent(range.Peek(), "or") &&
             mode == ConditionMode::kNormal &&
             RuntimeEnabledFeatures::CSSMediaQueries4Enabled()) {
    while (result && ConsumeIfIdent(range, "or")) {
      result = MediaQueryExpNode::Or(std::move(result), ConsumeInParens(range));
    }
  }

  return result;
}

std::unique_ptr<MediaQueryExpNode> MediaQueryParser::ConsumeInParens(
    CSSParserTokenRange& range) {
  CSSParserTokenRange original_range = range;

  // ( <media-condition> )
  if (range.Peek().GetType() == kLeftParenthesisToken &&
      RuntimeEnabledFeatures::CSSMediaQueries4Enabled()) {
    CSSParserTokenRange block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();
    auto node = ConsumeCondition(block);
    if (node && block.AtEnd())
      return MediaQueryExpNode::Nested(std::move(node));
  }
  range = original_range;

  // TODO(crbug.com/962417): <general-enclosed>

  // <media-feature>
  return ConsumeFeature(range);
}

scoped_refptr<MediaQuerySet> MediaQueryParser::ConsumeSingleCondition(
    CSSParserTokenRange range) {
  DCHECK_EQ(parser_type_, kMediaConditionParser);
  DCHECK(!range.AtEnd());

  std::unique_ptr<MediaQueryExpNode> node = ConsumeCondition(range);

  if (!node || !range.AtEnd()) {
    query_set_->AddMediaQuery(MediaQuery::CreateNotAll());
  } else {
    query_set_->AddMediaQuery(std::make_unique<MediaQuery>(
        MediaQuery::kNone, media_type_names::kAll, std::move(node)));
  }

  return query_set_;
}

std::unique_ptr<MediaQuery> MediaQueryParser::ConsumeQuery(
    CSSParserTokenRange& range) {
  DCHECK_EQ(parser_type_, kMediaQuerySetParser);
  CSSParserTokenRange original_range = range;

  // First try to parse following grammar:
  //
  // [ not | only ]? <media-type> [ and <media-condition-without-or> ]?
  MediaQuery::RestrictorType restrictor = ConsumeRestrictor(range);
  String type = ConsumeType(range);

  if (!type.IsNull()) {
    if (!ConsumeIfIdent(range, "and"))
      return std::make_unique<MediaQuery>(restrictor, type, nullptr);
    if (auto node = ConsumeCondition(range, ConditionMode::kWithoutOr))
      return std::make_unique<MediaQuery>(restrictor, type, std::move(node));
    return nullptr;
  }
  range = original_range;

  // Otherwise, <media-condition>
  if (auto node = ConsumeCondition(range)) {
    return std::make_unique<MediaQuery>(
        MediaQuery::kNone, media_type_names::kAll, std::move(node));
  }
  return nullptr;
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
    std::unique_ptr<MediaQuery> query = ConsumeQuery(range);
    bool ok = query && (range.AtEnd() || range.Peek().GetType() == kCommaToken);
    if (!ok)
      query_set_->AddMediaQuery(MediaQuery::CreateNotAll());
    else
      query_set_->AddMediaQuery(std::move(query));
  } while (!range.AtEnd() && ConsumeUntilCommaInclusive(range));

  return query_set_;
}

bool MediaQueryParser::IsMediaFeatureAllowedInMode(
    const String& media_feature) const {
  return mode_ == kUASheetMode ||
         media_feature != media_feature_names::kImmersiveMediaFeature;
}

bool MediaQueryParser::IsNotKeywordEnabled() const {
  // Support for 'not' was shipped for kMediaConditionParser before
  // RuntimeEnabledFeatures::CSSMediaQueries4 existed, hence it's always
  // enabled for that parser type.
  return (parser_type_ == kMediaConditionParser) ||
         RuntimeEnabledFeatures::CSSMediaQueries4Enabled();
}

}  // namespace blink
