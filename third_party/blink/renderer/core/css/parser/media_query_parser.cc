// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"

#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using css_parsing_utils::AtIdent;
using css_parsing_utils::ConsumeAnyValue;
using css_parsing_utils::ConsumeIfDelimiter;
using css_parsing_utils::ConsumeIfIdent;

bool MediaQueryParser::MediaQueryFeatureSet::IsAllowed(
    const AtomicString& feature) const {
  if (feature == media_feature_names::kInlineSizeMediaFeature ||
      feature == media_feature_names::kMinInlineSizeMediaFeature ||
      feature == media_feature_names::kMaxInlineSizeMediaFeature ||
      feature == media_feature_names::kBlockSizeMediaFeature ||
      feature == media_feature_names::kMinBlockSizeMediaFeature ||
      feature == media_feature_names::kMaxBlockSizeMediaFeature ||
      feature == media_feature_names::kStuckMediaFeature ||
      feature == media_feature_names::kSnappedMediaFeature ||
      feature == media_feature_names::kScrollableMediaFeature ||
      (feature == media_feature_names::kScrolledMediaFeature &&
       RuntimeEnabledFeatures::CSSScrolledContainerQueriesEnabled()) ||
      (CSSVariableParser::IsValidVariableName(feature) &&
       !RuntimeEnabledFeatures::CSSCustomMediaEnabled())) {
    return false;
  }
  return true;
}

bool MediaQueryParser::MediaQueryFeatureSet::IsAllowedWithoutValue(
    const AtomicString& feature,
    const ExecutionContext* execution_context) const {
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
         feature == media_feature_names::kOverflowInlineMediaFeature ||
         feature == media_feature_names::kOverflowBlockMediaFeature ||
         feature == media_feature_names::kUpdateMediaFeature ||
         (feature == media_feature_names::kPrefersReducedDataMediaFeature &&
          RuntimeEnabledFeatures::PrefersReducedDataEnabled()) ||
         feature ==
             media_feature_names::kPrefersReducedTransparencyMediaFeature ||
         (feature == media_feature_names::kForcedColorsMediaFeature &&
          RuntimeEnabledFeatures::ForcedColorsEnabled()) ||
         (feature == media_feature_names::kNavigationControlsMediaFeature &&
          RuntimeEnabledFeatures::MediaQueryNavigationControlsEnabled()) ||
         (feature == media_feature_names::kOriginTrialTestMediaFeature &&
          RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabled(
              execution_context)) ||
         (feature ==
              media_feature_names::kHorizontalViewportSegmentsMediaFeature &&
          RuntimeEnabledFeatures::ViewportSegmentsEnabled(execution_context)) ||
         (feature ==
              media_feature_names::kVerticalViewportSegmentsMediaFeature &&
          RuntimeEnabledFeatures::ViewportSegmentsEnabled(execution_context)) ||
         (feature == media_feature_names::kDevicePostureMediaFeature &&
          RuntimeEnabledFeatures::DevicePostureEnabled(execution_context)) ||
         (feature == media_feature_names::kInvertedColorsMediaFeature &&
          RuntimeEnabledFeatures::InvertedColorsEnabled()) ||
         CSSVariableParser::IsValidVariableName(feature) ||
         feature == media_feature_names::kScriptingMediaFeature ||
         (RuntimeEnabledFeatures::
              DesktopPWAsAdditionalWindowingControlsEnabled() &&
          feature == media_feature_names::kDisplayStateMediaFeature) ||
         (RuntimeEnabledFeatures::
              DesktopPWAsAdditionalWindowingControlsEnabled() &&
          feature == media_feature_names::kResizableMediaFeature);
}

bool MediaQueryParser::MediaQueryFeatureSet::IsAllowedWithValue(
    const AtomicString& feature) const {
  return (!RuntimeEnabledFeatures::CSSCustomMediaEnabled() ||
          !CSSVariableParser::IsValidVariableName(feature));
}

MediaQuerySet* MediaQueryParser::ParseMediaQuerySet(
    StringView query_string,
    ExecutionContext* execution_context) {
  CSSParserTokenStream stream(query_string);
  return ParseMediaQuerySet(stream, execution_context);
}

MediaQuerySet* MediaQueryParser::ParseMediaQuerySet(
    CSSParserTokenStream& stream,
    ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaQuerySetParser, execution_context)
      .ParseImpl(stream);
}

MediaQuerySet* MediaQueryParser::ParseMediaCondition(
    CSSParserTokenStream& stream,
    ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaConditionParser, execution_context)
      .ParseImpl(stream);
}

MediaQuerySet* MediaQueryParser::ParseCustomMediaDefinition(
    CSSParserTokenStream& stream,
    ExecutionContext* execution_context) {
  CSSParserTokenStream::Boundary boundary(stream, kSemicolonToken);
  return ParseMediaQuerySet(stream, execution_context);
}

MediaQueryParser::MediaQueryParser(ParserType parser_type,
                                   ExecutionContext* execution_context)
    : parser_type_(parser_type),
      execution_context_(execution_context),
      fake_context_(*MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode,
          SecureContextMode::kInsecureContext,
          DynamicTo<LocalDOMWindow>(execution_context)
              ? DynamicTo<LocalDOMWindow>(execution_context)->document()
              : nullptr)) {}

namespace {

bool IsRestrictorOrLogicalOperator(const CSSParserToken& token) {
  // FIXME: it would be more efficient to use lower-case always for tokenValue.
  return EqualIgnoringASCIICase(token.Value(), "not") ||
         EqualIgnoringASCIICase(token.Value(), "and") ||
         EqualIgnoringASCIICase(token.Value(), "or") ||
         EqualIgnoringASCIICase(token.Value(), "only") ||
         EqualIgnoringASCIICase(token.Value(), "layer");
}

bool ConsumeUntilCommaInclusive(CSSParserTokenStream& stream) {
  stream.SkipUntilPeekedTypeIs<kCommaToken>();
  if (stream.Peek().GetType() == kCommaToken) {
    stream.ConsumeIncludingWhitespace();
    return true;
  } else {
    return false;
  }
}

bool IsComparisonDelimiter(UChar c) {
  return c == '<' || c == '>' || c == '=';
}

void SkipUntilComparisonOrColon(CSSParserTokenStream& stream) {
  while (!stream.AtEnd()) {
    stream.SkipUntilPeekedTypeIs<kDelimiterToken, kColonToken>();
    if (stream.AtEnd()) {
      return;
    }
    const CSSParserToken& token = stream.Peek();
    if (token.GetType() == kDelimiterToken) {
      if (IsComparisonDelimiter(token.Delimiter())) {
        return;
      } else {
        stream.Consume();
      }
    } else {
      DCHECK_EQ(token.GetType(), kColonToken);
      return;
    }
  }
}

bool IsLtLe(MediaQueryOperator op) {
  return op == MediaQueryOperator::kLt || op == MediaQueryOperator::kLe;
}

bool IsGtGe(MediaQueryOperator op) {
  return op == MediaQueryOperator::kGt || op == MediaQueryOperator::kGe;
}

// Consume a MediaQueryExpValue without parsing against the feature grammar.
// Only used for container style queries for range syntax.
std::optional<MediaQueryExpValue> ConsumeUnparsed(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  wtf_size_t start = stream.Offset();
  // Skip until the first comparison delimiter.
  while (!stream.AtEnd()) {
    stream.SkipUntilPeekedTypeIs<kDelimiterToken>();
    if (stream.AtEnd()) {
      break;
    }
    if (IsComparisonDelimiter(stream.Peek().Delimiter())) {
      break;
    }
    if (!stream.AtEnd()) {
      stream.Consume();  // kDelimiterToken
    }
  }
  wtf_size_t end = stream.Offset();
  StringView value_string(stream.StringRangeAt(start, end - start));
  if (value_string.empty()) {
    return std::nullopt;
  }
  const CSSValue* value = CSSVariableParser::ParseDeclarationValue(
      value_string, /* is_animation_tainted = */ false, context);
  if (!value) {
    return std::nullopt;
  }
  return MediaQueryExpValue(*value);
}

}  // namespace

MediaQuery::RestrictorType MediaQueryParser::ConsumeRestrictor(
    CSSParserTokenStream& stream) {
  if (ConsumeIfIdent(stream, "not")) {
    return MediaQuery::RestrictorType::kNot;
  }
  if (ConsumeIfIdent(stream, "only")) {
    return MediaQuery::RestrictorType::kOnly;
  }
  return MediaQuery::RestrictorType::kNone;
}

AtomicString MediaQueryParser::ConsumeType(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kIdentToken) {
    return g_null_atom;
  }
  if (IsRestrictorOrLogicalOperator(stream.Peek())) {
    return g_null_atom;
  }
  return stream.ConsumeIncludingWhitespace().Value().ToAtomicString();
}

MediaQueryOperator MediaQueryParser::ConsumeComparison(
    CSSParserTokenStream& stream) {
  const CSSParserToken& first = stream.Peek();
  if (first.GetType() != kDelimiterToken ||
      !IsComparisonDelimiter(first.Delimiter())) {
    return MediaQueryOperator::kNone;
  }
  switch (first.Delimiter()) {
    case '=':
      stream.ConsumeIncludingWhitespace();
      return MediaQueryOperator::kEq;
    case '<':
      stream.Consume();
      if (ConsumeIfDelimiter(stream, '=')) {
        return MediaQueryOperator::kLe;
      }
      stream.ConsumeWhitespace();
      return MediaQueryOperator::kLt;
    case '>':
      stream.Consume();
      if (ConsumeIfDelimiter(stream, '=')) {
        return MediaQueryOperator::kGe;
      }
      stream.ConsumeWhitespace();
      return MediaQueryOperator::kGt;
  }

  NOTREACHED();
}

AtomicString MediaQueryParser::ConsumeAllowedName(
    CSSParserTokenStream& stream,
    const FeatureSet& feature_set) {
  if (stream.Peek().GetType() != kIdentToken) {
    return g_null_atom;
  }
  AtomicString name = stream.Peek().Value().ToAtomicString();
  if (!feature_set.IsCaseSensitive(name)) {
    name = name.LowerASCII();
  }
  if (!feature_set.IsAllowed(name)) {
    return g_null_atom;
  }
  stream.ConsumeIncludingWhitespace();
  return name;
}

AtomicString MediaQueryParser::ConsumeRangeContextFeatureName(
    CSSParserTokenStream& stream,
    const FeatureSet& feature_set) {
  AtomicString name = ConsumeAllowedName(stream, feature_set);
  if (name.IsNull()) {
    return name;
  }
  if (name.StartsWith("min-") || name.StartsWith("max-") ||
      name.StartsWith("--")) {
    return g_null_atom;
  }
  return name;
}

// <style-range> = <unparsed> <mf-comparison> <unparsed>
//               | <unparsed> <mf-lt> <unparsed> <mf-lt> <unparsed>
//               | <unparsed> <mf-gt> <unparsed> <mf-gt> <unparsed>
//
// Where <unparsed> is a <declaration-value> that does not allow
// any of the delimiters accepted by <mf-lt> or <mf-gt>.
const ConditionalExpNode* MediaQueryParser::ConsumeStyleFeatureRange(
    CSSParserTokenStream& stream) {
  CSSParserTokenStream::State start = stream.Save();
  std::optional<MediaQueryExpValue> value1 =
      ConsumeUnparsed(stream, fake_context_);
  if (!value1.has_value() || stream.AtEnd()) {
    stream.Restore(start);
    return nullptr;
  }

  MediaQueryOperator op1 = ConsumeComparison(stream);
  if (op1 == MediaQueryOperator::kNone) {
    stream.Restore(start);
    return nullptr;
  }

  std::optional<MediaQueryExpValue> value2 =
      ConsumeUnparsed(stream, fake_context_);
  if (!value2.has_value()) {
    stream.Restore(start);
    return nullptr;
  }

  if (stream.AtEnd()) {
    MediaQueryExpComparison left(*value1, op1);
    MediaQueryExpComparison right;
    return MakeGarbageCollected<MediaQueryFeatureExpNode>(MediaQueryExp::Create(
        value2.value(), MediaQueryExpBounds(left, right)));
  }

  MediaQueryOperator op2 = ConsumeComparison(stream);
  if (op2 == MediaQueryOperator::kNone ||
      std::abs(static_cast<int>(op2) - static_cast<int>(op1)) > 1) {
    stream.Restore(start);
    return nullptr;
  }

  std::optional<MediaQueryExpValue> value3 =
      ConsumeUnparsed(stream, fake_context_);
  if (!value3.has_value() || !stream.AtEnd()) {
    stream.Restore(start);
    return nullptr;
  }

  MediaQueryExpComparison left(*value1, op1);
  MediaQueryExpComparison right(*value3, op2);
  return MakeGarbageCollected<MediaQueryFeatureExpNode>(
      MediaQueryExp::Create(value2.value(), MediaQueryExpBounds(left, right)));
}

const ConditionalExpNode* MediaQueryParser::ConsumeFeature(
    CSSParserTokenStream& stream,
    const FeatureSet& feature_set) {
  // There are several possible grammars for media queries, and we don't
  // know where <mf-name> appears. Thus, our only strategy is to just try them
  // one by one and restart if we got it wrong.
  //

  CSSParserTokenStream::State start = stream.Save();

  {
    AtomicString feature_name = ConsumeAllowedName(stream, feature_set);

    // <mf-boolean> = <mf-name>
    if (!feature_name.IsNull() && stream.AtEnd() &&
        feature_set.IsAllowedWithoutValue(feature_name, execution_context_)) {
      if (RuntimeEnabledFeatures::CSSCustomMediaEnabled() &&
          CSSVariableParser::IsValidVariableName(feature_name) &&
          !feature_set.IsAllowedWithValue(feature_name)) {
        // custom media query
        return MakeGarbageCollected<MediaQueryFeatureExpNode>(
            MediaQueryExp::Create(feature_name));
      }
      return MakeGarbageCollected<MediaQueryFeatureExpNode>(
          MediaQueryExp::Create(feature_name, MediaQueryExpBounds()));
    }

    // <mf-plain> = <mf-name> : <mf-value>
    if (!feature_name.IsNull() && stream.Peek().GetType() == kColonToken &&
        feature_set.IsAllowedWithValue(feature_name)) {
      stream.ConsumeIncludingWhitespace();

      // NOTE: We do not check for stream.AtEnd() here, as an empty mf-value is
      // legal.
      auto exp = MediaQueryExp::Create(feature_name, stream, fake_context_,
                                       feature_set.SupportsElementDependent());
      if (exp.IsValid() && stream.AtEnd()) {
        return MakeGarbageCollected<MediaQueryFeatureExpNode>(exp);
      }
    }

    stream.Restore(start);
  }

  if (feature_set.SupportsStyleRange() &&
      RuntimeEnabledFeatures::CSSContainerStyleQueriesRangeEnabled()) {
    // A feature set must either support regular ranges *or* style ranges.
    CHECK(!feature_set.SupportsRange());
    return ConsumeStyleFeatureRange(stream);
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

  {
    // Try: <mf-name> <mf-comparison> <mf-value> (e.g., “width <= 10px”)
    AtomicString feature_name =
        ConsumeRangeContextFeatureName(stream, feature_set);
    if (!feature_name.IsNull() && !stream.AtEnd()) {
      MediaQueryOperator op = ConsumeComparison(stream);
      if (op != MediaQueryOperator::kNone) {
        auto value =
            MediaQueryExpValue::Consume(feature_name, stream, fake_context_,
                                        feature_set.SupportsElementDependent());
        if (value && stream.AtEnd()) {
          auto left = MediaQueryExpComparison();
          auto right = MediaQueryExpComparison(*value, op);

          UseCountRangeSyntax();
          return MakeGarbageCollected<MediaQueryFeatureExpNode>(
              MediaQueryExp::Create(feature_name,
                                    MediaQueryExpBounds(left, right)));
        }
      }
    }
    stream.Restore(start);
  }

  // It must be one of these three:
  //
  // <mf-value> <mf-comparison> <mf-name>  (e.g., “10px = width”)
  // <mf-value> <mf-lt> <mf-name> <mf-lt> <mf-value>
  // <mf-value> <mf-gt> <mf-name> <mf-gt> <mf-value>
  //
  // We don't know how to parse <mf-value> yet, so we need to skip it
  // and parse <mf-name> first, then return to (the first) <mf-value>
  // afterwards.
  //
  // Local variables names from here on are chosen with the expectation
  // that we are heading towards the most complicated form of <mf-range>
  // (the latter in the list), which corresponds to the local variables:
  //
  //  <value1> <op1> <feature_name> <op2> <value2>
  SkipUntilComparisonOrColon(stream);
  if (stream.AtEnd()) {
    return nullptr;
  }
  wtf_size_t offset_after_value1 = stream.LookAheadOffset();

  MediaQueryOperator op1 = ConsumeComparison(stream);
  if (op1 == MediaQueryOperator::kNone) {
    return nullptr;
  }

  AtomicString feature_name =
      ConsumeRangeContextFeatureName(stream, feature_set);
  if (feature_name.IsNull()) {
    return nullptr;
  }

  stream.ConsumeWhitespace();
  CSSParserTokenStream::State after_feature_name = stream.Save();

  stream.Restore(start);
  auto value1 =
      MediaQueryExpValue::Consume(feature_name, stream, fake_context_,
                                  feature_set.SupportsElementDependent());
  if (!value1) {
    return nullptr;
  }

  if (stream.LookAheadOffset() != offset_after_value1) {
    // There was junk between <value1> and <op1>.
    return nullptr;
  }

  // Skip over the comparison and name again.
  stream.Restore(after_feature_name);

  if (stream.AtEnd()) {
    // Must be: <mf-value> <mf-comparison> <mf-name>
    auto left = MediaQueryExpComparison(*value1, op1);
    auto right = MediaQueryExpComparison();

    UseCountRangeSyntax();
    return MakeGarbageCollected<MediaQueryFeatureExpNode>(
        MediaQueryExp::Create(feature_name, MediaQueryExpBounds(left, right)));
  }

  // Parse the last <mf-value>.
  MediaQueryOperator op2 = ConsumeComparison(stream);
  if (op2 == MediaQueryOperator::kNone) {
    return nullptr;
  }

  // Mixing [lt, le] and [gt, ge] is not allowed by the grammar.
  const bool both_lt_le = IsLtLe(op1) && IsLtLe(op2);
  const bool both_gt_ge = IsGtGe(op1) && IsGtGe(op2);
  if (!(both_lt_le || both_gt_ge)) {
    return nullptr;
  }

  auto value2 =
      MediaQueryExpValue::Consume(feature_name, stream, fake_context_,
                                  feature_set.SupportsElementDependent());
  if (!value2) {
    return nullptr;
  }

  UseCountRangeSyntax();
  return MakeGarbageCollected<MediaQueryFeatureExpNode>(MediaQueryExp::Create(
      feature_name,
      MediaQueryExpBounds(MediaQueryExpComparison(*value1, op1),
                          MediaQueryExpComparison(*value2, op2))));
}

const ConditionalExpNode* MediaQueryParser::ConsumeLeaf(
    CSSParserTokenStream& stream) {
  stream.ConsumeWhitespace();
  // ( <media-feature> )
  if (const ConditionalExpNode* feature =
          ConsumeFeature(stream, MediaQueryParser::MediaQueryFeatureSet())) {
    stream.ConsumeWhitespace();
    return feature;
  }

  return nullptr;
}

const ConditionalExpNode* MediaQueryParser::ConsumeFunction(
    CSSParserTokenStream&) {
  return nullptr;
}

MediaQuerySet* MediaQueryParser::ConsumeSingleCondition(
    CSSParserTokenStream& stream) {
  DCHECK_EQ(parser_type_, kMediaConditionParser);
  DCHECK(!stream.AtEnd());

  HeapVector<Member<const MediaQuery>> queries;
  const ConditionalExpNode* node = ConsumeCondition(stream);
  if (!node) {
    queries.push_back(MediaQuery::CreateNotAll());
  } else {
    queries.push_back(MakeGarbageCollected<MediaQuery>(
        MediaQuery::RestrictorType::kNone, media_type_names::kAll, node));
  }
  return MakeGarbageCollected<MediaQuerySet>(std::move(queries));
}

MediaQuery* MediaQueryParser::ConsumeQuery(CSSParserTokenStream& stream) {
  DCHECK_EQ(parser_type_, kMediaQuerySetParser);
  CSSParserTokenStream::State savepoint = stream.Save();

  // First try to parse following grammar:
  //
  // [ not | only ]? <media-type> [ and <media-condition-without-or> ]?
  MediaQuery::RestrictorType restrictor = ConsumeRestrictor(stream);
  AtomicString type = ConsumeType(stream);

  if (!type.IsNull()) {
    if (!ConsumeIfIdent(stream, "and")) {
      return MakeGarbageCollected<MediaQuery>(restrictor, type, nullptr);
    }
    if (const ConditionalExpNode* node =
            ConsumeCondition(stream, ParseMode::kWithoutOr)) {
      return MakeGarbageCollected<MediaQuery>(restrictor, type, node);
    }
    return nullptr;
  }
  stream.Restore(savepoint);

  // Otherwise, <media-condition>
  if (const ConditionalExpNode* node = ConsumeCondition(stream)) {
    return MakeGarbageCollected<MediaQuery>(MediaQuery::RestrictorType::kNone,
                                            media_type_names::kAll, node);
  }
  return nullptr;
}

MediaQuerySet* MediaQueryParser::ParseImpl(CSSParserTokenStream& stream) {
  stream.ConsumeWhitespace();

  // Note that we currently expect an empty input to evaluate to an empty
  // MediaQuerySet, rather than "not all".
  if (stream.AtEnd()) {
    return MakeGarbageCollected<MediaQuerySet>();
  }

  if (parser_type_ == kMediaConditionParser) {
    return ConsumeSingleCondition(stream);
  }

  DCHECK_EQ(parser_type_, kMediaQuerySetParser);

  HeapVector<Member<const MediaQuery>> queries;

  do {
    MediaQuery* query = ConsumeQuery(stream);
    bool ok =
        query && (stream.AtEnd() || stream.Peek().GetType() == kCommaToken);
    queries.push_back(ok ? query : MediaQuery::CreateNotAll());
  } while (!stream.AtEnd() && ConsumeUntilCommaInclusive(stream));

  return MakeGarbageCollected<MediaQuerySet>(std::move(queries));
}

void MediaQueryParser::UseCountRangeSyntax() {
  UseCounter::Count(execution_context_, WebFeature::kMediaQueryRangeSyntax);
}

}  // namespace blink
