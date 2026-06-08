// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/navigation_parser.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/css/navigation_query.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/route_matching/navigation_phase.h"
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
      stream.Peek().Value() != "url-pattern") {
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

std::optional<NavigationPreposition> PrepositionFromIdent(
    CSSParserToken token) {
  DCHECK_EQ(token.GetType(), kIdentToken);
  if (EqualIgnoringAsciiCase(token.Value(), "at")) {
    return NavigationPreposition::kAt;
  }
  if (EqualIgnoringAsciiCase(token.Value(), "from")) {
    return NavigationPreposition::kFrom;
  }
  if (EqualIgnoringAsciiCase(token.Value(), "to")) {
    return NavigationPreposition::kTo;
  }
  if (EqualIgnoringAsciiCase(token.Value(), "with")) {
    return NavigationPreposition::kWith;
  }
  return std::nullopt;
}

// https://drafts.csswg.org/css-navigation-1/#typedef-navigation-test
// https://github.com/w3c/csswg-drafts/blob/main/css-view-transitions-2/two-phase-transition-explainer.md
//
// <navigation-test> = <navigation-location-test> | <navigation-type-test> |
// preview
//
// <navigation-location-test> =
//   <navigation-location-keyword> : <navigation-location>
// <navigation-location-keyword> = at | from | to
// <navigation-location> = <route-name> | <url-pattern()>
// <route-name> = <dashed-ident>
//
// <navigation-type-test> = history : <navigation-type-keyword>
// <navigation-type-keyword> = traverse | back | forward | reload
NavigationTestExpression* ParseNavigationTest(CSSParserTokenStream& stream,
                                              const Document& document) {
  if (stream.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  CSSParserToken token = stream.ConsumeIncludingWhitespace();
  if (EqualIgnoringAsciiCase(token.Value(), "preview") &&
      RuntimeEnabledFeatures::TwoPhaseViewTransitionEnabled()) {
    return MakeGarbageCollected<NavigationPreviewTestExpression>();
  }
  if (stream.Peek().GetType() != kColonToken) {
    return nullptr;
  }
  stream.ConsumeIncludingWhitespace();
  if (EqualIgnoringAsciiCase(token.Value(), "history")) {
    // <navigation-type-test>
    if (stream.Peek().GetType() != kIdentToken) {
      return nullptr;
    }
    CSSParserToken type_token = stream.ConsumeIncludingWhitespace();
    NavigationTypeTestExpression::Type type;
    if (EqualIgnoringAsciiCase(type_token.Value(), "traverse")) {
      type = NavigationTypeTestExpression::kTraverse;
    } else if (EqualIgnoringAsciiCase(type_token.Value(), "back")) {
      type = NavigationTypeTestExpression::kBack;
    } else if (EqualIgnoringAsciiCase(type_token.Value(), "forward")) {
      type = NavigationTypeTestExpression::kForward;
    } else if (EqualIgnoringAsciiCase(type_token.Value(), "reload")) {
      // TODO(crbug.com/436805487): Support "reload".
      return nullptr;
    } else {
      return nullptr;
    }
    return MakeGarbageCollected<NavigationTypeTestExpression>(type);
  }

  if (EqualIgnoringAsciiCase(token.Value(), "phase")) {
    // <navigation-phase-test>
    if (stream.Peek().GetType() != kIdentToken) {
      return nullptr;
    }
    CSSParserToken phase_token = stream.ConsumeIncludingWhitespace();
    NavigationPhase phase;
    if (EqualIgnoringAsciiCase(phase_token.Value(), "loading")) {
      phase = NavigationPhase::kLoading;
    } else if (EqualIgnoringAsciiCase(phase_token.Value(), "ready")) {
      // TODO(crbug.com/436805487): Support "ready".
      return nullptr;
    } else if (EqualIgnoringAsciiCase(phase_token.Value(), "committed")) {
      phase = NavigationPhase::kCommitted;
    } else {
      return nullptr;
    }
    return MakeGarbageCollected<NavigationPhaseTestExpression>(phase);
  }

  std::optional<NavigationPreposition> preposition =
      PrepositionFromIdent(token);
  if (!preposition) {
    return nullptr;
  }

  AtomicString route_name;
  URLPatternParseResult url_pattern_result;
  if (stream.Peek().GetType() == kIdentToken) {
    route_name = stream.ConsumeIncludingWhitespace().Value().ToAtomicString();
  } else {
    url_pattern_result = ParseURLPattern(stream, document);
    if (!url_pattern_result.IsSuccess()) {
      return nullptr;
    }
  }
  if (!stream.AtEnd()) {
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

  return MakeGarbageCollected<NavigationLocationTestExpression>(*route_location,
                                                                *preposition);
}

}  // anonymous namespace

NavigationQuery* NavigationParser::ParseQuery(CSSParserTokenStream& stream,
                                              const Document& document) {
  NavigationParser parser(document);
  const ConditionalExpNode* root = parser.ConsumeCondition(stream);
  if (!root) {
    return nullptr;
  }
  return MakeGarbageCollected<NavigationQuery>(*root);
}

RouteLocation* NavigationParser::ParseLocation(CSSParserTokenStream& stream,
                                               const Document& document) {
  if (stream.Peek().GetType() == kIdentToken) {
    AtomicString route_name(
        stream.ConsumeIncludingWhitespace().Value().ToString());
    return MakeGarbageCollected<RouteLocation>(route_name);
  }
  URLPatternParseResult result = ParseURLPattern(stream, document);
  if (result.IsSuccess()) {
    return MakeGarbageCollected<RouteLocation>(result.url_pattern,
                                               result.original_string);
  }
  return nullptr;
}

std::optional<NavigationPreposition> NavigationParser::ParsePrepositionIdent(
    CSSParserToken token) {
  return PrepositionFromIdent(token);
}

const ConditionalExpNode* NavigationParser::ConsumeLeaf(
    CSSParserTokenStream& stream) {
  NavigationTestExpression* navigation_test =
      ParseNavigationTest(stream, document_);
  if (!navigation_test) {
    return nullptr;
  }
  return MakeGarbageCollected<NavigationExpNode>(*navigation_test);
}

const ConditionalExpNode* NavigationParser::ConsumeFunction(
    CSSParserTokenStream& stream) {
  return nullptr;
}

}  // namespace blink
