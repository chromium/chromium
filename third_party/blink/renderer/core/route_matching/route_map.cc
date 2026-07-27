// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_map.h"

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_activation.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"
#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_utils.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

RouteMap::RouteMap(Document& document) : Supplement<Document>(document) {
  DCHECK(RuntimeEnabledFeatures::RouteMatchingEnabled());
}

RouteMap::RouteMap() : Supplement<Document>(nullptr) {
  CHECK_IS_TEST();
}

void RouteMap::Trace(Visitor* v) const {
  v->Trace(routes_);
  v->Trace(anonymous_routes_);
  v->Trace(navigation_state_);
  Supplement<Document>::Trace(v);
}

// BEGIN Supplement support:

const char RouteMap::kSupplementName[] = "RouteMap";

const RouteMap* RouteMap::Get(const Document* document) {
  if (!document) {
    return nullptr;
  }
  return Supplement<Document>::From<RouteMap>(*document);
}

RouteMap* RouteMap::Get(Document* document) {
  if (!document) {
    return nullptr;
  }
  return Supplement<Document>::From<RouteMap>(*document);
}

RouteMap& RouteMap::Ensure(Document& document) {
  RouteMap* route_map = Get(&document);
  if (!route_map) {
    route_map = MakeGarbageCollected<RouteMap>(document);
    Supplement<Document>::ProvideTo<RouteMap>(document, route_map);
  }
  return *route_map;
}

// END Supplement support

void RouteMap::AddRouteFromRule(const String& dashed_ident,
                                URLPattern* url_pattern) {
  DCHECK(dashed_ident.starts_with("--"));

  if (routes_.find(dashed_ident) != routes_.end()) {
    // TODO(crbug.com/436805487): Handle route modificiation and removal.
    return;
  }
  Route* route = MakeGarbageCollected<Route>(GetDocument());
  route->AddPattern(url_pattern);
  routes_.insert(dashed_ident, route);
  SetNeedsStyleUpdateOnNavigation();
  route->UpdateMatchStatus(navigation_state_);
}

void RouteMap::AddAnonymousRoute(const AtomicString& url_pattern_string) {
  Member<Route>& route =
      anonymous_routes_.insert(url_pattern_string, nullptr).stored_value->value;
  if (route) {
    return;
  }

  V8URLPatternInput* url_pattern_input =
      MakeGarbageCollected<V8URLPatternInput>(url_pattern_string);
  const Document& document = GetDocument();
  URLPattern* url_pattern =
      URLPattern::Create(document.GetExecutionContext()->GetIsolate(),
                         url_pattern_input, document.Url(), IGNORE_EXCEPTION);

  route = MakeGarbageCollected<Route>(GetDocument());
  route->AddPattern(url_pattern);
  SetNeedsStyleUpdateOnNavigation();
  route->UpdateMatchStatus(navigation_state_);
}

const Route* RouteMap::FindRoute(const AtomicString& route_name) const {
  const auto it = routes_.find(route_name);
  return it == routes_.end() ? nullptr : it->value;
}

const Route* RouteMap::FindAnonymousRoute(
    const AtomicString& url_pattern_string) const {
  auto it = anonymous_routes_.find(url_pattern_string);
  return it == anonymous_routes_.end() ? nullptr : it->value;
}

void RouteMap::UpdateActiveRoutes() {
#if DCHECK_IS_ON()
  DCHECK(!is_updating_active_routes_);
  base::AutoReset<bool> is_updating(&is_updating_active_routes_, true);
#endif

  for (const auto& entry : routes_) {
    Route& route = *entry.value;
    route.UpdateMatchStatus(navigation_state_);
  }
  for (const auto& entry : anonymous_routes_) {
    Route& route = *entry.value;
    route.UpdateMatchStatus(navigation_state_);
  }
}

void RouteMap::GetActiveRoutesForTesting(NavigationPreposition preposition,
                                         MatchCollection* collection) const {
  collection->clear();
  for (const auto& entry : routes_) {
    Route& route = *entry.value;
    if (route.Matches(preposition)) {
      collection->insert(&route);
    }
  }
  for (const auto& entry : anonymous_routes_) {
    Route& route = *entry.value;
    if (route.Matches(preposition)) {
      collection->insert(&route);
    }
  }
}

void RouteMap::EstablishNavigationStateFromActivation() {
  NavigationActivation* activation =
      GetDocument().domWindow()->navigation()->activation();
  if (!activation) {
    return;
  }
  NavigationHistoryEntry* to = activation->entry();
  NavigationHistoryEntry* from = activation->from();
  if (!to || !from) {
    return;
  }
  OnNavigationStart(from->url(), to->url(), /*source_element=*/nullptr);
}

void RouteMap::OnNavigationStart(const KURL& previous_url,
                                 const KURL& next_url,
                                 Element* source_element) {
  navigation_state_ = MakeGarbageCollected<NavigationState>(
      previous_url, next_url, source_element);
  UpdateActiveRoutes();

  // Need to update active style right away, or view transitions might glitch.
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  style_engine.SetNeedsActiveStyleUpdate(GetDocument());
  style_engine.UpdateActiveStyle();

  if (source_element) {
    source_element->PseudoStateChanged(CSSSelector::kPseudoNavSource);
  }
}

void RouteMap::OnNavigationTraverse(NavigationState::HistoryTraverseType type) {
  DCHECK(navigation_state_);
  navigation_state_->SetTraverseType(type);
  NotifyStyleEngineIfNeeded();
}

void RouteMap::OnNavigationCommitted() {
  DCHECK(navigation_state_);
  navigation_state_->SetPhase(NavigationPhase::kCommitted);
  UpdateActiveRoutes();
  NotifyStyleEngineIfNeeded();
}

void RouteMap::OnNavigationDone() {
  if (!navigation_state_) {
    return;
  }
  if (ViewTransitionUtils::GetTransition(GetDocument())) {
    // Even if the document has finished loading, the navigation needs to remain
    // active, since there are ongoing view transitions. When view transitions
    // are done, this function will be called again.
    return;
  }
  if (Element* source_element = navigation_state_->GetSourceElement()) {
    source_element->PseudoStateChanged(CSSSelector::kPseudoNavSource);
  }
  navigation_state_ = nullptr;
  UpdateActiveRoutes();
  NotifyStyleEngineIfNeeded();
}

void RouteMap::OnPreviewStart() {
  CHECK(navigation_state_);
  CHECK(!navigation_state_->IsInPreview());
  CHECK(RuntimeEnabledFeatures::TwoPhaseViewTransitionEnabled());
  navigation_state_->SetIsInPreview(true);
  NotifyStyleEngineIfNeeded();
}

void RouteMap::OnPreviewFinished() {
  if (!navigation_state_ || !navigation_state_->IsInPreview()) {
    return;
  }
  CHECK(RuntimeEnabledFeatures::TwoPhaseViewTransitionEnabled());
  navigation_state_->SetIsInPreview(false);
  NotifyStyleEngineIfNeeded();
}

// Get the "active navigation URL", given the specified preposition.
//
// https://drafts.csswg.org/css-navigation-1/#active-navigation-url
KURL RouteMap::GetActiveNavigationURL(NavigationPreposition preposition) const {
  if (!navigation_state_) {
    return KURL();
  }
  const NavigationState& state = *navigation_state_;
  DCHECK(GetDocument().Url() == state.GetOldURL() ||
         GetDocument().Url() == state.GetNewURL());

  auto at_old_url = [&] {
    return state.GetPhase() == NavigationPhase::kLoading;
  };

  switch (preposition) {
    case NavigationPreposition::kAt:
      return at_old_url() ? state.GetOldURL() : state.GetNewURL();
    case NavigationPreposition::kFrom:
      return state.GetOldURL();
    case NavigationPreposition::kTo:
      return state.GetNewURL();
    case NavigationPreposition::kWith:
      return !at_old_url() ? state.GetOldURL() : state.GetNewURL();
  }
}

void RouteMap::NotifyStyleEngineIfNeeded() {
  if (needs_style_update_on_navigation_) {
    GetDocument().GetStyleEngine().NavigationsMayHaveChanged();
  }
}

}  // namespace blink
