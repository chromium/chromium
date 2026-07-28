// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/navigation_state.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

const char NavigationState::kSupplementName[] = "NavigationState";

void NavigationState::Trace(Visitor* visitor) const {
  visitor->Trace(source_element_);
  Supplement<Document>::Trace(visitor);
}

NavigationState& NavigationState::Create(Document& document,
                                         const KURL& old_url,
                                         const KURL& new_url,
                                         Element* source_element) {
  NavigationState* navigation_state = MakeGarbageCollected<NavigationState>(
      document, old_url, new_url, source_element);
  Supplement<Document>::ProvideTo<NavigationState>(document, navigation_state);
  if (source_element) {
    source_element->PseudoStateChanged(CSSSelector::kPseudoNavSource);
  }
  return *navigation_state;
}

void NavigationState::AttemptFinishNavigationAndDestroy(Document* document) {
  NavigationState* navigation_state = Get(document);
  if (!navigation_state) {
    return;
  }
  if (RuntimeEnabledFeatures::RouteMatchingEnabled() &&
      !RouteMap::Get(document)->AttemptSetNavigationFinished()) {
    // Something is keeping the navigation active. For instance an active view
    // transition.
    return;
  }

  if (Element* source_element = navigation_state->GetSourceElement()) {
    source_element->PseudoStateChanged(CSSSelector::kPseudoNavSource);
  }

  document->RemoveSupplement<NavigationState>();
}

}  // namespace blink
