// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/navigation_query.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

void NavigationLocation::Trace(Visitor* v) const {
  v->Trace(url_pattern_);
}

const Route* NavigationLocation::FindOrCreateRoute(Document& document) const {
  if (url_pattern_) {
    // A URLPattern becomes an anonymous route. One route for each unique
    // URLPattern.
    RouteMap::Ensure(document).AddAnonymousRoute(url_pattern_);
  }
  const auto* route_map = RouteMap::Get(&document);
  if (!route_map) {
    return nullptr;
  }
  if (url_pattern_) {
    return route_map->FindRoute(url_pattern_);
  }
  return route_map->FindRoute(GetRouteName());
}

void NavigationLocation::SerializeTo(StringBuilder& builder) const {
  DCHECK(!string_.IsNull());
  if (url_pattern_) {
    builder.Append("url-pattern(\"");
    builder.Append(string_);
    builder.Append("\")");
  } else {
    builder.Append(string_);
  }
}

bool NavigationTestExpression::Matches(Document& document) const {
  const Route* route = navigation_location_->FindOrCreateRoute(document);
  return route && route->Matches(preposition_);
}

void NavigationTestExpression::SerializeTo(StringBuilder& builder) const {
  switch (preposition_) {
    case NavigationPreposition::kAt:
      builder.Append("at: ");
      break;
    case NavigationPreposition::kFrom:
      builder.Append("from: ");
      break;
    case NavigationPreposition::kTo:
      builder.Append("to: ");
      break;
  }
  navigation_location_->SerializeTo(builder);
}

void NavigationExpNode::Trace(Visitor* v) const {
  ConditionalExpNode::Trace(v);
  v->Trace(navigation_test_);
}

KleeneValue NavigationExpNode::Evaluate(
    ConditionalExpNodeVisitor& visitor) const {
  return visitor.EvaluateNavigationExpNode(*this);
}

void NavigationExpNode::SerializeTo(StringBuilder& builder) const {
  navigation_test_->SerializeTo(builder);
}

void NavigationQuery::Trace(Visitor* v) const {
  v->Trace(root_exp_);
}

bool NavigationQuery::Evaluate(Document* document) const {
  class Handler : public ConditionalExpNodeVisitor {
    STACK_ALLOCATED();

   public:
    explicit Handler(Document& document) : document_(document) {}

    KleeneValue EvaluateNavigationExpNode(
        const NavigationExpNode& node) override {
      const NavigationTestExpression& test = node.NavigationTest();
      return test.Matches(document_) ? KleeneValue::kTrue : KleeneValue::kFalse;
    }

   private:
    Document& document_;
  };

  Handler handler(*document);
  return root_exp_->Evaluate(handler) == KleeneValue::kTrue;
}

}  // namespace blink
