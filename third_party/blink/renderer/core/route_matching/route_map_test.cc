// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

TEST(RouteMapTest, ParseAndMatch) {
  Persistent<RouteMap> route_map = MakeGarbageCollected<RouteMap>();
  route_map->ParseRoutes(R"({
    "routes": [
      {
        "name": "route1",
        "pattern": "/foo"
      },
      {
        "name": "route2",
        "pattern": ["/bar", "/baz"]
      }
    ]
  })");

  EXPECT_TRUE(
      route_map->MatchesRoute(KURL("https://example.com/foo"), "route1"));
  EXPECT_FALSE(
      route_map->MatchesRoute(KURL("https://example.com/bar"), "route1"));

  EXPECT_TRUE(
      route_map->MatchesRoute(KURL("https://example.com/bar"), "route2"));
  EXPECT_TRUE(
      route_map->MatchesRoute(KURL("https://example.com/baz"), "route2"));
  EXPECT_FALSE(
      route_map->MatchesRoute(KURL("https://example.com/foo"), "route2"));
}

TEST(RouteMapTest, GetActiveRoutes) {
  Persistent<RouteMap> route_map = MakeGarbageCollected<RouteMap>();
  route_map->ParseRoutes(R"({
    "routes": [
      {
        "name": "route1",
        "pattern": "/foo"
      },
      {
        "name": "route2",
        "pattern": ["/bar", "/baz"]
      },
      {
        "name": "route3",
        "pattern": "/foo"
      }
    ]
  })");

  HashSet<String> active_routes =
      route_map->GetActiveRoutes(KURL("https://example.com/foo"));
  EXPECT_EQ(2u, active_routes.size());
  EXPECT_TRUE(active_routes.Contains("route1"));
  EXPECT_TRUE(active_routes.Contains("route3"));

  active_routes = route_map->GetActiveRoutes(KURL("https://example.com/bar"));
  EXPECT_EQ(1u, active_routes.size());
  EXPECT_TRUE(active_routes.Contains("route2"));
}

}  // namespace blink
