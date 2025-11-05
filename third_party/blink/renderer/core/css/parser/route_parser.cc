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
struct URLPatternParseResult {
  STACK_ALLOCATED();

 public:
  URLPatternParseResult() = default;
  URLPatternParseResult(URLPattern* url_pattern,
                        const AtomicString& original_string)
      : url_pattern(url_pattern), original_string(original_string) {
    DCHECK(url_pattern);
  }

  bool IsSuccess() const { return !!url_pattern; }

  URLPattern* url_pattern = nullptr;
  AtomicString original_string;
};

URLPatternParseResult ParseURLPattern(CSSParserTokenStream& stream,
                                      const Document& document) {
  if (stream.Peek().GetType() != kFunctionToken ||
      stream.Peek().Value() != "urlpattern") {
    return URLPatternParseResult();
  }

  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();
  if (stream.Peek().GetType() != kStringToken) {
    return URLPatternParseResult();
  }
  const CSSParserToken& pattern = stream.ConsumeIncludingWhitespace();
  if (pattern.GetType() == kBadStringToken || !stream.UncheckedAtEnd()) {
    return URLPatternParseResult();
  }

  AtomicString pattern_str = pattern.Value().ToAtomicString();
  V8URLPatternInput* url_pattern_input =
      MakeGarbageCollected<V8URLPatternInput>(pattern_str);
  return URLPatternParseResult(
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
  URLPatternParseResult url_pattern_result;

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

  RouteLocation* route_location;
  if (url_pattern_result.IsSuccess()) {
    route_location = MakeGarbageCollected<RouteLocation>(
        url_pattern_result.url_pattern, url_pattern_result.original_string);
  } else {
    DCHECK(!route_name.empty());
    route_location = MakeGarbageCollected<RouteLocation>(route_name);
  }

  return MakeGarbageCollected<RouteTest>(*route_location, preposition);
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

RouteLocation* RouteParser::ParseLocation(CSSParserTokenStream& stream,
                                          const Document& document) {
  if (stream.Peek().GetType() == kIdentToken) {
    AtomicString route_name(
        stream.ConsumeIncludingWhitespace().Value().ToString());
    if (stream.AtEnd()) {
      return MakeGarbageCollected<RouteLocation>(route_name);
    }
  } else {
    URLPatternParseResult result = ParseURLPattern(stream, document);
    if (result.IsSuccess()) {
      stream.ConsumeWhitespace();
      if (stream.AtEnd()) {
        return MakeGarbageCollected<RouteLocation>(result.url_pattern,
                                                   result.original_string);
      }
    }
  }
  return nullptr;
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
