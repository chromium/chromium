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

bool IsNone(const CSSValue& value) {
  const auto* ident = DynamicTo<CSSIdentifierValue>(value);
  return ident && ident->GetValueID() == CSSValueID::kNone;
}

AtomicString ConsumeContainerName(CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
  CSSValue* name = css_parsing_utils::ConsumeContainerName(range, context);
  if (auto* custom_ident = DynamicTo<CSSCustomIdentValue>(name))
    return custom_ident->Value();
  return g_null_atom;
}

unsigned ConsumeContainerType(CSSParserTokenRange& range) {
  unsigned result = 0;
  CSSValue* type = css_parsing_utils::ConsumeContainerType(range);
  if (!type || IsNone(*type))
    return kContainerTypeNone;
  for (const CSSValue* item : To<CSSValueList>(*type))
    result |= To<CSSIdentifierValue>(*item).ConvertTo<EContainerType>();
  return result;
}

// not <func> | <func> [ and <func> ]* | <func> [ or <func> ]*
//
// For example, if <func> is a function that can parse <container-query>,
// then ConsumeNotAndOr can be used to parse <container-condition>:
//
// https://drafts.csswg.org/css-contain-3/#typedef-container-condition
template <typename Func>
std::unique_ptr<MediaQueryExpNode> ConsumeNotAndOr(Func func,
                                                   CSSParserTokenRange& range) {
  if (ConsumeIfIdent(range, "not"))
    return MediaQueryExpNode::Not(func(range));

  std::unique_ptr<MediaQueryExpNode> result = func(range);

  if (AtIdent(range.Peek(), "and")) {
    while (result && ConsumeIfIdent(range, "and"))
      result = MediaQueryExpNode::And(std::move(result), func(range));
  } else if (AtIdent(range.Peek(), "or")) {
    while (ConsumeIfIdent(range, "or"))
      result = MediaQueryExpNode::Or(std::move(result), func(range));
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

absl::optional<ContainerSelector> ContainerQueryParser::ConsumeSelector(
    CSSParserTokenRange& range) {
  AtomicString bare_name = ConsumeContainerName(range, context_);
  if (!bare_name.IsNull())
    return ContainerSelector(bare_name);

  absl::optional<AtomicString> name;
  absl::optional<unsigned> type;

  for (int i = 0; i < 2; i++) {
    if (range.Peek().FunctionId() == CSSValueID::kName && !name) {
      auto block = range.ConsumeBlock();
      block.ConsumeWhitespace();
      range.ConsumeWhitespace();
      name = ConsumeContainerName(block, context_);
      if (name->IsNull() || !block.AtEnd())
        return absl::nullopt;
      continue;
    }

    if (range.Peek().FunctionId() == CSSValueID::kType && !type) {
      auto block = range.ConsumeBlock();
      block.ConsumeWhitespace();
      range.ConsumeWhitespace();
      type = ConsumeContainerType(block);
      if (!*type || !block.AtEnd())
        return absl::nullopt;
      continue;
    }

    break;
  }

  return ContainerSelector(name.value_or(g_null_atom), type.value_or(0));
}

std::unique_ptr<MediaQueryExpNode> ContainerQueryParser::ParseQuery(
    String value) {
  auto tokens = CSSTokenizer(value).TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  return ParseQuery(range);
}

std::unique_ptr<MediaQueryExpNode> ContainerQueryParser::ParseQuery(
    CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  auto node = ConsumeContainerQuery(range);
  if (!range.AtEnd())
    return nullptr;
  return node;
}

std::unique_ptr<MediaQueryExpNode> ContainerQueryParser::ConsumeContainerQuery(
    CSSParserTokenRange& range) {
  CSSParserTokenRange original_range = range;

  // ( <container-condition> ) | size( <size-query> )
  if (range.Peek().GetType() == kLeftParenthesisToken) {
    auto block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();
    auto condition = ConsumeContainerCondition(block);
    if (condition && block.AtEnd())
      return MediaQueryExpNode::Nested(std::move(condition));
  } else if (range.Peek().GetType() == kFunctionToken &&
             range.Peek().FunctionId() == CSSValueID::kSize) {
    auto block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();
    auto query = ConsumeFeatureQuery(block, SizeFeatureSet());
    if (query && block.AtEnd())
      return MediaQueryExpNode::Function(std::move(query), "size");
  }
  range = original_range;

  // <general-enclosed>
  return media_query_parser_.ConsumeGeneralEnclosed(range);
}

std::unique_ptr<MediaQueryExpNode>
ContainerQueryParser::ConsumeContainerCondition(CSSParserTokenRange& range) {
  return ConsumeNotAndOr(
      [this](CSSParserTokenRange& range) {
        return this->ConsumeContainerQuery(range);
      },
      range);
}

std::unique_ptr<MediaQueryExpNode> ContainerQueryParser::ConsumeFeatureQuery(
    CSSParserTokenRange& range,
    const FeatureSet& feature_set) {
  CSSParserTokenRange original_range = range;

  if (auto feature = ConsumeFeature(range, feature_set))
    return feature;
  range = original_range;

  if (auto node = ConsumeFeatureCondition(range, feature_set))
    return node;

  return nullptr;
}

std::unique_ptr<MediaQueryExpNode>
ContainerQueryParser::ConsumeFeatureQueryInParens(
    CSSParserTokenRange& range,
    const FeatureSet& feature_set) {
  CSSParserTokenRange original_range = range;

  if (range.Peek().GetType() == kLeftParenthesisToken) {
    auto block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();
    auto query = ConsumeFeatureQuery(block, feature_set);
    if (query && block.AtEnd())
      return MediaQueryExpNode::Nested(std::move(query));
  }
  range = original_range;

  return media_query_parser_.ConsumeGeneralEnclosed(range);
}

std::unique_ptr<MediaQueryExpNode>
ContainerQueryParser::ConsumeFeatureCondition(CSSParserTokenRange& range,
                                              const FeatureSet& feature_set) {
  return ConsumeNotAndOr(
      [this, &feature_set](CSSParserTokenRange& range) {
        return this->ConsumeFeatureQueryInParens(range, feature_set);
      },
      range);
}

std::unique_ptr<MediaQueryExpNode> ContainerQueryParser::ConsumeFeature(
    CSSParserTokenRange& range,
    const FeatureSet& feature_set) {
  return media_query_parser_.ConsumeFeature(range, feature_set);
}

}  // namespace blink
