// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_match_state.h"

#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"

namespace blink {

RouteMatchState* RouteMatchState::Create(const RouteMap& map) {
  RouteMatchState* state = MakeGarbageCollected<RouteMatchState>();
  map.GetActiveRoutes(NavigationPreposition::kAt, &state->at_routes_);
  map.GetActiveRoutes(NavigationPreposition::kFrom, &state->from_routes_);
  map.GetActiveRoutes(NavigationPreposition::kTo, &state->to_routes_);
  return state;
}

bool RouteMatchState::Equals(const RouteMatchState& other) const {
  return at_routes_ == other.at_routes_ && from_routes_ == other.from_routes_ &&
         to_routes_ == other.to_routes_;
}

void RouteMatchState::Trace(Visitor* v) const {
  v->Trace(at_routes_);
  v->Trace(from_routes_);
  v->Trace(to_routes_);
}

}  // namespace blink
