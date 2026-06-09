// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

class RouteMapTest : public PageTestBase {
 public:
  RouteMap& GetRouteMap() const { return RouteMap::Ensure(GetDocument()); }
};

TEST_F(RouteMapTest, ParseAndMatch) {
  KURL start_url("https://example.com/foo");
  GetDocument().SetURL(start_url);

  RouteMap& route_map = GetRouteMap();
  route_map.ParseAndApplyRoutes(R"({
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

  const Route* route1 = route_map.FindRoute("route1");
  ASSERT_TRUE(route1);
  const Route* route2 = route_map.FindRoute("route2");
  ASSERT_TRUE(route2);

  // Nothing should match when there's no active navigation.
  EXPECT_FALSE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));

  KURL from = start_url;
  KURL to = start_url;
  route_map.OnNavigationStart(from, to);
  EXPECT_TRUE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));
  route_map.OnNavigationCommitted();
  EXPECT_TRUE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));
  route_map.OnNavigationDone();
  EXPECT_FALSE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));

  to = KURL("https://example.com/bar");
  route_map.OnNavigationStart(from, to);
  EXPECT_TRUE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));
  GetDocument().SetURL(to);
  route_map.OnNavigationCommitted();
  EXPECT_FALSE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_TRUE(route2->Matches(NavigationPreposition::kAt));
  route_map.OnNavigationDone();
  EXPECT_FALSE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));

  from = to;
  to = KURL("https://example.com/baz");
  route_map.OnNavigationStart(from, to);
  EXPECT_FALSE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_TRUE(route2->Matches(NavigationPreposition::kAt));
  GetDocument().SetURL(to);
  route_map.OnNavigationCommitted();
  EXPECT_FALSE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_TRUE(route2->Matches(NavigationPreposition::kAt));
  route_map.OnNavigationDone();
  EXPECT_FALSE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));
}

TEST_F(RouteMapTest, GetActiveRoutesForTesting) {
  KURL start_url("https://example.com/foo");
  GetDocument().SetURL(start_url);

  RouteMap& route_map = GetRouteMap();
  route_map.ParseAndApplyRoutes(R"({
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

  RouteMap::MatchCollection collection;
  route_map.GetActiveRoutesForTesting(NavigationPreposition::kAt, &collection);
  // No active routes when there's no active navigation.
  EXPECT_EQ(0u, collection.size());

  KURL from = start_url;
  KURL to = start_url;
  route_map.OnNavigationStart(from, to);
  route_map.GetActiveRoutesForTesting(NavigationPreposition::kAt, &collection);
  EXPECT_EQ(2u, collection.size());
  GetDocument().SetURL(to);
  route_map.OnNavigationCommitted();
  route_map.GetActiveRoutesForTesting(NavigationPreposition::kAt, &collection);
  EXPECT_EQ(2u, collection.size());
  route_map.OnNavigationDone();
  route_map.GetActiveRoutesForTesting(NavigationPreposition::kAt, &collection);
  // No active routes when there's no active navigation.
  EXPECT_EQ(0u, collection.size());

  to = KURL("https://example.com/bar");
  route_map.OnNavigationStart(from, to);
  route_map.GetActiveRoutesForTesting(NavigationPreposition::kAt, &collection);
  EXPECT_EQ(2u, collection.size());
  GetDocument().SetURL(to);
  route_map.OnNavigationCommitted();
  route_map.GetActiveRoutesForTesting(NavigationPreposition::kAt, &collection);
  EXPECT_EQ(1u, collection.size());
  route_map.OnNavigationDone();
  route_map.GetActiveRoutesForTesting(NavigationPreposition::kAt, &collection);
  // No active routes when there's no active navigation.
  EXPECT_EQ(0u, collection.size());
}

}  // anonymous namespace

}  // namespace blink
