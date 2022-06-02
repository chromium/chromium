// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
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
  if (ConsumeIfIdent(range, "not"))
    return MediaQueryExpNode::Not(func(range));

  const MediaQueryExpNode* result = func(range);

  if (AtIdent(range.Peek(), "and")) {
    while (result && ConsumeIfIdent(range, "and"))
      result = MediaQueryExpNode::And(result, func(range));
  } else if (AtIdent(range.Peek(), "or")) {
    while (ConsumeIfIdent(range, "or"))
      result = MediaQueryExpNode::Or(result, func(range));
  }

  return result;
}

class SizeFeatureSet : public MediaQueryParser::FeatureSet {
  STACK_ALLOCATED();

 public:
  bool IsAllowed(const String& name) const override {
    return name == media_feature_names::kWidthMediaFeature ||
           name == media_feature_names::kMinWidthMediaFeature ||
           name == media_feature_names::kMaxWidthMediaFeature ||
           name == media_feature_names::kHeightMediaFeature ||
           name == media_feature_names::kMinHeightMediaFeature ||
           name == media_feature_names::kMaxHeightMediaFeature ||
           name == media_feature_names::kInlineSizeMediaFeature ||
           name == media_feature_names::kMinInlineSizeMediaFeature ||
           name == media_feature_names::kMaxInlineSizeMediaFeature ||
           name == media_feature_names::kBlockSizeMediaFeature ||
           name == media_feature_names::kMinBlockSizeMediaFeature ||
           name == media_feature_names::kMaxBlockSizeMediaFeature ||
           name == media_feature_names::kAspectRatioMediaFeature ||
           name == media_feature_names::kMinAspectRatioMediaFeature ||
           name == media_feature_names::kMaxAspectRatioMediaFeature ||
           name == media_feature_names::kOrientationMediaFeature;
  }
};

}  // namespace

ContainerQueryParser::ContainerQueryParser(const CSSParserContext& context)
    : context_(context),
      media_query_parser_(MediaQueryParser::kMediaQuerySetParser,
                          kHTMLStandardMode,
                          context.GetExecutionContext(),
                          MediaQueryParser::SyntaxLevel::kLevel4) {}

const MediaQueryExpNode* ContainerQueryParser::ParseQuery(String value) {
  auto tokens = CSSTokenizer(value).TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  return ParseQuery(range);
}

const MediaQueryExpNode* ContainerQueryParser::ParseQuery(
    CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  const MediaQueryExpNode* node = ConsumeContainerQuery(range);
  if (!range.AtEnd())
    return nullptr;
  return node;
}

// <container-query> = ( <container-condition> )
//                   | ( <size-feature> )
//                   | style( <style-query> )
//                   | <general-enclosed>
const MediaQueryExpNode* ContainerQueryParser::ConsumeContainerQuery(
    CSSParserTokenRange& range) {
  CSSParserTokenRange original_range = range;

  // ( <size-feature> ) | ( <container-condition> )
  if (range.Peek().GetType() == kLeftParenthesisToken) {
    CSSParserTokenRange block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();

    CSSParserTokenRange original_block = block;
    // <size-feature>
    const MediaQueryExpNode* query = ConsumeFeature(block, SizeFeatureSet());
    if (query && block.AtEnd())
      return MediaQueryExpNode::Nested(query);
    block = original_block;

    // <container-condition>
    const MediaQueryExpNode* condition = ConsumeContainerCondition(block);
    if (condition && block.AtEnd())
      return MediaQueryExpNode::Nested(condition);
  }
  range = original_range;

  // TODO(crbug.com/1302630): Support style( <style-query> ).

  // <general-enclosed>
  return media_query_parser_.ConsumeGeneralEnclosed(range);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeContainerCondition(
    CSSParserTokenRange& range) {
  return ConsumeNotAndOr(
      [this](CSSParserTokenRange& range) {
        return this->ConsumeContainerQuery(range);
      },
      range);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeatureQuery(
    CSSParserTokenRange& range,
    const FeatureSet& feature_set) {
  CSSParserTokenRange original_range = range;

  if (const MediaQueryExpNode* feature = ConsumeFeature(range, feature_set))
    return feature;
  range = original_range;

  if (const MediaQueryExpNode* node =
          ConsumeFeatureCondition(range, feature_set)) {
    return node;
  }

  return nullptr;
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeatureQueryInParens(
    CSSParserTokenRange& range,
    const FeatureSet& feature_set) {
  CSSParserTokenRange original_range = range;

  if (range.Peek().GetType() == kLeftParenthesisToken) {
    auto block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();
    const MediaQueryExpNode* query = ConsumeFeatureQuery(block, feature_set);
    if (query && block.AtEnd())
      return MediaQueryExpNode::Nested(query);
  }
  range = original_range;

  return media_query_parser_.ConsumeGeneralEnclosed(range);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeatureCondition(
    CSSParserTokenRange& range,
    const FeatureSet& feature_set) {
  return ConsumeNotAndOr(
      [this, &feature_set](CSSParserTokenRange& range) {
        return this->ConsumeFeatureQueryInParens(range, feature_set);
      },
      range);
}

const MediaQueryExpNode* ContainerQueryParser::ConsumeFeature(
    CSSParserTokenRange& range,
    const FeatureSet& feature_set) {
  return media_query_parser_.ConsumeFeature(range, feature_set);
}

}  // namespace blink
