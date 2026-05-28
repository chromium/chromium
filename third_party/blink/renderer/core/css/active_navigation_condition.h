// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ACTIVE_NAVIGATION_CONDITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ACTIVE_NAVIGATION_CONDITION_H_

#include "third_party/blink/renderer/core/route_matching/navigation_preposition.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Element;
class RouteLocation;
class StringBuilder;

// An active navigation condition, i.e. a :active-navigation() selector. Takes a
// preposition and an optional <route-location>.
//
// See https://drafts.csswg.org/css-navigation-1/#active-navigation-pseudo-class
class ActiveNavigationCondition
    : public GarbageCollected<ActiveNavigationCondition> {
 public:
  ActiveNavigationCondition(RouteLocation*, NavigationPreposition);

  void Trace(Visitor*) const;

  bool CheckSelectorMatch(const Element&) const;
  void SerializeTo(StringBuilder&) const;

 private:
  // This one will not be set if <active-navigation-condition> is link-href.
  Member<RouteLocation> route_location_;

  NavigationPreposition preposition_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ACTIVE_NAVIGATION_CONDITION_H_
