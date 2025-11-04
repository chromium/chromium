// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/route_query.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

RouteTest::RouteTest(const AtomicString& route_name,
                     RoutePreposition preposition)
    : string_(route_name), preposition_(preposition) {}

RouteTest::RouteTest(URLPattern* url_pattern,
                     const AtomicString& original_url_pattern_string,
                     RoutePreposition preposition)
    : url_pattern_(url_pattern),
      string_(original_url_pattern_string),
      preposition_(preposition) {}

void RouteTest::Trace(Visitor* v) const {
  v->Trace(url_pattern_);
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
  switch (route_test_->GetPreposition()) {
    case RoutePreposition::kAt:
      builder.Append("at: ");
      break;
    case RoutePreposition::kFrom:
      builder.Append("from: ");
      break;
    case RoutePreposition::kTo:
      builder.Append("to: ");
      break;
  }
  if (route_test_->GetURLPattern()) {
    builder.Append("urlpattern(\"");
    builder.Append(route_test_->OriginalURLPatternString());
    builder.Append("\")");
  } else {
    DCHECK(!route_test_->GetRouteName().IsNull());
    builder.Append(route_test_->GetRouteName());
  }
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
