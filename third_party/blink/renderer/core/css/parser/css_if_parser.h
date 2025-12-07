// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_IF_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_IF_PARSER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/conditional_parser.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenStream;

class CORE_EXPORT CSSIfParser : public ConditionalParser {
  STACK_ALLOCATED();

 public:
  explicit CSSIfParser(const CSSParserContext&);

  const ConditionalExpNode* ConsumeIfCondition(CSSParserTokenStream&);

  const ConditionalExpNode* ConsumeLeaf(CSSParserTokenStream&) override;
  const ConditionalExpNode* ConsumeFunction(CSSParserTokenStream&) override;

 private:
  const CSSParserContext& parser_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_IF_PARSER_H_
