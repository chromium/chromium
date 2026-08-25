// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/navigation_state.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_activation.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

const char NavigationState::kSupplementName[] = "NavigationState";

void NavigationState::Trace(Visitor* visitor) const {
  visitor->Trace(source_element_);
  Supplement<Document>::Trace(visitor);
}

NavigationState* NavigationState::Create(Document& document,
                                         const KURL& old_url,
                                         const KURL& new_url,
                                         Element* source_element) {
  auto* navigation_state = MakeGarbageCollected<NavigationState>(
      document, old_url, new_url, source_element);
  Supplement<Document>::ProvideTo<NavigationState>(document, navigation_state);
  if (source_element) {
    source_element->PseudoStateChanged(CSSSelector::kPseudoNavigationSource);
  }
  return navigation_state;
}

NavigationState* NavigationState::CreateFromActivation(Document& document) {
  NavigationActivation* activation =
      document.domWindow()->navigation()->activation();
  if (!activation) {
    return nullptr;
  }
  NavigationHistoryEntry* to = activation->entry();
  NavigationHistoryEntry* from = activation->from();
  V8NavigationType::Enum nav_type = activation->navigationType().AsEnum();
  if (!to || (!from && nav_type != V8NavigationType::Enum::kReload)) {
    return nullptr;
  }
  KURL from_url = from ? from->url() : to->url();
  auto* state = NavigationState::Create(document, from_url, to->url(),
                                        /*source_element=*/nullptr);
  if (nav_type == V8NavigationType::Enum::kTraverse) {
    bool back = to->index() < from->index();
    state->SetTraverseType(back ? NavigationState::kBack
                                : NavigationState::kForward);
  } else if (nav_type == V8NavigationType::Enum::kReload) {
    state->SetTraverseType(NavigationState::kReload);
  }
  state->SetNavigationStarted();
  return state;
}

void NavigationState::AttemptFinishNavigationAndDestroy(Document* document) {
  NavigationState* navigation_state = Get(document);
  if (!navigation_state) {
    return;
  }
  if (RuntimeEnabledFeatures::RouteMatchingEnabled()) {
    if (ViewTransitionUtils::GetTransition(*document)) {
      // Even if the document has finished loading, the navigation needs to
      // remain active, since there are ongoing view transitions. When view
      // transitions are done, this function will be called again.
      return;
    }
    navigation_state->NotifyStyleEngineIfNeeded();
  }

  if (Element* source_element = navigation_state->GetSourceElement()) {
    source_element->PseudoStateChanged(CSSSelector::kPseudoNavigationSource);
  }

  document->RemoveSupplement<NavigationState>();
}

void NavigationState::SetNavigationStarted() {
  // Need to update active style right away, or view transitions might glitch.
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  style_engine.SetNeedsActiveStyleUpdate(GetDocument());
  style_engine.UpdateActiveStyle();
}

void NavigationState::SetCommitted() {
  phase_ = NavigationPhase::kCommitted;
  NotifyStyleEngineIfNeeded();
}

void NavigationState::OnPreviewStart() {
  CHECK(!IsInPreview());
  CHECK(RuntimeEnabledFeatures::TwoPhaseViewTransitionEnabled());
  is_in_preview_ = true;
  NotifyStyleEngineIfNeeded();
}

void NavigationState::OnPreviewFinished() {
  if (!is_in_preview_) {
    return;
  }
  CHECK(RuntimeEnabledFeatures::TwoPhaseViewTransitionEnabled());
  is_in_preview_ = false;
  NotifyStyleEngineIfNeeded();
}

bool NavigationState::Matches(NavigationPreposition preposition,
                              const URLPattern& pattern) const {
  const KURL& url = [&] {
    switch (preposition) {
      case NavigationPreposition::kAt:
        if (phase_ == NavigationPhase::kCommitted) {
          return new_url_;
        }
        return old_url_;
      case NavigationPreposition::kFrom:
        return old_url_;
      case NavigationPreposition::kTo:
        return new_url_;
    }
  }();

  return !url.IsEmpty() && pattern.Match(url);
}

void NavigationState::NotifyStyleEngineIfNeeded() {
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  if (style_engine.NeedsStyleUpdateOnNavigation()) {
    style_engine.NavigationsMayHaveChanged();
  }
}

}  // namespace blink
