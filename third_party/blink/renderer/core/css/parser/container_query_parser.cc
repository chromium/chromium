// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"

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
                                         CSSParserTokenRange& range) {
  if (ConsumeIfIdent(range, "not")) {
    return MediaQueryExpNode::Not(func(range));
  }

  const MediaQueryExpNode* result = func(range);

  if (AtIdent(range.Peek(), "and")) {
    while (result && ConsumeIfIdent(range, "and")) {
      result = MediaQueryExpNode::And(result, func(range));
    }
  } else if (AtIdent(range.Peek(), "or")) {
    while (ConsumeIfIdent(range, "or")) {
      result = MediaQueryExpNode::Or(result, func(range));
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
    return false;
  }
  bool IsCaseSensitive(const String& feature) const override {
    // TODO(crbug.com/1302630): non-custom properties are case-insensitive.
    return true;
  }
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
  auto [tokens, raw_offsets] = CSSTokenizer(value).TokenizeToEOFWithOffsets();
  CSSParserTokenRange range(tokens);
  CSSParserTokenOffsets offsets(tokens, std::move(raw_offsets), value);
  return ParseCondition(range, offsets);
}

const MediaQueryExpNode* ContainerQueryParser::ParseCondition(
    CSSParserTokenRange range,
    const CSSParserTokenOffsets& offsets) {
  range.ConsumeWhitespace();
  const MediaQueryExpNode* node = ConsumeContainerCondition(range, offsets);
  if (!range.AtEnd()) {
    return nullptr;
  }
  return node;
}

// <query-in-parens> = ( <container-condition> )
//                   | ( <size-feature> )
//                   | style( <style-query> )
//                   | <general-enclosed>
const MediaQueryExpNode* ContainerQueryParser::ConsumeQueryInParens(
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets) {
  CSSParserTokenRange original_range = range;

  if (range.Peek().GetType() == kLeftParenthesisToken) {
    // ( <size-feature> ) | ( <container-condition> )
    CSSParserTokenRange block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();

    CSSParserTokenRange original_block = block;
    // <size-feature>
    const MediaQueryExpNode* query =
        ConsumeFeature(block, offsets, SizeFeatureSet());
    if (query && block.AtEnd()) {
      return MediaQueryExpNode::Nested(query);
    }
    block = original_block;

    // <container-condition>
    const MediaQueryExpNode* condition =
        ConsumeContainerCondition(block, offsets);
    if (condition && block.AtEnd()) {
      return MediaQueryExpNode::Nested(condition);
    }
  } else if (RuntimeEnabledFeatures::CSSStyleQueriesEnabled() &&
             range.Peek().GetType() == kFunctionToken &&
             range.Peek().FunctionId() == CSSValueID::kStyle) {
    // style( <style-query> )
    CSSParserTokenRange block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();

    if (const MediaQueryExpNode* query =
            ConsumeFeatureQuery(block, offsets, StyleFeatureSet())) {
      return MediaQueryExpNode::Function(query, "style");
    }
  }
  range = original_range;

  // <general-enclosed>
  return media_query_parser_.ConsumeGeneralEnclosed(range);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeContainerCondition(
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets) {
  return ConsumeNotAndOr(
      [this, offsets](CSSParserTokenRange& range) {
        return this->ConsumeQueryInParens(range, offsets);
      },
      range);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeatureQuery(
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets,
    const FeatureSet& feature_set) {
  CSSParserTokenRange original_range = range;

  if (const MediaQueryExpNode* feature =
          ConsumeFeature(range, offsets, feature_set)) {
    return feature;
  }
  range = original_range;

  if (const MediaQueryExpNode* node =
          ConsumeFeatureCondition(range, offsets, feature_set)) {
    return node;
  }

  return nullptr;
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeatureQueryInParens(
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets,
    const FeatureSet& feature_set) {
  CSSParserTokenRange original_range = range;

  if (range.Peek().GetType() == kLeftParenthesisToken) {
    auto block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();
    const MediaQueryExpNode* query =
        ConsumeFeatureQuery(block, offsets, feature_set);
    if (query && block.AtEnd()) {
      return MediaQueryExpNode::Nested(query);
    }
  }
  range = original_range;

  return media_query_parser_.ConsumeGeneralEnclosed(range);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeatureCondition(
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets,
    const FeatureSet& feature_set) {
  return ConsumeNotAndOr(
      [this, &offsets, &feature_set](CSSParserTokenRange& range) {
        return this->ConsumeFeatureQueryInParens(range, offsets, feature_set);
      },
      range);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeature(
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets,
    const FeatureSet& feature_set) {
  return media_query_parser_.ConsumeFeature(range, offsets, feature_set);
}

}  // namespace blink
