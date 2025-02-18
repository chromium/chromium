// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_if_parser.h"

#include "third_party/blink/renderer/core/css/if_test.h"
#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"

namespace blink {

CSSIfParser::CSSIfParser(const CSSParserContext& context)
    : container_query_parser_(ContainerQueryParser(context)),
      media_query_parser_(MediaQueryParser::kMediaQuerySetParser,
                          kHTMLStandardMode,
                          context.GetExecutionContext(),
                          MediaQueryParser::SyntaxLevel::kLevel4) {}

// <if-test> = media( <media-query> ) | style( <style-query> )
std::optional<IfTest> CSSIfParser::ConsumeIfTest(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kFunctionToken &&
      stream.Peek().FunctionId() == CSSValueID::kStyle) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    if (const MediaQueryExpNode* query =
            container_query_parser_.ConsumeFeatureQuery(
                stream, ContainerQueryParser::StyleFeatureSet())) {
      guard.Release();
      stream.ConsumeWhitespace();
      return IfTest(MediaQueryExpNode::Function(query, AtomicString("style")));
    }
  }
  if (stream.Peek().GetType() == kFunctionToken &&
      stream.Peek().FunctionId() == CSSValueID::kMedia) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    if (const MediaQuery* query = media_query_parser_.ConsumeQuery(stream)) {
      guard.Release();
      stream.ConsumeWhitespace();
      return IfTest(query);
    }
  }
  return std::nullopt;
}

}  // namespace blink
