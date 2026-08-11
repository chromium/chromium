// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/navigation_query.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/route_matching/navigation_state.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

bool MatchesCurrentNavigation(const Document& document,
                              NavigationPreposition preposition,
                              const URLPattern& pattern) {
  // TODO(crbug.com/436805487): RouteMap doesn't seem like an obvious home for
  // this utility function.
  const auto* route_map = RouteMap::Get(&document);
  DCHECK(route_map);
  return route_map->MatchesCurrentNavigation(preposition, pattern);
}

}  // anonymous namespace

const URLPattern* NavigationLocation::FindOrCreateURLPattern(
    Document& document) const {
  if (type_ == kUrlPattern || type_ == kUrl) {
    // The value is url() or url-pattern().
    V8URLPatternInput* url_pattern_input =
        MakeGarbageCollected<V8URLPatternInput>(value_);
    URLPattern* pattern =
        URLPattern::Create(document.GetExecutionContext()->GetIsolate(),
                           url_pattern_input, document.Url(), IGNORE_EXCEPTION);
    return pattern;
  }
  // The value is an @location dashed-ident.
  DCHECK_EQ(type_, kLocationName);
  if (const auto* route_map = RouteMap::Get(&document)) {
    return route_map->FindURLPatternByLocation(value_);
  }
  return nullptr;
}

bool NavigationLocation::CheckSelectorMatch(
    const Element& element,
    std::optional<NavigationPreposition> preposition) const {
  const auto* anchor = DynamicTo<HTMLAnchorElement>(&element);
  if (!anchor) {
    return false;
  }

  Document& document = element.GetDocument();
  const URLPattern* url_pattern = FindOrCreateURLPattern(document);
  if (!url_pattern || !url_pattern->Match(anchor->Href())) {
    return false;
  }
  return !preposition ||
         MatchesCurrentNavigation(document, *preposition, *url_pattern);
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
  const URLPattern* url_pattern =
      navigation_location_->FindOrCreateURLPattern(document);
  return url_pattern &&
         MatchesCurrentNavigation(document, preposition_, *url_pattern);
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
  const URLPattern* pattern1 =
      navigation_location1_->FindOrCreateURLPattern(document);
  const URLPattern* pattern2 =
      navigation_location2_->FindOrCreateURLPattern(document);
  if (!pattern1 || !pattern2) {
    return false;
  }

  constexpr auto from = NavigationPreposition::kFrom;
  constexpr auto to = NavigationPreposition::kTo;
  return (MatchesCurrentNavigation(document, from, *pattern1) &&
          MatchesCurrentNavigation(document, to, *pattern2)) ||
         (MatchesCurrentNavigation(document, to, *pattern1) &&
          MatchesCurrentNavigation(document, from, *pattern2));
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
    case NavigationState::kReload:
      return type_ == kReload;
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
    case kReload:
      builder.Append("reload");
      break;
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
  RouteMap::Ensure(*document).SetNeedsStyleUpdateOnNavigation();

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
