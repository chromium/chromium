// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

class RouteMapTest : public PageTestBase {
 public:
  RouteMap& GetRouteMap() const { return RouteMap::Ensure(GetDocument()); }

  URLPattern* MakePattern(const String& pattern) {
    auto* input = MakeGarbageCollected<V8URLPatternInput>(pattern);
    return URLPattern::Create(GetDocument().GetExecutionContext()->GetIsolate(),
                              input, GetDocument().Url(), ASSERT_NO_EXCEPTION);
  }
};

TEST_F(RouteMapTest, AddAndMatch) {
  KURL start_url("https://example.com/foo");
  GetDocument().SetURL(start_url);

  RouteMap& route_map = GetRouteMap();
  route_map.AddRouteFromRule("--route1", MakePattern("/foo"));
  route_map.AddRouteFromRule("--route2", MakePattern("/bar"));

  const Route* route1 = route_map.FindRoute(AtomicString("--route1"));
  ASSERT_TRUE(route1);
  const Route* route2 = route_map.FindRoute(AtomicString("--route2"));
  ASSERT_TRUE(route2);

  // Nothing should match when there's no active navigation.
  EXPECT_FALSE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));

  Element* source_element = nullptr;
  KURL from = start_url;
  KURL to = start_url;
  route_map.OnNavigationStart(from, to, source_element);
  EXPECT_TRUE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));
  route_map.OnNavigationCommitted();
  EXPECT_TRUE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));
  route_map.OnNavigationDone();
  EXPECT_FALSE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));

  to = KURL("https://example.com/bar");
  route_map.OnNavigationStart(from, to, source_element);
  EXPECT_TRUE(route1->Matches(NavigationPreposition::kAt));
  EXPECT_FALSE(route2->Matches(NavigationPreposition::kAt));
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
  route_map.AddRouteFromRule("--route1", MakePattern("/foo"));
  route_map.AddRouteFromRule("--route2", MakePattern("/bar"));
  route_map.AddRouteFromRule("--route3", MakePattern("/foo"));

  RouteMap::MatchCollection collection;
  route_map.GetActiveRoutesForTesting(NavigationPreposition::kAt, &collection);
  // No active routes when there's no active navigation.
  EXPECT_EQ(0u, collection.size());

  KURL from = start_url;
  KURL to = start_url;
  Element* source_element = nullptr;
  route_map.OnNavigationStart(from, to, source_element);
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
  route_map.OnNavigationStart(from, to, source_element);
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
