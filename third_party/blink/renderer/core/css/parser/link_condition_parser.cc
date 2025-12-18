// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/link_condition_parser.h"

#include "third_party/blink/renderer/core/css/link_condition.h"
#include "third_party/blink/renderer/core/css/parser/conditional_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/navigation_parser.h"

namespace blink {

LinkCondition* LinkConditionParser::Parse(CSSParserTokenStream& stream,
                                          const Document& document) {
  // https://drafts.csswg.org/css-navigation-1/#typedef-link-condition
  NavigationLocation* navigation_location =
      NavigationParser::ParseLocation(stream, document);
  if (!navigation_location) {
    return nullptr;
  }
  stream.ConsumeWhitespace();

  if (stream.AtEnd()) {
    return MakeGarbageCollected<LinkCondition>(
        navigation_location, /*navigation_param_root_exp=*/nullptr);
  }

  CSSParserToken token = stream.ConsumeIncludingWhitespace();
  if (token.GetType() != kIdentToken || token.Value().ToString() != "with") {
    return nullptr;
  }

  class NavigationParamExpressionParser : public ConditionalParser {
   public:
    const ConditionalExpNode* ConsumeLeaf(CSSParserTokenStream& stream) final {
      if (stream.Peek().GetType() != kIdentToken) {
        return nullptr;
      }
      CSSParserToken token = stream.ConsumeIncludingWhitespace();
      AtomicString param = token.Value().ToAtomicString();
      token = stream.ConsumeIncludingWhitespace();
      if (token.GetType() != kColonToken) {
        return nullptr;
      }
      token = stream.ConsumeIncludingWhitespace();
      if (token.GetType() != kStringToken) {
        return nullptr;
      }
      AtomicString value = token.Value().ToAtomicString();
      if (value.empty()) {
        return nullptr;
      }
      return MakeGarbageCollected<NavigationParamExpNode>(param, value);
    }
    const ConditionalExpNode* ConsumeFunction(
        CSSParserTokenStream& stream) final {
      if (stream.Peek().FunctionId() != CSSValueID::kNavigationParam) {
        return nullptr;
      }
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      stream.ConsumeWhitespace();
      if (stream.Peek().GetType() != kIdentToken) {
        return nullptr;
      }
      CSSParserToken token = stream.ConsumeIncludingWhitespace();
      const ConditionalExpNode* node =
          MakeGarbageCollected<NavigationParamExpNode>(
              token.Value().ToAtomicString());
      if (node && guard.Release()) {
        return node;
      }
      return nullptr;
    }
  };

  NavigationParamExpressionParser param_parser;
  const ConditionalExpNode* root_node = param_parser.ConsumeCondition(stream);
  if (!root_node) {
    return nullptr;
  }

  return MakeGarbageCollected<LinkCondition>(navigation_location, root_node);
}

}  // namespace blink
