// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/navigation_query.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

void RouteLocation::Trace(Visitor* v) const {
  v->Trace(url_pattern_);
}

const Route* RouteLocation::FindOrCreateRoute(Document& document) const {
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

bool RouteLocation::CheckSelectorMatch(
    const Element& element,
    std::optional<NavigationPreposition> preposition) const {
  const auto* anchor = DynamicTo<HTMLAnchorElement>(&element);
  if (!anchor) {
    return false;
  }

  const Route* route = FindOrCreateRoute(element.GetDocument());
  return route && route->MatchesUrl(anchor->Href()) &&
         (!preposition || route->Matches(*preposition));
}

void RouteLocation::SerializeTo(StringBuilder& builder) const {
  DCHECK(!string_.IsNull());
  if (url_pattern_) {
    builder.Append("url-pattern(");
    SerializeString(string_, builder);
    builder.Append(")");
  } else {
    SerializeIdentifier(string_, builder);
  }
}

void NavigationLocationTestExpression::Trace(Visitor* visitor) const {
  visitor->Trace(route_location_);
  NavigationTestExpression::Trace(visitor);
}

bool NavigationLocationTestExpression::Matches(Document& document) const {
  const Route* route = route_location_->FindOrCreateRoute(document);
  return route && route->Matches(preposition_);
}

void NavigationLocationTestExpression::SerializeTo(
    StringBuilder& builder) const {
  SerializePrepositionTo(preposition_, builder);
  route_location_->SerializeTo(builder);
}

void NavigationLocationTestExpression::SerializePrepositionTo(
    NavigationPreposition preposition,
    StringBuilder& builder) {
  switch (preposition) {
    case NavigationPreposition::kAt:
      builder.Append("at: ");
      break;
    case NavigationPreposition::kFrom:
      builder.Append("from: ");
      break;
    case NavigationPreposition::kTo:
      builder.Append("to: ");
      break;
    case NavigationPreposition::kWith:
      builder.Append("with: ");
      break;
  }
}

void NavigationLocationBetweenTestExpression::Trace(Visitor* visitor) const {
  visitor->Trace(route_location1_);
  visitor->Trace(route_location2_);
  NavigationTestExpression::Trace(visitor);
}

bool NavigationLocationBetweenTestExpression::Matches(
    Document& document) const {
  const Route* route1 = route_location1_->FindOrCreateRoute(document);
  const Route* route2 = route_location2_->FindOrCreateRoute(document);
  if (!route1 || !route2) {
    return false;
  }
  return (route1->Matches(NavigationPreposition::kFrom) &&
          route2->Matches(NavigationPreposition::kTo)) ||
         (route1->Matches(NavigationPreposition::kTo) &&
          route2->Matches(NavigationPreposition::kFrom));
}

void NavigationLocationBetweenTestExpression::SerializeTo(
    StringBuilder& builder) const {
  builder.Append("between: ");
  route_location1_->SerializeTo(builder);
  builder.Append(" and ");
  route_location2_->SerializeTo(builder);
}

bool NavigationPhaseTestExpression::Matches(Document& document) const {
  const auto* route_map = RouteMap::Get(&document);
  return route_map && route_map->GetPhase() == phase_;
}

void NavigationPhaseTestExpression::SerializeTo(StringBuilder& builder) const {
  builder.Append("phase: ");
  switch (phase_) {
    case NavigationPhase::kLoading:
      builder.Append("loading");
      break;
    case NavigationPhase::kReady:
      builder.Append("ready");
      break;
    case NavigationPhase::kCommitted:
      builder.Append("committed");
      break;
    case NavigationPhase::kInactive:
      NOTREACHED();
  }
}

bool NavigationTypeTestExpression::Matches(Document& document) const {
  const auto* route_map = RouteMap::Get(&document);
  if (!route_map) {
    return false;
  }
  switch (route_map->GetHistoryTraverseType()) {
    case RouteMap::kNotTraversing:
      return false;
    case RouteMap::kBack:
      return type_ == kTraverse || type_ == kBack;
    case RouteMap::kForward:
      return type_ == kTraverse || type_ == kForward;
  }
}

void NavigationTypeTestExpression::SerializeTo(StringBuilder& builder) const {
  builder.Append("history: ");
  switch (type_) {
    case kTraverse:
      builder.Append("traverse");
      break;
    case kBack:
      builder.Append("back");
      break;
    case kForward:
      builder.Append("forward");
      break;
      // TODO(crbug.com/436805487): Support "reload".
  }
}

bool NavigationPreviewTestExpression::Matches(Document& document) const {
  return RouteMap::Get(&document)->IsInPreview();
}

void NavigationPreviewTestExpression::SerializeTo(
    StringBuilder& builder) const {
  builder.Append("preview");
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
  // TODO(crbug.com/436805487): Detect history navigation queries properly,
  // instead of assuming that we have those just because there's at least one
  // @navigation rule to evaluate.
  RouteMap::Ensure(*document).SetHasHistoryRules();

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
