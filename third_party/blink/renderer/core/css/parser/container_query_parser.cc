// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"

namespace blink {

ContainerQueryParser::ContainerQueryParser(const CSSParserContext& context)
    : media_query_parser_(MediaQueryParser::kMediaQuerySetParser,
                          kHTMLStandardMode,
                          context.GetExecutionContext(),
                          MediaQueryParser::SyntaxLevel::kLevel4) {}

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
