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
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
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
            RuntimeEnabledFeatures::ViewportSegmentsEnabled(
                execution_context)) ||
           (feature ==
                media_feature_names::kVerticalViewportSegmentsMediaFeature &&
            RuntimeEnabledFeatures::ViewportSegmentsEnabled(
                execution_context)) ||
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

  bool IsCaseSensitive(const String& feature) const override { return false; }
  bool SupportsRange() const override { return true; }
};

}  // namespace

MediaQuerySet* MediaQueryParser::ParseMediaQuerySet(
    const String& query_string,
    const ExecutionContext* execution_context) {
  CSSParserTokenStream stream(query_string);
  return ParseMediaQuerySet(stream, execution_context);
}

MediaQuerySet* MediaQueryParser::ParseMediaQuerySet(
    CSSParserTokenStream& stream,
    const ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaQuerySetParser, kHTMLStandardMode,
                          execution_context)
      .ParseImpl(stream);
}

MediaQuerySet* MediaQueryParser::ParseMediaQuerySetInMode(
    CSSParserTokenStream& stream,
    CSSParserMode mode,
    const ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaQuerySetParser, mode, execution_context)
      .ParseImpl(stream);
}

MediaQuerySet* MediaQueryParser::ParseMediaCondition(
    CSSParserTokenStream& stream,
    const ExecutionContext* execution_context) {
  return MediaQueryParser(kMediaConditionParser, kHTMLStandardMode,
                          execution_context)
      .ParseImpl(stream);
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

String MediaQueryParser::ConsumeType(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kIdentToken) {
    return g_null_atom;
  }
  if (IsRestrictorOrLogicalOperator(stream.Peek())) {
    return g_null_atom;
  }
  return stream.ConsumeIncludingWhitespace().Value().ToString();
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

  NOTREACHED_IN_MIGRATION();
  return MediaQueryOperator::kNone;
}

String MediaQueryParser::ConsumeAllowedName(CSSParserTokenStream& stream,
                                            const FeatureSet& feature_set) {
  if (stream.Peek().GetType() != kIdentToken) {
    return g_null_atom;
  }
  String name = stream.Peek().Value().ToString();
  if (!feature_set.IsCaseSensitive(name)) {
    name = name.LowerASCII();
  }
  name = AttemptStaticStringCreation(name);
  if (!feature_set.IsAllowed(name)) {
    return g_null_atom;
  }
  stream.ConsumeIncludingWhitespace();
  return name;
}

String MediaQueryParser::ConsumeUnprefixedName(CSSParserTokenStream& stream,
                                               const FeatureSet& feature_set) {
  String name = ConsumeAllowedName(stream, feature_set);
  if (name.IsNull()) {
    return name;
  }
  if (name.StartsWith("min-") || name.StartsWith("max-")) {
    return g_null_atom;
  }
  return name;
}

const MediaQueryExpNode* MediaQueryParser::ConsumeFeature(
    CSSParserTokenStream& stream,
    const FeatureSet& feature_set) {
  // There are several possible grammars for media queries, and we don't
  // know where <mf-name> appears. Thus, our only strategy is to just try them
  // one by one and restart if we got it wrong.
  //

  CSSParserTokenStream::State start = stream.Save();

  {
    String feature_name = ConsumeAllowedName(stream, feature_set);

    // <mf-boolean> = <mf-name>
    if (!feature_name.IsNull() && stream.AtEnd() &&
        feature_set.IsAllowedWithoutValue(feature_name, execution_context_)) {
      return MakeGarbageCollected<MediaQueryFeatureExpNode>(
          MediaQueryExp::Create(feature_name, MediaQueryExpBounds()));
    }

    // <mf-plain> = <mf-name> : <mf-value>
    if (!feature_name.IsNull() && stream.Peek().GetType() == kColonToken) {
      stream.ConsumeIncludingWhitespace();

      // NOTE: We do not check for stream.AtEnd() here, as an empty mf-value is
      // legal.
      auto exp = MediaQueryExp::Create(feature_name, stream, fake_context_);
      if (exp.IsValid() && stream.AtEnd()) {
        return MakeGarbageCollected<MediaQueryFeatureExpNode>(exp);
      }
    }

    stream.Restore(start);
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
    String feature_name = ConsumeUnprefixedName(stream, feature_set);
    if (!feature_name.IsNull() && !stream.AtEnd()) {
      MediaQueryOperator op = ConsumeComparison(stream);
      if (op != MediaQueryOperator::kNone) {
        auto value =
            MediaQueryExpValue::Consume(feature_name, stream, fake_context_);
        if (value && stream.AtEnd()) {
          auto left = MediaQueryExpComparison();
          auto right = MediaQueryExpComparison(*value, op);

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

  String feature_name = ConsumeUnprefixedName(stream, feature_set);
  if (feature_name.IsNull()) {
    return nullptr;
  }

  stream.ConsumeWhitespace();
  CSSParserTokenStream::State after_feature_name = stream.Save();

  stream.Restore(start);
  auto value1 =
      MediaQueryExpValue::Consume(feature_name, stream, fake_context_);
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
      MediaQueryExpValue::Consume(feature_name, stream, fake_context_);
  if (!value2) {
    return nullptr;
  }

  return MakeGarbageCollected<MediaQueryFeatureExpNode>(MediaQueryExp::Create(
      feature_name,
      MediaQueryExpBounds(MediaQueryExpComparison(*value1, op1),
                          MediaQueryExpComparison(*value2, op2))));
}

const MediaQueryExpNode* MediaQueryParser::ConsumeCondition(
    CSSParserTokenStream& stream,
    ConditionMode mode) {
  // <media-not>
  if (ConsumeIfIdent(stream, "not")) {
    return MediaQueryExpNode::Not(ConsumeInParens(stream));
  }

  // Otherwise:
  // <media-in-parens> [ <media-and>* | <media-or>* ]

  const MediaQueryExpNode* result = ConsumeInParens(stream);

  if (AtIdent(stream.Peek(), "and")) {
    while (result && ConsumeIfIdent(stream, "and")) {
      result = MediaQueryExpNode::And(result, ConsumeInParens(stream));
    }
  } else if (result && AtIdent(stream.Peek(), "or") &&
             mode == ConditionMode::kNormal) {
    while (result && ConsumeIfIdent(stream, "or")) {
      result = MediaQueryExpNode::Or(result, ConsumeInParens(stream));
    }
  }

  return result;
}

const MediaQueryExpNode* MediaQueryParser::ConsumeInParens(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kLeftParenthesisToken) {
    {
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      stream.ConsumeWhitespace();

      // ( <media-condition> )
      const MediaQueryExpNode* condition = ConsumeCondition(stream);
      if (condition && guard.Release()) {
        stream.ConsumeWhitespace();
        return MediaQueryExpNode::Nested(condition);
      }
    }

    {
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      stream.ConsumeWhitespace();
      // ( <media-feature> )
      const MediaQueryExpNode* feature =
          ConsumeFeature(stream, MediaQueryFeatureSet());
      if (feature && guard.Release()) {
        stream.ConsumeWhitespace();
        return MediaQueryExpNode::Nested(feature);
      }
    }
  }

  // <general-enclosed>
  return ConsumeGeneralEnclosed(stream);
}

const MediaQueryExpNode* MediaQueryParser::ConsumeGeneralEnclosed(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kLeftParenthesisToken &&
      stream.Peek().GetType() != kFunctionToken) {
    return nullptr;
  }

  wtf_size_t start_offset = stream.Offset();
  StringView general_enclosed;
  {
    CSSParserTokenStream::BlockGuard guard(stream);

    stream.ConsumeWhitespace();

    // Note that <any-value> is optional in <general-enclosed>, so having an
    // empty block is fine.
    ConsumeAnyValue(stream);
    if (!stream.AtEnd()) {
      return nullptr;
    }
  }

  wtf_size_t end_offset = stream.Offset();

  // TODO(crbug.com/962417): This is not well specified.
  general_enclosed =
      stream.StringRangeAt(start_offset, end_offset - start_offset);

  stream.ConsumeWhitespace();
  return MakeGarbageCollected<MediaQueryUnknownExpNode>(
      general_enclosed.ToString());
}

MediaQuerySet* MediaQueryParser::ConsumeSingleCondition(
    CSSParserTokenStream& stream) {
  DCHECK_EQ(parser_type_, kMediaConditionParser);
  DCHECK(!stream.AtEnd());

  HeapVector<Member<const MediaQuery>> queries;
  const MediaQueryExpNode* node = ConsumeCondition(stream);
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
  String type = ConsumeType(stream);

  if (!type.IsNull()) {
    if (!ConsumeIfIdent(stream, "and")) {
      return MakeGarbageCollected<MediaQuery>(restrictor, type, nullptr);
    }
    if (const MediaQueryExpNode* node =
            ConsumeCondition(stream, ConditionMode::kWithoutOr)) {
      return MakeGarbageCollected<MediaQuery>(restrictor, type, node);
    }
    return nullptr;
  }
  stream.Restore(savepoint);

  // Otherwise, <media-condition>
  if (const MediaQueryExpNode* node = ConsumeCondition(stream)) {
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

}  // namespace blink
