// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_NAVIGATION_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_NAVIGATION_PARSER_H_

#include <optional>

#include "third_party/blink/renderer/core/css/parser/conditional_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/route_matching/navigation_preposition.h"

namespace blink {

class CSSParserTokenStream;
class Document;
class NavigationQuery;
class NavigationTestExpression;
class RouteLocation;

class NavigationParser : public ConditionalParser {
 public:
  // https://drafts.csswg.org/css-navigation-1/#typedef-navigation-test
  //
  // <navigation-test> = <navigation-location-test> | <navigation-type-test> |
  // <navigation-phase-test>
  static NavigationTestExpression* ParseNavigationTest(CSSParserTokenStream&,
                                                       const Document&);

  static NavigationQuery* ParseQuery(CSSParserTokenStream&, const Document&);
  static RouteLocation* ParseLocation(CSSParserTokenStream&, const Document&);
  static std::optional<NavigationPreposition> ParsePrepositionIdent(
      CSSParserToken);

  explicit NavigationParser(const Document& document) : document_(document) {}

  const ConditionalExpNode* ConsumeLeaf(CSSParserTokenStream&) final;
  const ConditionalExpNode* ConsumeFunction(CSSParserTokenStream&) final;

 private:
  // TODO(crbug.com/514721936): Parsing shouldn't need a Document. It's
  // currently used to parse URLPattern, but this should be performed later, not
  // during parsing. Several documents may share a style sheet, and, as such,
  // resolving a URLPattern against the Document that happens to be the one
  // involved when parsing the style sheet is just wrong. There may not even be
  // a Document present.
  const Document& document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_NAVIGATION_PARSER_H_
