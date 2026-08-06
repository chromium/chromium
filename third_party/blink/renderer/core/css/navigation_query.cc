// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/navigation_query.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/route_matching/navigation_state.h"
#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

const Route* NavigationLocation::FindOrCreateRoute(Document& document) const {
  if (type_ == kUrlPattern || type_ == kUrl) {
    // url-pattern() and url() become anonymous routes. One route for each
    // unique entry.
    RouteMap::Ensure(document).AddAnonymousRoute(value_);
  }
  const auto* route_map = RouteMap::Get(&document);
  if (!route_map) {
    return nullptr;
  }
  switch (type_) {
    case kUrl:
    case kUrlPattern:
      return route_map->FindAnonymousRoute(value_);
    case kLocationName:
      return route_map->FindRoute(value_);
  }
}

bool NavigationLocation::CheckSelectorMatch(
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

void NavigationLocation::SerializeTo(StringBuilder& builder) const {
  DCHECK(!value_.IsNull());
  switch (type_) {
    case kUrlPattern:
      builder.Append("url-pattern(");
      SerializeString(value_, builder);
      builder.Append(")");
      break;
    case kUrl:
      builder.Append("url(");
      SerializeString(value_, builder);
      builder.Append(")");
      break;
    case kLocationName:
      SerializeIdentifier(value_, builder);
      break;
  }
}

void NavigationLocationTestExpression::Trace(Visitor* visitor) const {
  visitor->Trace(navigation_location_);
  NavigationTestExpression::Trace(visitor);
}

bool NavigationLocationTestExpression::Matches(Document& document) const {
  const Route* route = navigation_location_->FindOrCreateRoute(document);
  return route && route->Matches(preposition_);
}

void NavigationLocationTestExpression::SerializeTo(
    StringBuilder& builder) const {
  SerializePrepositionTo(preposition_, builder);
  builder.Append(": ");
  navigation_location_->SerializeTo(builder);
}

void NavigationLocationTestExpression::SerializePrepositionTo(
    NavigationPreposition preposition,
    StringBuilder& builder) {
  switch (preposition) {
    case NavigationPreposition::kAt:
      builder.Append("at");
      break;
    case NavigationPreposition::kFrom:
      builder.Append("from");
      break;
    case NavigationPreposition::kTo:
      builder.Append("to");
      break;
  }
}

void NavigationLocationBetweenTestExpression::Trace(Visitor* visitor) const {
  visitor->Trace(navigation_location1_);
  visitor->Trace(navigation_location2_);
  NavigationTestExpression::Trace(visitor);
}

bool NavigationLocationBetweenTestExpression::Matches(
    Document& document) const {
  const Route* route1 = navigation_location1_->FindOrCreateRoute(document);
  const Route* route2 = navigation_location2_->FindOrCreateRoute(document);
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
  navigation_location1_->SerializeTo(builder);
  builder.Append(" and ");
  navigation_location2_->SerializeTo(builder);
}

bool NavigationPhaseTestExpression::Matches(Document& document) const {
  const auto* state = NavigationState::Get(&document);
  return state && state->GetPhase() == phase_;
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
  }
}

bool NavigationTypeTestExpression::Matches(Document& document) const {
  const auto* state = NavigationState::Get(&document);
  if (!state) {
    return false;
  }
  switch (state->GetTraverseType()) {
    case NavigationState::kNotTraversing:
      return false;
    case NavigationState::kBack:
      return type_ == kTraverse || type_ == kBack;
    case NavigationState::kForward:
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
  const auto* state = NavigationState::Get(&document);
  return state && state->IsInPreview();
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
