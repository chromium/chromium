// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"

#include "third_party/blink/renderer/core/css/conditional_exp_node.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"

namespace blink {

namespace {

class SizeFeatureSet : public MediaQueryParser::FeatureSet {
  STACK_ALLOCATED();

 public:
  bool IsAllowed(const AtomicString& feature) const override {
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
  bool IsAllowedWithoutValue(const AtomicString& feature,
                             const ExecutionContext*) const override {
    return feature == media_feature_names::kWidthMediaFeature ||
           feature == media_feature_names::kHeightMediaFeature ||
           feature == media_feature_names::kInlineSizeMediaFeature ||
           feature == media_feature_names::kBlockSizeMediaFeature ||
           feature == media_feature_names::kAspectRatioMediaFeature ||
           feature == media_feature_names::kOrientationMediaFeature;
  }
  bool IsAllowedWithValue(const AtomicString& feature) const override {
    return true;
  }
  bool IsCaseSensitive(const AtomicString& feature) const override {
    return false;
  }
  bool SupportsRange() const override { return true; }
  bool SupportsStyleRange() const override { return false; }
  bool SupportsElementDependent() const override { return true; }
};

class StateFeatureSet : public MediaQueryParser::FeatureSet {
  STACK_ALLOCATED();

 public:
  bool IsAllowed(const AtomicString& feature) const override {
    return feature == media_feature_names::kStuckMediaFeature ||
           feature == media_feature_names::kSnappedMediaFeature ||
           feature == media_feature_names::kScrollableMediaFeature ||
           (RuntimeEnabledFeatures::CSSScrolledContainerQueriesEnabled() &&
            feature == media_feature_names::kScrolledMediaFeature);
  }
  bool IsAllowedWithoutValue(const AtomicString& feature,
                             const ExecutionContext*) const override {
    return true;
  }
  bool IsAllowedWithValue(const AtomicString& feature) const override {
    return true;
  }
  bool IsCaseSensitive(const AtomicString& feature) const override {
    return false;
  }
  bool SupportsRange() const override { return false; }
  bool SupportsStyleRange() const override { return false; }
  bool SupportsElementDependent() const override { return true; }
};

class AnchoredFeatureSet : public MediaQueryParser::FeatureSet {
  STACK_ALLOCATED();

 public:
  bool IsAllowed(const AtomicString& feature) const override {
    return feature == media_feature_names::kFallbackMediaFeature;
  }
  bool IsAllowedWithoutValue(const AtomicString& feature,
                             const ExecutionContext*) const override {
    return true;
  }
  bool IsAllowedWithValue(const AtomicString& feature) const override {
    return true;
  }
  bool IsCaseSensitive(const AtomicString& feature) const override {
    return false;
  }
  bool SupportsRange() const override { return false; }
  bool SupportsStyleRange() const override { return false; }
  bool SupportsElementDependent() const override { return true; }
};

}  // namespace

ContainerQueryParser::ContainerQueryParser(const CSSParserContext& context)
    : context_(context) {}

const ConditionalExpNode* ContainerQueryParser::ParseCondition(String value) {
  CSSParserTokenStream stream(value);
  const ConditionalExpNode* node = ParseCondition(stream);
  if (!stream.AtEnd()) {
    return nullptr;
  }
  return node;
}

const ConditionalExpNode* ContainerQueryParser::ParseCondition(
    CSSParserTokenStream& stream) {
  stream.ConsumeWhitespace();
  return ConsumeCondition(stream);
}

const ConditionalExpNode* ContainerQueryParser::ConsumeLeaf(
    CSSParserTokenStream& stream) {
  CSSParserTokenStream::State savepoint = stream.Save();
  stream.ConsumeWhitespace();
  // <size-feature>
  const ConditionalExpNode* query = ConsumeFeature(stream, SizeFeatureSet());
  if (query && stream.AtEnd()) {
    stream.ConsumeWhitespace();
    return query;
  }
  stream.Restore(savepoint);
  return nullptr;
}

// style( <style-query> ) |
// scroll-state( <scroll-state-query> ) |
// anchored( <anchored-state-query> )
const ConditionalExpNode* ContainerQueryParser::ConsumeFunction(
    CSSParserTokenStream& stream) {
  DCHECK_EQ(stream.Peek().GetType(), kFunctionToken);

  if (stream.Peek().FunctionId() == CSSValueID::kStyle) {
    // style( <style-query> )
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    if (const ConditionalExpNode* query =
            ConsumeFeatureQuery(stream, StyleFeatureSet())) {
      context_.Count(WebFeature::kCSSStyleContainerQuery);
      guard.Release();
      return ConditionalExpNode::Function(query, AtomicString("style"));
    }
  } else if (stream.Peek().FunctionId() == CSSValueID::kScrollState) {
    // scroll-state(stuck: [ none | top | right | bottom | left | block-start |
    // inline-start | block-end | inline-end ] )
    // scroll-state(snapped: [ none | x | y | block | inline ] )
    // scroll-state(scrollable: [ none | top | right | bottom | left |
    // block-start | inline-start | block-end | inline-end | x | y | block |
    // inline ] )
    // scroll-state(scrolled: [ none | top | right | bottom | left
    // | block-start | inline-start | block-end | inline-end | x | y | block |
    // inline ] )
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    if (const ConditionalExpNode* query =
            ConsumeFeatureQuery(stream, StateFeatureSet())) {
      guard.Release();
      return ConditionalExpNode::Function(query, AtomicString("scroll-state"));
    }
  } else if (RuntimeEnabledFeatures::CSSFallbackContainerQueriesEnabled() &&
             stream.Peek().FunctionId() == CSSValueID::kAnchored) {
    // anchored(fallback: [<dashed-ident> || <try-tactic>] | <'position-area'>)
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();

    if (const ConditionalExpNode* query =
            ConsumeFeatureQuery(stream, AnchoredFeatureSet())) {
      guard.Release();
      return ConditionalExpNode::Function(query, AtomicString("anchored"));
    }
  }

  return nullptr;
}

const ConditionalExpNode* ContainerQueryParser::ConsumeFeatureQuery(
    CSSParserTokenStream& stream,
    const FeatureSet& feature_set) {
  stream.EnsureLookAhead();
  CSSParserTokenStream::State savepoint = stream.Save();
  if (const ConditionalExpNode* feature = ConsumeFeature(stream, feature_set)) {
    return feature;
  }

  // Not at a feature leaf. Most likely because of parentheses inside a
  // function. Parse expressions (such as parentheses, `and`, `or`, `not`). For
  // each actual feature, call back into this parser, with the specified feature
  // set.

  stream.Restore(savepoint);

  class ExpressionParser : public ConditionalParser {
   public:
    ExpressionParser(ContainerQueryParser& parent_parser,
                     const FeatureSet& feature_set)
        : parent_parser_(parent_parser), feature_set_(feature_set) {}

    const ConditionalExpNode* ConsumeLeaf(
        CSSParserTokenStream& stream) override {
      return parent_parser_.ConsumeFeature(stream, feature_set_);
    }
    const ConditionalExpNode* ConsumeFunction(CSSParserTokenStream&) override {
      return nullptr;
    }

   private:
    ContainerQueryParser& parent_parser_;
    const FeatureSet& feature_set_;
  };

  ExpressionParser nested_parser(*this, feature_set);
  return nested_parser.ConsumeCondition(stream);
}

const ConditionalExpNode* ContainerQueryParser::ConsumeFeature(
    CSSParserTokenStream& stream,
    const FeatureSet& feature_set) {
  MediaQueryParser media_query_parser(MediaQueryParser::kMediaQuerySetParser,
                                      context_.GetExecutionContext());
  return media_query_parser.ConsumeFeature(stream, feature_set);
}

}  // namespace blink
