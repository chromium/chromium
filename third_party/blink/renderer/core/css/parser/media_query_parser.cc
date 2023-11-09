// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"

#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
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
  MediaQueryFeatureSet() = default;

  bool IsAllowed(const String& feature) const override {
    if (feature == media_feature_names::kInlineSizeMediaFeature ||
        feature == media_feature_names::kMinInlineSizeMediaFeature ||
        feature == media_feature_names::kMaxInlineSizeMediaFeature ||
        feature == media_feature_names::kBlockSizeMediaFeature ||
        feature == media_feature_names::kMinBlockSizeMediaFeature ||
        feature == media_feature_names::kMaxBlockSizeMediaFeature ||
        feature == media_feature_names::kStuckMediaFeature ||
        feature == media_feature_names::kSnappedMediaFeature ||
        CSSVariableParser::IsValidVariableName(feature)) {
      return false;
    }
    return true;
  }
  bool IsAllowedWithoutValue(
      const String& feature,
      const ExecutionContext* execution_context) const override {
    // Media features that are prefixed by min/max cannot be used without a
    // value.
    return feature == media_feature_names::kMonochromeMediaFeature ||
           feature == media_feature_names::kColorMediaFeature ||
           feature == media_feature_names::kColorIndexMediaFeature ||
           feature == media_feature_names::kGridMediaFeature ||
           feature == media_feature_names::kHeightMediaFeature ||
           feature == media_feature_names::kWidthMediaFeature ||
           feature == media_feature_names::kBlockSizeMediaFeature ||
           feature == media_feature_names::kInlineSizeMediaFeature ||
           feature == media_feature_names::kDeviceHeightMediaFeature ||
           feature == media_feature_names::kDeviceWidthMediaFeature ||
           feature == media_feature_names::kOrientationMediaFeature ||
           feature == media_feature_names::kAspectRatioMediaFeature ||
           feature == media_feature_names::kDeviceAspectRatioMediaFeature ||
           feature == media_feature_names::kHoverMediaFeature ||
           feature == media_feature_names::kAnyHoverMediaFeature ||
           feature == media_feature_names::kTransform3dMediaFeature ||
           feature == media_feature_names::kPointerMediaFeature ||
           feature == media_feature_names::kAnyPointerMediaFeature ||
           feature == media_feature_names::kDevicePixelRatioMediaFeature ||
           feature == media_feature_names::kResolutionMediaFeature ||
           feature == media_feature_names::kDisplayModeMediaFeature ||
           feature == media_feature_names::kScanMediaFeature ||
           feature == media_feature_names::kColorGamutMediaFeature ||
           feature == media_feature_names::kPrefersColorSchemeMediaFeature ||
           feature == media_feature_names::kPrefersContrastMediaFeature ||
           feature == media_feature_names::kPrefersReducedMotionMediaFeature ||
           (feature == media_feature_names::kUpdateMediaFeature &&
            RuntimeEnabledFeatures::CSSUpdateMediaFeatureEnabled()) ||
           (feature == media_feature_names::kPrefersReducedDataMediaFeature &&
            RuntimeEnabledFeatures::PrefersReducedDataEnabled()) ||
           (feature ==
                media_feature_names::kPrefersReducedTransparencyMediaFeature &&
            RuntimeEnabledFeatures::PrefersReducedTransparencyEnabled()) ||
           (feature == media_feature_names::kForcedColorsMediaFeature &&
            RuntimeEnabledFeatures::ForcedColorsEnabled()) ||
           (feature == media_feature_names::kNavigationControlsMediaFeature &&
            RuntimeEnabledFeatures::MediaQueryNavigationControlsEnabled()) ||
           (feature == media_feature_names::kOriginTrialTestMediaFeature &&
            RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabled(
                execution_context)) ||
           (feature ==
                media_feature_names::kHorizontalViewportSegmentsMediaFeature &&
            RuntimeEnabledFeatures::ViewportSegmentsEnabled()) ||
           (feature ==
                media_feature_names::kVerticalViewportSegmentsMediaFeature &&
            RuntimeEnabledFeatures::ViewportSegmentsEnabled()) ||
           (feature == media_feature_names::kDevicePostureMediaFeature &&
            RuntimeEnabledFeatures::DevicePostureEnabled()) ||
           (feature == media_feature_names::kOverflowInlineMediaFeature &&
            RuntimeEnabledFeatures::CSSOverflowMediaFeaturesEnabled()) ||
           (feature == media_feature_names::kOverflowBlockMediaFeature &&
            RuntimeEnabledFeatures::CSSOverflowMediaFeaturesEnabled()) ||
           (feature == media_feature_names::kInvertedColorsMediaFeature &&
            RuntimeEnabledFeatures::InvertedColorsEnabled()) ||
           CSSVariableParser::IsValidVariableName(feature) ||
           (feature == media_feature_names::kScriptingMediaFeature &&
            RuntimeEnabledFeatures::ScriptingMediaFeatureEnabled()) ||
           (RuntimeEnabledFeatures::
                DesktopPWAsAdditionalWindowingControlsEnabled() &&
            feature == media_feature_names::kDisplayStateMediaFeature) ||
           (RuntimeEnabledFeatures::
                DesktopPWAsAdditionalWindowingControlsEnabled() &&
            feature == media_feature_names::kResizableMediaFeature);
  }

  bool IsCaseSensitive(const String& feature) const override { return false; }
  bool SupportsRange() const override { return true; }
};

}  // namespace

MediaQuerySet* MediaQueryParser::ParseMediaQuerySet(
    const String& query_string,
    const ExecutionContext* execution_context) {
  CSSTokenizer tokenizer(query_string);
  auto [tokens, raw_offsets] = tokenizer.TokenizeToEOFWithOffsets();
  CSSParserTokenRange range(tokens);
  CSSParserTokenOffsets offsets(tokens, std::move(raw_offsets), query_string);
  return ParseMediaQuerySet(range, offsets, execution_context);
}

MediaQuerySet* MediaQueryParser::ParseMediaQuerySet(
    CSSParserTokenRange range,
    const CSSParserTokenOffsets& offsets,
    const ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaQuerySetParser, kHTMLStandardMode,
                          execution_context)
      .ParseImpl(range, offsets);
}

MediaQuerySet* MediaQueryParser::ParseMediaQuerySetInMode(
    CSSParserTokenRange range,
    const CSSParserTokenOffsets& offsets,
    CSSParserMode mode,
    const ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaQuerySetParser, mode, execution_context)
      .ParseImpl(range, offsets);
}

MediaQuerySet* MediaQueryParser::ParseMediaCondition(
    CSSParserTokenRange range,
    const CSSParserTokenOffsets& offsets,
    const ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaConditionParser, kHTMLStandardMode,
                          execution_context)
      .ParseImpl(range, offsets);
}

MediaQueryParser::MediaQueryParser(ParserType parser_type,
                                   CSSParserMode mode,
                                   const ExecutionContext* execution_context,
                                   SyntaxLevel syntax_level)
    : parser_type_(parser_type),
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
         EqualIgnoringASCIICase(token.Value(), "only") ||
         EqualIgnoringASCIICase(token.Value(), "layer");
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
  if (ConsumeIfIdent(range, "not")) {
    return MediaQuery::RestrictorType::kNot;
  }
  if (ConsumeIfIdent(range, "only")) {
    return MediaQuery::RestrictorType::kOnly;
  }
  return MediaQuery::RestrictorType::kNone;
}

String MediaQueryParser::ConsumeType(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken) {
    return g_null_atom;
  }
  if (IsRestrictorOrLogicalOperator(range.Peek())) {
    return g_null_atom;
  }
  return range.ConsumeIncludingWhitespace().Value().ToString();
}

MediaQueryOperator MediaQueryParser::ConsumeComparison(
    CSSParserTokenRange& range) {
  const CSSParserToken& first = range.Peek();
  if (first.GetType() != kDelimiterToken) {
    return MediaQueryOperator::kNone;
  }
  DCHECK(IsComparisonDelimiter(first.Delimiter()));
  switch (first.Delimiter()) {
    case '=':
      range.ConsumeIncludingWhitespace();
      return MediaQueryOperator::kEq;
    case '<':
      range.Consume();
      if (ConsumeIfDelimiter(range, '=')) {
        return MediaQueryOperator::kLe;
      }
      range.ConsumeWhitespace();
      return MediaQueryOperator::kLt;
    case '>':
      range.Consume();
      if (ConsumeIfDelimiter(range, '=')) {
        return MediaQueryOperator::kGe;
      }
      range.ConsumeWhitespace();
      return MediaQueryOperator::kGt;
  }

  NOTREACHED();
  return MediaQueryOperator::kNone;
}

String MediaQueryParser::ConsumeAllowedName(CSSParserTokenRange& range,
                                            const FeatureSet& feature_set) {
  if (range.Peek().GetType() != kIdentToken) {
    return g_null_atom;
  }
  String name = range.Peek().Value().ToString();
  if (!feature_set.IsCaseSensitive(name)) {
    name = name.LowerASCII();
  }
  name = AttemptStaticStringCreation(name);
  if (!feature_set.IsAllowed(name)) {
    return g_null_atom;
  }
  range.ConsumeIncludingWhitespace();
  return name;
}

String MediaQueryParser::ConsumeUnprefixedName(CSSParserTokenRange& range,
                                               const FeatureSet& feature_set) {
  String name = ConsumeAllowedName(range, feature_set);
  if (name.IsNull()) {
    return name;
  }
  if (name.StartsWith("min-") || name.StartsWith("max-")) {
    return g_null_atom;
  }
  return name;
}

const MediaQueryExpNode* MediaQueryParser::ParseNameValueComparison(
    CSSParserTokenRange lhs,
    MediaQueryOperator op,
    CSSParserTokenRange rhs,
    const CSSParserTokenOffsets& offsets,
    NameAffinity name_affinity,
    const FeatureSet& feature_set) {
  if (name_affinity == NameAffinity::kRight) {
    std::swap(lhs, rhs);
  }

  String feature_name = ConsumeUnprefixedName(lhs, feature_set);
  if (feature_name.IsNull() || !lhs.AtEnd()) {
    return nullptr;
  }

  auto value =
      MediaQueryExpValue::Consume(feature_name, rhs, offsets, fake_context_);

  if (!value || !rhs.AtEnd()) {
    return nullptr;
  }

  auto left = MediaQueryExpComparison();
  auto right = MediaQueryExpComparison(*value, op);

  if (name_affinity == NameAffinity::kRight) {
    std::swap(left, right);
  }

  return MakeGarbageCollected<MediaQueryFeatureExpNode>(
      MediaQueryExp::Create(feature_name, MediaQueryExpBounds(left, right)));
}

const MediaQueryExpNode* MediaQueryParser::ConsumeFeature(
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets,
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
    if (feature_name.IsNull() || !segment1.AtEnd() ||
        !feature_set.IsAllowedWithoutValue(feature_name, execution_context_)) {
      return nullptr;
    }
    return MakeGarbageCollected<MediaQueryFeatureExpNode>(
        MediaQueryExp::Create(feature_name, MediaQueryExpBounds()));
  }

  // <mf-plain> = <mf-name> : <mf-value>
  if (range.Peek().GetType() == kColonToken) {
    range.ConsumeIncludingWhitespace();
    String feature_name = ConsumeAllowedName(segment1, feature_set);
    if (feature_name.IsNull() || !segment1.AtEnd()) {
      return nullptr;
    }
    auto exp =
        MediaQueryExp::Create(feature_name, range, offsets, fake_context_);
    if (!exp.IsValid() || !range.AtEnd()) {
      return nullptr;
    }
    return MakeGarbageCollected<MediaQueryFeatureExpNode>(exp);
  }

  if (!feature_set.SupportsRange()) {
    return nullptr;
  }

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
    if (const MediaQueryExpNode* node =
            ParseNameValueComparison(segment1, op1, segment2, offsets,
                                     NameAffinity::kLeft, feature_set)) {
      return node;
    }

    // Otherwise: <mf-value> <mf-comparison> <mf-name>
    return ParseNameValueComparison(segment1, op1, segment2, offsets,
                                    NameAffinity::kRight, feature_set);
  }

  // Otherwise, the feature must be on the form:
  //
  // <segment1> <op1> <segment2> <op2> <segment3>
  //
  // This grammar is easier to deal with, since <mf-name> can only appear
  // at <segment2>.
  MediaQueryOperator op2 = ConsumeComparison(range);
  if (op2 == MediaQueryOperator::kNone) {
    return nullptr;
  }

  // Mixing [lt, le] and [gt, ge] is not allowed by the grammar.
  const bool both_lt_le = IsLtLe(op1) && IsLtLe(op2);
  const bool both_gt_ge = IsGtGe(op1) && IsGtGe(op2);
  if (!(both_lt_le || both_gt_ge)) {
    return nullptr;
  }

  if (range.AtEnd()) {
    return nullptr;
  }

  String feature_name = ConsumeUnprefixedName(segment2, feature_set);
  if (feature_name.IsNull() || !segment2.AtEnd()) {
    return nullptr;
  }

  auto left_value = MediaQueryExpValue::Consume(feature_name, segment1, offsets,
                                                fake_context_);
  if (!left_value || !segment1.AtEnd()) {
    return nullptr;
  }

  CSSParserTokenRange& segment3 = range;
  auto right_value = MediaQueryExpValue::Consume(feature_name, segment3,
                                                 offsets, fake_context_);
  if (!right_value || !segment3.AtEnd()) {
    return nullptr;
  }

  return MakeGarbageCollected<MediaQueryFeatureExpNode>(MediaQueryExp::Create(
      feature_name,
      MediaQueryExpBounds(MediaQueryExpComparison(*left_value, op1),
                          MediaQueryExpComparison(*right_value, op2))));
}

const MediaQueryExpNode* MediaQueryParser::ConsumeCondition(
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets,
    ConditionMode mode) {
  // <media-not>
  if (ConsumeIfIdent(range, "not")) {
    return MediaQueryExpNode::Not(ConsumeInParens(range, offsets));
  }

  // Otherwise:
  // <media-in-parens> [ <media-and>* | <media-or>* ]

  const MediaQueryExpNode* result = ConsumeInParens(range, offsets);

  if (AtIdent(range.Peek(), "and")) {
    while (result && ConsumeIfIdent(range, "and")) {
      result = MediaQueryExpNode::And(result, ConsumeInParens(range, offsets));
    }
  } else if (result && AtIdent(range.Peek(), "or") &&
             mode == ConditionMode::kNormal) {
    while (result && ConsumeIfIdent(range, "or")) {
      result = MediaQueryExpNode::Or(result, ConsumeInParens(range, offsets));
    }
  }

  return result;
}

const MediaQueryExpNode* MediaQueryParser::ConsumeInParens(
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets) {
  CSSParserTokenRange original_range = range;

  if (range.Peek().GetType() == kLeftParenthesisToken) {
    CSSParserTokenRange block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();

    CSSParserTokenRange original_block = block;

    // ( <media-condition> )
    const MediaQueryExpNode* condition = ConsumeCondition(block, offsets);
    if (condition && block.AtEnd()) {
      return MediaQueryExpNode::Nested(condition);
    }
    block = original_block;

    // ( <media-feature> )
    const MediaQueryExpNode* feature =
        ConsumeFeature(block, offsets, MediaQueryFeatureSet());
    if (feature && block.AtEnd()) {
      return MediaQueryExpNode::Nested(feature);
    }
  }
  range = original_range;

  // <general-enclosed>
  return ConsumeGeneralEnclosed(range);
}

const MediaQueryExpNode* MediaQueryParser::ConsumeGeneralEnclosed(
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
    if (!ConsumeAnyValue(block) || !block.AtEnd()) {
      return nullptr;
    }
  }

  // TODO(crbug.com/962417): This is not well specified.
  String general_enclosed =
      range.MakeSubRange(first, range.begin()).Serialize();
  range.ConsumeWhitespace();
  return MakeGarbageCollected<MediaQueryUnknownExpNode>(general_enclosed);
}

MediaQuerySet* MediaQueryParser::ConsumeSingleCondition(
    CSSParserTokenRange range,
    const CSSParserTokenOffsets& offsets) {
  DCHECK_EQ(parser_type_, kMediaConditionParser);
  DCHECK(!range.AtEnd());

  const MediaQueryExpNode* node = ConsumeCondition(range, offsets);

  HeapVector<Member<const MediaQuery>> queries;

  if (!node || !range.AtEnd()) {
    queries.push_back(MediaQuery::CreateNotAll());
  } else {
    queries.push_back(MakeGarbageCollected<MediaQuery>(
        MediaQuery::RestrictorType::kNone, media_type_names::kAll, node));
  }

  return MakeGarbageCollected<MediaQuerySet>(std::move(queries));
}

MediaQuery* MediaQueryParser::ConsumeQuery(
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets) {
  DCHECK_EQ(parser_type_, kMediaQuerySetParser);
  CSSParserTokenRange original_range = range;

  // First try to parse following grammar:
  //
  // [ not | only ]? <media-type> [ and <media-condition-without-or> ]?
  MediaQuery::RestrictorType restrictor = ConsumeRestrictor(range);
  String type = ConsumeType(range);

  if (!type.IsNull()) {
    if (!ConsumeIfIdent(range, "and")) {
      return MakeGarbageCollected<MediaQuery>(restrictor, type, nullptr);
    }
    if (const MediaQueryExpNode* node =
            ConsumeCondition(range, offsets, ConditionMode::kWithoutOr)) {
      return MakeGarbageCollected<MediaQuery>(restrictor, type, node);
    }
    return nullptr;
  }
  range = original_range;

  // Otherwise, <media-condition>
  if (const MediaQueryExpNode* node = ConsumeCondition(range, offsets)) {
    return MakeGarbageCollected<MediaQuery>(MediaQuery::RestrictorType::kNone,
                                            media_type_names::kAll, node);
  }
  return nullptr;
}

MediaQuerySet* MediaQueryParser::ParseImpl(
    CSSParserTokenRange range,
    const CSSParserTokenOffsets& offsets) {
  range.ConsumeWhitespace();

  // Note that we currently expect an empty input to evaluate to an empty
  // MediaQuerySet, rather than "not all".
  if (range.AtEnd()) {
    return MakeGarbageCollected<MediaQuerySet>();
  }

  if (parser_type_ == kMediaConditionParser) {
    return ConsumeSingleCondition(range, offsets);
  }

  DCHECK_EQ(parser_type_, kMediaQuerySetParser);

  HeapVector<Member<const MediaQuery>> queries;

  do {
    MediaQuery* query = ConsumeQuery(range, offsets);
    bool ok = query && (range.AtEnd() || range.Peek().GetType() == kCommaToken);
    queries.push_back(ok ? query : MediaQuery::CreateNotAll());
  } while (!range.AtEnd() && ConsumeUntilCommaInclusive(range));

  return MakeGarbageCollected<MediaQuerySet>(std::move(queries));
}

}  // namespace blink
