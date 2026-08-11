// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_map.h"

#include "base/check_is_test.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_activation.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"
#include "third_party/blink/renderer/core/route_matching/navigation_state.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

RouteMap::RouteMap(Document& document) : Supplement<Document>(document) {
  DCHECK(RuntimeEnabledFeatures::RouteMatchingEnabled());
}

RouteMap::RouteMap() : Supplement<Document>(nullptr) {
  CHECK_IS_TEST();
}

void RouteMap::Trace(Visitor* v) const {
  v->Trace(locations_);
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

void RouteMap::AddURLPatternFromLocation(const String& dashed_ident,
                                         URLPattern* url_pattern) {
  DCHECK(dashed_ident.starts_with("--"));
  if (locations_.find(dashed_ident) != locations_.end()) {
    // TODO(crbug.com/436805487): Handle route modificiation and removal.
    return;
  }
  locations_.insert(dashed_ident, url_pattern);
}

const URLPattern* RouteMap::FindURLPatternByLocation(
    const AtomicString& location_name) const {
  const auto it = locations_.find(location_name);
  return it == locations_.end() ? nullptr : it->value;
}

void RouteMap::EstablishNavigationStateFromActivation() {
  NavigationActivation* activation =
      GetDocument().domWindow()->navigation()->activation();
  if (!activation) {
    return;
  }
  NavigationHistoryEntry* to = activation->entry();
  NavigationHistoryEntry* from = activation->from();
  V8NavigationType::Enum nav_type = activation->navigationType().AsEnum();
  if (!to || (!from && nav_type != V8NavigationType::Enum::kReload)) {
    return;
  }
  KURL from_url = from ? from->url() : to->url();
  auto& state = NavigationState::Create(GetDocument(), from_url, to->url(),
                                        /*source_element=*/nullptr);
  if (nav_type == V8NavigationType::Enum::kTraverse) {
    bool back = to->index() < from->index();
    state.SetTraverseType(back ? NavigationState::kBack
                               : NavigationState::kForward);
  } else if (nav_type == V8NavigationType::Enum::kReload) {
    state.SetTraverseType(NavigationState::kReload);
  }
  SetNavigationStarted();
}

void RouteMap::SetNavigationStarted() {
  auto* navigation_state = NavigationState::Get(&GetDocument());
  DCHECK(navigation_state);

  // Need to update active style right away, or view transitions might glitch.
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  style_engine.SetNeedsActiveStyleUpdate(GetDocument());
  style_engine.UpdateActiveStyle();
}

void RouteMap::SetCommitted() {
  auto* navigation_state = NavigationState::Get(&GetDocument());
  DCHECK(navigation_state);
  navigation_state->SetPhase(NavigationPhase::kCommitted);
  NotifyStyleEngineIfNeeded();
}

bool RouteMap::AttemptSetNavigationFinished() {
  if (!NavigationState::Get(&GetDocument())) {
    return true;
  }
  if (ViewTransitionUtils::GetTransition(GetDocument())) {
    // Even if the document has finished loading, the navigation needs to remain
    // active, since there are ongoing view transitions. When view transitions
    // are done, this function will be called again.
    return false;
  }
  NotifyStyleEngineIfNeeded();
  return true;
}

void RouteMap::OnPreviewStart() {
  auto* navigation_state = NavigationState::Get(&GetDocument());
  CHECK(navigation_state);
  CHECK(!navigation_state->IsInPreview());
  CHECK(RuntimeEnabledFeatures::TwoPhaseViewTransitionEnabled());
  navigation_state->SetIsInPreview(true);
  NotifyStyleEngineIfNeeded();
}

void RouteMap::OnPreviewFinished() {
  auto* navigation_state = NavigationState::Get(&GetDocument());
  if (!navigation_state || !navigation_state->IsInPreview()) {
    return;
  }
  CHECK(RuntimeEnabledFeatures::TwoPhaseViewTransitionEnabled());
  navigation_state->SetIsInPreview(false);
  NotifyStyleEngineIfNeeded();
}

bool RouteMap::MatchesCurrentNavigation(NavigationPreposition preposition,
                                        const URLPattern& pattern) const {
  const auto* navigation_state = NavigationState::Get(&GetDocument());
  if (!navigation_state) {
    return false;
  }
  const KURL& url = [&] {
    switch (preposition) {
      case NavigationPreposition::kAt:
        if (navigation_state->GetPhase() == NavigationPhase::kCommitted) {
          return navigation_state->GetNewURL();
        }
        return navigation_state->GetOldURL();
      case NavigationPreposition::kFrom:
        return navigation_state->GetOldURL();
      case NavigationPreposition::kTo:
        return navigation_state->GetNewURL();
    }
  }();

  return !url.IsEmpty() && pattern.Match(url);
}

void RouteMap::NotifyStyleEngineIfNeeded() {
  if (needs_style_update_on_navigation_) {
    GetDocument().GetStyleEngine().NavigationsMayHaveChanged();
  }
}

}  // namespace blink
