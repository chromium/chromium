// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/route_query.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"

namespace blink {

RouteTest::RouteTest(const String& route_name, RoutePreposition preposition)
    : location_(route_name), preposition_(preposition) {}

RouteTest::RouteTest(URLPattern* pattern, RoutePreposition preposition)
    : location_(pattern), preposition_(preposition) {}

void RouteTest::Trace(Visitor* v) const {
  if (const auto* pattern = std::get_if<Member<URLPattern>>(&location_)) {
    v->Trace(*pattern);
  }
}

bool RouteTest::Matches(Document& document) const {
  URLPattern* url_pattern = GetURLPattern();
  if (url_pattern) {
    // A URLPattern becomes an anonymous route. One route for each unique
    // URLPattern.
    RouteMap::Ensure(document).AddAnonymousRoute(url_pattern);
  }
  const auto* route_map = RouteMap::Get(&document);
  if (!route_map) {
    return false;
  }
  if (url_pattern) {
    return route_map->MatchesURLPattern(url_pattern, preposition_);
  }
  DCHECK(GetRouteName());
  return route_map->MatchesRoute(GetRouteName(), preposition_);
}

void RouteQueryExpNode::Trace(Visitor* v) const {
  ConditionalExpNode::Trace(v);
  v->Trace(route_test_);
}

KleeneValue RouteQueryExpNode::Evaluate(
    ConditionalExpNodeVisitor& visitor) const {
  return visitor.EvaluateRouteQueryExpNode(*this);
}

void RouteQueryExpNode::SerializeTo(StringBuilder& builder) const {
  // TODO(crbug.com/436805487): Implement this.
  NOTREACHED() << "Not yet implemented.";
}

void RouteQuery::Trace(Visitor* v) const {
  v->Trace(root_exp_);
}

bool RouteQuery::Evaluate(Document* document) const {
  class Handler : public ConditionalExpNodeVisitor {
    STACK_ALLOCATED();

   public:
    explicit Handler(Document& document) : document_(document) {}

    KleeneValue EvaluateRouteQueryExpNode(
        const RouteQueryExpNode& node) override {
      const RouteTest& test = node.GetRouteTest();
      return test.Matches(document_) ? KleeneValue::kTrue : KleeneValue::kFalse;
    }

   private:
    Document& document_;
  };

  Handler handler(*document);
  return root_exp_->Evaluate(handler) == KleeneValue::kTrue;
}

}  // namespace blink
