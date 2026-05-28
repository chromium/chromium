// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/active_navigation_condition.h"

#include "third_party/blink/renderer/core/css/navigation_query.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"

namespace blink {

ActiveNavigationCondition::ActiveNavigationCondition(
    RouteLocation* location,
    NavigationPreposition preposition)
    : route_location_(location), preposition_(preposition) {}

void ActiveNavigationCondition::Trace(Visitor* visitor) const {
  visitor->Trace(route_location_);
}

bool ActiveNavigationCondition::CheckSelectorMatch(
    const Element& element) const {
  const auto* anchor = DynamicTo<HTMLAnchorElement>(&element);
  if (!anchor) {
    return false;
  }
  Document& document = element.GetDocument();
  RouteMap& route_map = RouteMap::Ensure(document);

  // TODO(crbug.com/436805487): Should only need to call this if there's no
  // <route-location>. If there's a route, we should detect match changes
  // automatically when navigation changes.
  route_map.SetEverHadActiveNavigationCondition();

  KURL active_navigation_url = route_map.GetActiveNavigationURL(preposition_);
  KURL href = anchor->Href();
  if (active_navigation_url.IsEmpty() || href.IsEmpty()) {
    return false;
  }

  if (!route_location_) {
    // <active-navigation-condition> is link-href.
    return href == active_navigation_url;
  }

  // <active-navigation-condition> is <route-location>.
  const Route* route = route_location_->FindOrCreateRoute(document);
  if (!route) {
    return false;
  }
  return route->URLPatternMatchesURLAndHref(active_navigation_url, href);
}

void ActiveNavigationCondition::SerializeTo(StringBuilder& builder) const {
  NavigationLocationTestExpression::SerializePrepositionTo(preposition_,
                                                           builder);
  if (route_location_) {
    route_location_->SerializeTo(builder);
  } else {
    // TODO(crbug.com/436805487): Serialize "link-href". And write tests, too.
  }
}

}  // namespace blink
