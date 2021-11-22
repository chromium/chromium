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
  auto node = media_query_parser_.ConsumeCondition(range);
  if (!range.AtEnd())
    return nullptr;
  return node;
}

}  // namespace blink
