// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MATCH_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MATCH_STATE_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class Route;
class RouteMap;

class RouteMatchState : public GarbageCollected<RouteMatchState> {
 public:
  using MatchCollection = HeapHashSet<WeakMember<Route>>;

  static RouteMatchState* Create(const RouteMap&);

  bool Equals(const RouteMatchState&) const;

  void Trace(Visitor*) const;

 private:
  // Routes we're currently at.
  MatchCollection at_routes_;

  // Routes we're navigating away from.
  MatchCollection from_routes_;

  // Routes we're navigating to.
  MatchCollection to_routes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_MATCH_STATE_H_
