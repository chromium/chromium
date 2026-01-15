// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MATCH_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MATCH_STATE_H_

#include "third_party/blink/renderer/core/route_matching/route_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class RouteMatchState : public GarbageCollected<RouteMatchState> {
 public:
  static RouteMatchState* Create(const RouteMap&);

  bool Equals(const RouteMatchState&) const;

  void Trace(Visitor*) const;

 private:
  // Routes we're currently at.
  RouteMap::MatchCollection at_routes_;

  // Routes we're navigating away from.
  RouteMap::MatchCollection from_routes_;

  // Routes we're navigating to.
  RouteMap::MatchCollection to_routes_;

  RouteMap::HistoryTraverseType traverse_type_ = RouteMap::kNotTraversing;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MATCH_STATE_H_
