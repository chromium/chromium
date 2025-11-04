// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/route_parser.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/route_query.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"

namespace blink {

namespace {

// Even if the URLPattern is parsed now, we need to keep the original string for
// serialization. The URLPattern API deliberately doesn't provide this.
struct URLPatternResult {
  STACK_ALLOCATED();

 public:
  URLPatternResult() = default;
  URLPatternResult(URLPattern* url_pattern, const AtomicString& original_string)
      : url_pattern(url_pattern), original_string(original_string) {
    DCHECK(url_pattern);
  }

  bool IsSuccess() const { return !!url_pattern; }

  URLPattern* url_pattern = nullptr;
  AtomicString original_string;
};

URLPatternResult ParseURLPattern(CSSParserTokenStream& stream,
                                 const Document& document) {
  if (stream.Peek().GetType() != kFunctionToken ||
      stream.Peek().Value() != "urlpattern") {
    return URLPatternResult();
  }

  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();
  if (stream.Peek().GetType() != kStringToken) {
    return URLPatternResult();
  }
  const CSSParserToken& pattern = stream.ConsumeIncludingWhitespace();
  if (pattern.GetType() == kBadStringToken || !stream.UncheckedAtEnd()) {
    return URLPatternResult();
  }

  AtomicString pattern_str = pattern.Value().ToAtomicString();
  V8URLPatternInput* url_pattern_input =
      MakeGarbageCollected<V8URLPatternInput>(pattern_str);
  return URLPatternResult(
      URLPattern::Create(document.GetExecutionContext()->GetIsolate(),
                         url_pattern_input, document.Url(), IGNORE_EXCEPTION),
      pattern_str);
}

// https://wicg.github.io/declarative-partial-updates/css-route-matching/#at-route
//
// <route-test> = <route-location> | <route-keyword> : <route-location>
// <route-keyword> = at | from | to
// <route-location> = <route-name> | <urlpattern()>
// <route-name> = <custom-ident>
RouteTest* ParseRouteTest(CSSParserTokenStream& stream,
                          const Document& document) {
  AtomicString route_name;
  RoutePreposition preposition = RoutePreposition::kAt;
  URLPatternResult url_pattern_result;

  bool header_valid = [&]() {
    if (stream.Peek().GetType() == kIdentToken) {
      AtomicString first_string(
          stream.ConsumeIncludingWhitespace().Value().ToString());
      if (stream.Peek().GetType() == kColonToken) {
        if (first_string == "at") {
          preposition = RoutePreposition::kAt;
        } else if (first_string == "from") {
          preposition = RoutePreposition::kFrom;
        } else if (first_string == "to") {
          preposition = RoutePreposition::kTo;
        } else {
          return false;
        }
        stream.ConsumeIncludingWhitespace();
        if (stream.Peek().GetType() == kIdentToken) {
          route_name = AtomicString(
              stream.ConsumeIncludingWhitespace().Value().ToString());
        } else {
          url_pattern_result = ParseURLPattern(stream, document);
          if (!url_pattern_result.IsSuccess()) {
            return false;
          }
        }
        return stream.AtEnd();
      }
      if (stream.AtEnd()) {
        route_name = first_string;
        return true;
      }
    } else {
      url_pattern_result = ParseURLPattern(stream, document);
      return url_pattern_result.IsSuccess() && stream.AtEnd();
    }
    return false;
  }();

  if (!header_valid) {
    return nullptr;
  }

  if (url_pattern_result.IsSuccess()) {
    return MakeGarbageCollected<RouteTest>(url_pattern_result.url_pattern,
                                           url_pattern_result.original_string,
                                           preposition);
  }
  DCHECK(!route_name.empty());
  return MakeGarbageCollected<RouteTest>(route_name, preposition);
}

}  // anonymous namespace

RouteQuery* RouteParser::ParseQuery(CSSParserTokenStream& stream,
                                    const Document& document) {
  RouteParser parser(document);
  const ConditionalExpNode* root = parser.ConsumeCondition(stream);
  if (!root) {
    return nullptr;
  }
  return MakeGarbageCollected<RouteQuery>(*root);
}

const ConditionalExpNode* RouteParser::ConsumeLeaf(
    CSSParserTokenStream& stream) {
  RouteTest* route_test = ParseRouteTest(stream, document_);
  if (!route_test) {
    return nullptr;
  }
  return MakeGarbageCollected<RouteQueryExpNode>(*route_test);
}

const ConditionalExpNode* RouteParser::ConsumeFunction(
    CSSParserTokenStream& stream) {
  return nullptr;
}

}  // namespace blink
