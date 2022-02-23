// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using css_parsing_utils::AtIdent;
using css_parsing_utils::ConsumeAnyValue;
using css_parsing_utils::ConsumeIfDelimiter;
using css_parsing_utils::ConsumeIfIdent;

namespace {

class MediaQueryFeatureSet : public MediaQueryParser::FeatureSet {
  STACK_ALLOCATED();

 public:
  explicit MediaQueryFeatureSet(CSSParserMode parser_mode)
      : parser_mode_(parser_mode) {}

  bool IsAllowed(const String& feature) const override {
    if (feature == media_feature_names::kImmersiveMediaFeature)
      return parser_mode_ == kUASheetMode;
    if (feature == media_feature_names::kInlineSizeMediaFeature ||
        feature == media_feature_names::kMinInlineSizeMediaFeature ||
        feature == media_feature_names::kMaxInlineSizeMediaFeature ||
        feature == media_feature_names::kBlockSizeMediaFeature ||
        feature == media_feature_names::kMinBlockSizeMediaFeature ||
        feature == media_feature_names::kMaxBlockSizeMediaFeature) {
      return false;
    }
    return true;
  }

 private:
  CSSParserMode parser_mode_;
};

}  // namespace

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
                                   const ExecutionContext* execution_context,
                                   SyntaxLevel syntax_level)
    : parser_type_(parser_type),
      query_set_(MediaQuerySet::Create()),
      mode_(mode),
      execution_context_(execution_context),
      syntax_level_(syntax_level),
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

bool IsComparisonDelimiter(UChar c) {
  return c == '<' || c == '>' || c == '=';
}

CSSParserTokenRange ConsumeUntilComparisonOrColon(CSSParserTokenRange& range) {
  const CSSParserToken* first = range.begin();
  while (!range.AtEnd()) {
    const CSSParserToken& token = range.Peek();
    if ((token.GetType() == kDelimiterToken &&
         IsComparisonDelimiter(token.Delimiter())) ||
        token.GetType() == kColonToken) {
      break;
    }
    range.ConsumeComponentValue();
  }
  return range.MakeSubRange(first, range.begin());
}

bool IsLtLe(MediaQueryOperator op) {
  return op == MediaQueryOperator::kLt || op == MediaQueryOperator::kLe;
}

bool IsGtGe(MediaQueryOperator op) {
  return op == MediaQueryOperator::kGt || op == MediaQueryOperator::kGe;
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

MediaQueryOperator MediaQueryParser::ConsumeComparison(
    CSSParserTokenRange& range) {
  const CSSParserToken& first = range.Peek();
  if (first.GetType() != kDelimiterToken)
    return MediaQueryOperator::kNone;
  DCHECK(IsComparisonDelimiter(first.Delimiter()));
  switch (first.Delimiter()) {
    case '=':
      range.ConsumeIncludingWhitespace();
      return MediaQueryOperator::kEq;
    case '<':
      range.Consume();
      if (ConsumeIfDelimiter(range, '='))
        return MediaQueryOperator::kLe;
      range.ConsumeWhitespace();
      return MediaQueryOperator::kLt;
    case '>':
      range.Consume();
      if (ConsumeIfDelimiter(range, '='))
        return MediaQueryOperator::kGe;
      range.ConsumeWhitespace();
      return MediaQueryOperator::kGt;
  }

  NOTREACHED();
  return MediaQueryOperator::kNone;
}

String MediaQueryParser::ConsumeAllowedName(CSSParserTokenRange& range,
                                            const FeatureSet& feature_set) {
  if (range.Peek().GetType() != kIdentToken)
    return g_null_atom;
  String name =
      AttemptStaticStringCreation(range.Peek().Value().ToString().LowerASCII());
  if (!feature_set.IsAllowed(name))
    return g_null_atom;
  range.ConsumeIncludingWhitespace();
  return name;
}

String MediaQueryParser::ConsumeUnprefixedName(CSSParserTokenRange& range,
                                               const FeatureSet& feature_set) {
  String name = ConsumeAllowedName(range, feature_set);
  if (name.IsNull())
    return name;
  DCHECK_EQ(name, name.LowerASCII());
  if (name.StartsWith("min-") || name.StartsWith("max-"))
    return g_null_atom;
  return name;
}

std::unique_ptr<MediaQueryExpNode> MediaQueryParser::ParseNameValueComparison(
    CSSParserTokenRange lhs,
    MediaQueryOperator op,
    CSSParserTokenRange rhs,
    NameAffinity name_affinity,
    const FeatureSet& feature_set) {
  if (name_affinity == NameAffinity::kRight)
    std::swap(lhs, rhs);

  String feature_name = ConsumeUnprefixedName(lhs, feature_set);
  if (feature_name.IsNull() || !lhs.AtEnd())
    return nullptr;

  auto value = MediaQueryExpValue::Consume(feature_name, rhs, fake_context_,
                                           execution_context_);

  if (!value || !rhs.AtEnd())
    return nullptr;

  auto left = MediaQueryExpComparison();
  auto right = MediaQueryExpComparison(*value, op);

  if (name_affinity == NameAffinity::kRight)
    std::swap(left, right);

  return std::make_unique<MediaQueryFeatureExpNode>(
      MediaQueryExp::Create(feature_name, MediaQueryExpBounds(left, right)));
}

std::unique_ptr<MediaQueryExpNode> MediaQueryParser::ConsumeFeature(
    CSSParserTokenRange& range,
    const FeatureSet& feature_set) {
  // Because we don't know exactly where <mf-name> appears in the grammar, we
  // split |range| on top-level separators, and parse each segment
  // individually.
  //
  // Local variables names in this function are chosen with the expectation
  // that we are heading towards the most complicated form of <mf-range>:
  //
  //  <mf-value> <mf-gt> <mf-name> <mf-gt> <mf-value>
  //
  // Which corresponds to the local variables:
  //
  //  <segment1> <op1> <segment2> <op2> <segment3>

  CSSParserTokenRange segment1 = ConsumeUntilComparisonOrColon(range);

  // <mf-boolean> = <mf-name>
  if (range.AtEnd()) {
    String feature_name = ConsumeAllowedName(segment1, feature_set);
    if (feature_name.IsNull() || !segment1.AtEnd())
      return nullptr;
    auto exp = MediaQueryExp::Create(feature_name, range, fake_context_,
                                     execution_context_);
    if (!exp.IsValid())
      return nullptr;
    return std::make_unique<MediaQueryFeatureExpNode>(exp);
  }

  // <mf-plain> = <mf-name> : <mf-value>
  if (range.Peek().GetType() == kColonToken) {
    range.ConsumeIncludingWhitespace();
    if (range.AtEnd())
      return nullptr;
    String feature_name = ConsumeAllowedName(segment1, feature_set);
    if (feature_name.IsNull() || !segment1.AtEnd())
      return nullptr;
    auto exp = MediaQueryExp::Create(feature_name, range, fake_context_,
                                     execution_context_);
    if (!exp.IsValid() || !range.AtEnd())
      return nullptr;
    return std::make_unique<MediaQueryFeatureExpNode>(exp);
  }

  if (!IsMediaQueries4SyntaxEnabled())
    return nullptr;

  // Otherwise <mf-range>:
  //
  // <mf-range> = <mf-name> <mf-comparison> <mf-value>
  //            | <mf-value> <mf-comparison> <mf-name>
  //            | <mf-value> <mf-lt> <mf-name> <mf-lt> <mf-value>
  //            | <mf-value> <mf-gt> <mf-name> <mf-gt> <mf-value>

  MediaQueryOperator op1 = ConsumeComparison(range);
  DCHECK_NE(op1, MediaQueryOperator::kNone);

  CSSParserTokenRange segment2 = ConsumeUntilComparisonOrColon(range);

  // If the range ended, the feature must be on the following form:
  //
  //  <segment1> <op1> <segment2>
  //
  // We don't know which of <segment1> and <segment2> should be interpreted as
  // the <mf-name> and which should be interpreted as <mf-value>. We have to
  // try both.
  if (range.AtEnd()) {
    // Try: <mf-name> <mf-comparison> <mf-value>
    if (auto node = ParseNameValueComparison(
            segment1, op1, segment2, NameAffinity::kLeft, feature_set)) {
      return node;
    }

    // Otherwise: <mf-value> <mf-comparison> <mf-name>
    return ParseNameValueComparison(segment1, op1, segment2,
                                    NameAffinity::kRight, feature_set);
  }

  // Otherwise, the feature must be on the form:
  //
  // <segment1> <op1> <segment2> <op2> <segment3>
  //
  // This grammar is easier to deal with, since <mf-name> can only appear
  // at <segment2>.
  MediaQueryOperator op2 = ConsumeComparison(range);
  if (op2 == MediaQueryOperator::kNone)
    return nullptr;

  // Mixing [lt, le] and [gt, ge] is not allowed by the grammar.
  const bool both_lt_le = IsLtLe(op1) && IsLtLe(op2);
  const bool both_gt_ge = IsGtGe(op1) && IsGtGe(op2);
  if (!(both_lt_le || both_gt_ge))
    return nullptr;

  if (range.AtEnd())
    return nullptr;

  String feature_name = ConsumeUnprefixedName(segment2, feature_set);
  if (feature_name.IsNull() || !segment2.AtEnd())
    return nullptr;

  auto left_value = MediaQueryExpValue::Consume(
      feature_name, segment1, fake_context_, execution_context_);
  if (!left_value || !segment1.AtEnd())
    return nullptr;

  CSSParserTokenRange& segment3 = range;
  auto right_value = MediaQueryExpValue::Consume(
      feature_name, segment3, fake_context_, execution_context_);
  if (!right_value || !segment3.AtEnd())
    return nullptr;

  return std::make_unique<MediaQueryFeatureExpNode>(MediaQueryExp::Create(
      feature_name,
      MediaQueryExpBounds(MediaQueryExpComparison(*left_value, op1),
                          MediaQueryExpComparison(*right_value, op2))));
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
             mode == ConditionMode::kNormal && IsMediaQueries4SyntaxEnabled()) {
    while (result && ConsumeIfIdent(range, "or")) {
      result = MediaQueryExpNode::Or(std::move(result), ConsumeInParens(range));
    }
  }

  return result;
}

std::unique_ptr<MediaQueryExpNode> MediaQueryParser::ConsumeInParens(
    CSSParserTokenRange& range) {
  CSSParserTokenRange original_range = range;

  if (range.Peek().GetType() == kLeftParenthesisToken) {
    CSSParserTokenRange block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();

    CSSParserTokenRange original_block = block;

    // ( <media-condition> )
    if (IsMediaQueries4SyntaxEnabled()) {
      auto condition = ConsumeCondition(block);
      if (condition && block.AtEnd())
        return MediaQueryExpNode::Nested(std::move(condition));
    }
    block = original_block;

    // ( <media-feature> )
    auto feature = ConsumeFeature(block, MediaQueryFeatureSet(mode_));
    if (feature && block.AtEnd())
      return MediaQueryExpNode::Nested(std::move(feature));
  }
  range = original_range;

  // <general-enclosed>
  return ConsumeGeneralEnclosed(range);
}

std::unique_ptr<MediaQueryExpNode> MediaQueryParser::ConsumeGeneralEnclosed(
    CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kLeftParenthesisToken &&
      range.Peek().GetType() != kFunctionToken) {
    return nullptr;
  }

  const CSSParserToken* first = range.begin();

  CSSParserTokenRange block = range.ConsumeBlock();
  block.ConsumeWhitespace();

  // Note that <any-value> is optional in <general-enclosed>, so having an
  // empty block is fine.
  if (!block.AtEnd()) {
    if (!ConsumeAnyValue(block) || !block.AtEnd())
      return nullptr;
  }

  // TODO(crbug.com/962417): This is not well specified.
  String general_enclosed =
      range.MakeSubRange(first, range.begin()).Serialize();
  range.ConsumeWhitespace();
  return std::make_unique<MediaQueryUnknownExpNode>(general_enclosed);
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

bool MediaQueryParser::IsNotKeywordEnabled() const {
  // Support for 'not' was shipped for kMediaConditionParser before
  // RuntimeEnabledFeatures::CSSMediaQueries4 existed, hence it's always
  // enabled for that parser type.
  return (parser_type_ == kMediaConditionParser) ||
         IsMediaQueries4SyntaxEnabled();
}

bool MediaQueryParser::IsMediaQueries4SyntaxEnabled() const {
  switch (syntax_level_) {
    case SyntaxLevel::kAuto:
      return RuntimeEnabledFeatures::CSSMediaQueries4Enabled();
    case SyntaxLevel::kLevel4:
      return true;
  }
}

}  // namespace blink
