// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"

namespace blink {

using css_parsing_utils::AtIdent;
using css_parsing_utils::ConsumeIfIdent;

namespace {

// not <func> | <func> [ and <func> ]* | <func> [ or <func> ]*
//
// For example, if <func> is a function that can parse <container-query>,
// then ConsumeNotAndOr can be used to parse <container-condition>:
//
// https://drafts.csswg.org/css-contain-3/#typedef-container-condition
template <typename Func>
const MediaQueryExpNode* ConsumeNotAndOr(Func func,
                                         CSSParserTokenStream& stream) {
  if (ConsumeIfIdent(stream, "not")) {
    return MediaQueryExpNode::Not(func(stream));
  }

  const MediaQueryExpNode* result = func(stream);

  if (AtIdent(stream.Peek(), "and")) {
    while (result && ConsumeIfIdent(stream, "and")) {
      result = MediaQueryExpNode::And(result, func(stream));
    }
  } else if (AtIdent(stream.Peek(), "or")) {
    while (ConsumeIfIdent(stream, "or")) {
      result = MediaQueryExpNode::Or(result, func(stream));
    }
  }

  return result;
}

class SizeFeatureSet : public MediaQueryParser::FeatureSet {
  STACK_ALLOCATED();

 public:
  bool IsAllowed(const String& feature) const override {
    return feature == media_feature_names::kWidthMediaFeature ||
           feature == media_feature_names::kMinWidthMediaFeature ||
           feature == media_feature_names::kMaxWidthMediaFeature ||
           feature == media_feature_names::kHeightMediaFeature ||
           feature == media_feature_names::kMinHeightMediaFeature ||
           feature == media_feature_names::kMaxHeightMediaFeature ||
           feature == media_feature_names::kInlineSizeMediaFeature ||
           feature == media_feature_names::kMinInlineSizeMediaFeature ||
           feature == media_feature_names::kMaxInlineSizeMediaFeature ||
           feature == media_feature_names::kBlockSizeMediaFeature ||
           feature == media_feature_names::kMinBlockSizeMediaFeature ||
           feature == media_feature_names::kMaxBlockSizeMediaFeature ||
           feature == media_feature_names::kAspectRatioMediaFeature ||
           feature == media_feature_names::kMinAspectRatioMediaFeature ||
           feature == media_feature_names::kMaxAspectRatioMediaFeature ||
           feature == media_feature_names::kOrientationMediaFeature;
  }
  bool IsAllowedWithoutValue(const String& feature,
                             const ExecutionContext*) const override {
    return feature == media_feature_names::kWidthMediaFeature ||
           feature == media_feature_names::kHeightMediaFeature ||
           feature == media_feature_names::kInlineSizeMediaFeature ||
           feature == media_feature_names::kBlockSizeMediaFeature ||
           feature == media_feature_names::kAspectRatioMediaFeature ||
           feature == media_feature_names::kOrientationMediaFeature;
  }
  bool IsCaseSensitive(const String& feature) const override { return false; }
  bool SupportsRange() const override { return true; }
};

class StyleFeatureSet : public MediaQueryParser::FeatureSet {
  STACK_ALLOCATED();

 public:
  bool IsAllowed(const String& feature) const override {
    // TODO(crbug.com/1302630): Only support querying custom properties for now.
    return CSSVariableParser::IsValidVariableName(feature);
  }
  bool IsAllowedWithoutValue(const String& feature,
                             const ExecutionContext*) const override {
    return true;
  }
  bool IsCaseSensitive(const String& feature) const override {
    // TODO(crbug.com/1302630): non-custom properties are case-insensitive.
    return true;
  }
  bool SupportsRange() const override { return false; }
};

class StateFeatureSet : public MediaQueryParser::FeatureSet {
  STACK_ALLOCATED();

 public:
  bool IsAllowed(const String& feature) const override {
    return (RuntimeEnabledFeatures::CSSStickyContainerQueriesEnabled() &&
            feature == media_feature_names::kStuckMediaFeature) ||
           (RuntimeEnabledFeatures::CSSSnapContainerQueriesEnabled() &&
            feature == media_feature_names::kSnappedMediaFeature);
  }
  bool IsAllowedWithoutValue(const String& feature,
                             const ExecutionContext*) const override {
    return true;
  }
  bool IsCaseSensitive(const String& feature) const override { return false; }
  bool SupportsRange() const override { return false; }
};

}  // namespace

ContainerQueryParser::ContainerQueryParser(const CSSParserContext& context)
    : context_(context),
      media_query_parser_(MediaQueryParser::kMediaQuerySetParser,
                          kHTMLStandardMode,
                          context.GetExecutionContext(),
                          MediaQueryParser::SyntaxLevel::kLevel4) {}

const MediaQueryExpNode* ContainerQueryParser::ParseCondition(String value) {
  CSSParserTokenStream stream(value);
  const MediaQueryExpNode* node = ParseCondition(stream);
  if (!stream.AtEnd()) {
    return nullptr;
  }
  return node;
}

const MediaQueryExpNode* ContainerQueryParser::ParseCondition(
    CSSParserTokenStream& stream) {
  stream.ConsumeWhitespace();
  return ConsumeContainerCondition(stream);
}

// <query-in-parens> = ( <container-condition> )
//                   | ( <size-feature> )
//                   | style( <style-query> )
//                   | <general-enclosed>
const MediaQueryExpNode* ContainerQueryParser::ConsumeQueryInParens(
    CSSParserTokenStream& stream) {
  CSSParserTokenStream::State savepoint = stream.Save();

  if (stream.Peek().GetType() == kLeftParenthesisToken) {
    // ( <size-feature> ) | ( <container-condition> )
    {
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      stream.ConsumeWhitespace();
      // <size-feature>
      const MediaQueryExpNode* query = ConsumeFeature(stream, SizeFeatureSet());
      if (query && stream.AtEnd()) {
        guard.Release();
        stream.ConsumeWhitespace();
        return MediaQueryExpNode::Nested(query);
      }
    }

    {
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      stream.ConsumeWhitespace();
      // <container-condition>
      const MediaQueryExpNode* condition = ConsumeContainerCondition(stream);
      if (condition && stream.AtEnd()) {
        guard.Release();
        stream.ConsumeWhitespace();
        return MediaQueryExpNode::Nested(condition);
      }
    }
  } else if (stream.Peek().GetType() == kFunctionToken &&
             stream.Peek().FunctionId() == CSSValueID::kStyle) {
    // style( <style-query> )
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    if (const MediaQueryExpNode* query =
            ConsumeFeatureQuery(stream, StyleFeatureSet())) {
      context_.Count(WebFeature::kCSSStyleContainerQuery);
      guard.Release();
      stream.ConsumeWhitespace();
      return MediaQueryExpNode::Function(query, AtomicString("style"));
    }
  } else if (RuntimeEnabledFeatures::CSSScrollStateContainerQueriesEnabled() &&
             stream.Peek().GetType() == kFunctionToken &&
             stream.Peek().FunctionId() == CSSValueID::kScrollState) {
    // scroll-state(stuck: [ none | top | left | right | bottom | inset-* ] )
    // scroll-state(snapped: [ none | block | inline | x | y ] )
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    if (const MediaQueryExpNode* query =
            ConsumeFeatureQuery(stream, StateFeatureSet())) {
      guard.Release();
      stream.ConsumeWhitespace();
      return MediaQueryExpNode::Function(query, AtomicString("scroll-state"));
    }
  }
  stream.Restore(savepoint);

  // <general-enclosed>
  return media_query_parser_.ConsumeGeneralEnclosed(stream);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeContainerCondition(
    CSSParserTokenStream& stream) {
  return ConsumeNotAndOr(
      [this](CSSParserTokenStream& stream) {
        return this->ConsumeQueryInParens(stream);
      },
      stream);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeatureQuery(
    CSSParserTokenStream& stream,
    const FeatureSet& feature_set) {
  stream.EnsureLookAhead();
  CSSParserTokenStream::State savepoint = stream.Save();
  if (const MediaQueryExpNode* feature = ConsumeFeature(stream, feature_set)) {
    return feature;
  }
  stream.Restore(savepoint);

  if (const MediaQueryExpNode* node =
          ConsumeFeatureCondition(stream, feature_set)) {
    return node;
  }

  return nullptr;
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeatureQueryInParens(
    CSSParserTokenStream& stream,
    const FeatureSet& feature_set) {
  CSSParserTokenStream::State savepoint = stream.Save();
  if (stream.Peek().GetType() == kLeftParenthesisToken) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    const MediaQueryExpNode* query = ConsumeFeatureQuery(stream, feature_set);
    if (query && stream.AtEnd()) {
      guard.Release();
      stream.ConsumeWhitespace();
      return MediaQueryExpNode::Nested(query);
    }
  }
  stream.Restore(savepoint);

  return media_query_parser_.ConsumeGeneralEnclosed(stream);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeatureCondition(
    CSSParserTokenStream& stream,
    const FeatureSet& feature_set) {
  return ConsumeNotAndOr(
      [this, &feature_set](CSSParserTokenStream& stream) {
        return this->ConsumeFeatureQueryInParens(stream, feature_set);
      },
      stream);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeature(
    CSSParserTokenStream& stream,
    const FeatureSet& feature_set) {
  return media_query_parser_.ConsumeFeature(stream, feature_set);
}

}  // namespace blink
