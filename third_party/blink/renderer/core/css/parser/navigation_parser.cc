// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/navigation_parser.h"

#include "third_party/blink/renderer/core/css/navigation_query.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/route_matching/navigation_phase.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

NavigationTestExpression* NavigationParser::ParseNavigationTest(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kIdentToken) {
    return nullptr;
  }
  CSSParserToken token = stream.ConsumeIncludingWhitespace();
  if (EqualIgnoringAsciiCase(token.Value(), "preview") &&
      RuntimeEnabledFeatures::TwoPhaseViewTransitionEnabled()) {
    // TODO(crbug.com/436805487): Not in the spec.
    return MakeGarbageCollected<NavigationPreviewTestExpression>();
  }
  if (stream.Peek().GetType() != kColonToken) {
    return nullptr;
  }
  stream.ConsumeIncludingWhitespace();
  if (EqualIgnoringAsciiCase(token.Value(), "history")) {
    // <navigation-type-test> = history : <navigation-type-keyword>
    // <navigation-type-keyword> = traverse | back | forward | reload
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
    // <navigation-phase-test> = phase : <navigation-phase-keyword>
    // <navigation-phase-keyword> = loading | ready | committed
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

  if (EqualIgnoringAsciiCase(token.Value(), "between")) {
    // <navigation-location-between-test> =
    //   between : <navigation-location> and <navigation-location>
    NavigationLocation* location1 = ParseLocation(stream);
    if (!location1) {
      return nullptr;
    }
    stream.ConsumeWhitespace();
    if (stream.Peek().GetType() != kIdentToken) {
      return nullptr;
    }
    CSSParserToken and_token = stream.ConsumeIncludingWhitespace();
    if (and_token.GetType() != kIdentToken ||
        !EqualIgnoringAsciiCase(and_token.Value(), "and")) {
      return nullptr;
    }
    NavigationLocation* location2 = ParseLocation(stream);
    if (!location2 || !stream.AtEnd()) {
      return nullptr;
    }

    return MakeGarbageCollected<NavigationLocationBetweenTestExpression>(
        *location1, *location2);
  }

  // <navigation-location-test> =
  //   <navigation-location-keyword> : <navigation-location>
  // <navigation-location-keyword> = at | from | to
  // <navigation-location> = <location-name> | <url-pattern()>
  // <location-name> = <dashed-ident>
  std::optional<NavigationPreposition> preposition =
      ParsePrepositionIdent(token);
  if (!preposition) {
    return nullptr;
  }

  NavigationLocation* location = ParseLocation(stream);
  if (!location || !stream.AtEnd()) {
    return nullptr;
  }

  return MakeGarbageCollected<NavigationLocationTestExpression>(*location,
                                                                *preposition);
}

NavigationQuery* NavigationParser::ParseQuery(CSSParserTokenStream& stream) {
  NavigationParser parser;
  const ConditionalExpNode* root = parser.ConsumeCondition(stream);
  if (!root) {
    return nullptr;
  }
  return MakeGarbageCollected<NavigationQuery>(*root);
}

NavigationLocation* NavigationParser::ParseLocation(
    CSSParserTokenStream& stream) {
  if (css_parsing_utils::IsDashedIdent(stream.Peek())) {
    // <location-name>
    AtomicString location_name(
        stream.ConsumeIncludingWhitespace().Value().ToString());
    return MakeGarbageCollected<NavigationLocation>(
        NavigationLocation::kLocationName, location_name);
  }

  NavigationLocation::Type type;
  AtomicString value;
  if (stream.Peek().GetType() == kUrlToken) {
    // Unquoted url().
    CSSParserToken token = stream.ConsumeIncludingWhitespace();
    type = NavigationLocation::kUrl;
    value = token.Value().ToAtomicString();
  } else {
    // url-pattern() or quoted url().
    if (stream.Peek().GetType() != kFunctionToken) {
      return nullptr;
    }
    const AtomicString arg(stream.Peek().Value());
    if (EqualIgnoringAsciiCase(arg, "url-pattern")) {
      type = NavigationLocation::kUrlPattern;
    } else if (EqualIgnoringAsciiCase(arg, "url")) {
      type = NavigationLocation::kUrl;
    } else {
      return nullptr;
    }

    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();
    if (stream.Peek().GetType() != kStringToken) {
      return nullptr;
    }
    const CSSParserToken& token = stream.ConsumeIncludingWhitespace();
    if (token.GetType() == kBadStringToken || !stream.UncheckedAtEnd()) {
      return nullptr;
    }
    value = token.Value().ToAtomicString();
  }

  return MakeGarbageCollected<NavigationLocation>(type, value);
}

std::optional<NavigationPreposition> NavigationParser::ParsePrepositionIdent(
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
  return std::nullopt;
}

const ConditionalExpNode* NavigationParser::ConsumeLeaf(
    CSSParserTokenStream& stream) {
  NavigationTestExpression* navigation_test = ParseNavigationTest(stream);
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
