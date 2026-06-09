// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/navigation_state.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"

namespace blink {

const NavigationState* NavigationState::Get(const Document* document) {
  const auto* route_map = RouteMap::Get(document);
  if (!route_map) {
    return nullptr;
  }
  return route_map->GetNavigationState();
}

void NavigationState::Trace(Visitor*) const {}

}  // namespace blink
