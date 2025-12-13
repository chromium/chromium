// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_if_parser.h"

#include "third_party/blink/renderer/core/css/if_condition.h"
#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/navigation_parser.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

CSSIfParser::CSSIfParser(const CSSParserContext& context)
    : parser_context_(context) {}

const ConditionalExpNode* CSSIfParser::ConsumeLeaf(CSSParserTokenStream&) {
  // Everything is functions. See ConsumeFunction().
  return nullptr;
}

// <if-test> =
//   supports( [ <supports-condition> | <ident> : <declaration-value> ] ) |
//   media( <media-feature> | <media-condition> ) |
//   style( <style-query> ) |
//   navigation( <navigation-condition> )
const ConditionalExpNode* CSSIfParser::ConsumeFunction(
    CSSParserTokenStream& stream) {
  DCHECK_EQ(stream.Peek().GetType(), kFunctionToken);
  if (RuntimeEnabledFeatures::CSSInlineIfForSupportsQueriesEnabled() &&
      stream.Peek().FunctionId() == CSSValueID::kSupports) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    CSSParserImpl supports_query_parser(&parser_context_);
    CSSSupportsParser::Result supports_parsing_result =
        CSSSupportsParser::ConsumeSupportsCondition(stream,
                                                    supports_query_parser);
    if (supports_parsing_result != CSSSupportsParser::Result::kParseFailure) {
      guard.Release();
      bool result =
          (supports_parsing_result == CSSSupportsParser::Result::kSupported);
      return MakeGarbageCollected<IfTestSupports>(result);
    }
    if (stream.Peek().GetType() == kIdentToken &&
        supports_query_parser.ConsumeSupportsDeclaration(stream) &&
        guard.Release()) {
      return MakeGarbageCollected<IfTestSupports>(true);
    }
  }
  if (RuntimeEnabledFeatures::CSSInlineIfForMediaQueriesEnabled() &&
      stream.Peek().FunctionId() == CSSValueID::kMedia) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    // `MediaQueryParser::ConsumeFeature` does not restore the stream,
    // hence, unlike the spec, we first try to consume <media-condition>
    // and then <media-feature>.
    MediaQueryParser media_query_parser(MediaQueryParser::kMediaQuerySetParser,
                                        parser_context_.GetExecutionContext());
    if (const ConditionalExpNode* query =
            media_query_parser.ConsumeCondition(stream)) {
      guard.Release();
      return MakeGarbageCollected<IfTestMedia>(query);
    }
    if (const ConditionalExpNode* query = media_query_parser.ConsumeFeature(
            stream, MediaQueryParser::MediaQueryFeatureSet())) {
      guard.Release();
      return MakeGarbageCollected<IfTestMedia>(query);
    }
  }
  if (stream.Peek().FunctionId() == CSSValueID::kStyle) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    ContainerQueryParser container_query_parser(parser_context_);
    if (const ConditionalExpNode* query =
            container_query_parser.ConsumeFeatureQuery(
                stream, ContainerQueryParser::StyleFeatureSet())) {
      guard.Release();
      return ConditionalExpNode::Function(query, AtomicString("style"));
    }
  }
  if (stream.Peek().FunctionId() == CSSValueID::kNavigation &&
      RuntimeEnabledFeatures::RouteMatchingEnabled()) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    NavigationParser navigation_parser(*parser_context_.GetDocument());
    CSSParserTokenStream::State savepoint = stream.Save();
    // We're inside the function's parentheses. Don't require any additional
    // ones. Look for <navigation-test>.
    const ConditionalExpNode* node = navigation_parser.ConsumeLeaf(stream);
    if (!node) {
      // If that fails, though, look for <navigation-condition>, to handle
      // additional parentheses and expressions.
      stream.Restore(savepoint);
      node = navigation_parser.ConsumeCondition(stream);
    }
    if (node) {
      guard.Release();
      return ConditionalExpNode::Nested(node);
    }
  }
  return nullptr;
}

// <if-condition> = <boolean-expr[ <if-test> ]> | else
// https://drafts.csswg.org/css-values-5/#if-notation
const ConditionalExpNode* CSSIfParser::ConsumeIfCondition(
    CSSParserTokenStream& stream) {
  // else
  if (stream.Peek().Id() == CSSValueID::kElse) {
    stream.ConsumeIncludingWhitespace();
    return MakeGarbageCollected<IfConditionElse>();
  }
  // <boolean-expr[ <if-test> ]>
  return ConsumeCondition(stream);
}

}  // namespace blink
