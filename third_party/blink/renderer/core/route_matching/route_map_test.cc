// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/route_matching/navigation_state.h"
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
  route_map.AddURLPatternFromLocation(AtomicString("--route1"),
                                      MakePattern("/foo"));
  route_map.AddURLPatternFromLocation(AtomicString("--route2"),
                                      MakePattern("/bar"));

  const URLPattern* pattern1 =
      route_map.FindURLPatternByLocation(AtomicString("--route1"));
  ASSERT_TRUE(pattern1);
  const URLPattern* pattern2 =
      route_map.FindURLPatternByLocation(AtomicString("--route2"));
  ASSERT_TRUE(pattern2);

  // Nothing should match when there's no active navigation.
  EXPECT_FALSE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                  *pattern1));
  EXPECT_FALSE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                  *pattern2));

  Element* source_element = nullptr;
  KURL from = start_url;
  KURL to = start_url;
  NavigationState::Create(GetDocument(), from, to, source_element);
  route_map.SetNavigationStarted();
  EXPECT_TRUE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                 *pattern1));
  EXPECT_FALSE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                  *pattern2));
  route_map.SetCommitted();
  EXPECT_TRUE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                 *pattern1));
  EXPECT_FALSE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                  *pattern2));
  NavigationState::AttemptFinishNavigationAndDestroy(&GetDocument());
  EXPECT_FALSE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                  *pattern1));
  EXPECT_FALSE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                  *pattern2));

  to = KURL("https://example.com/bar");
  NavigationState::Create(GetDocument(), from, to, source_element);
  route_map.SetNavigationStarted();
  EXPECT_TRUE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                 *pattern1));
  EXPECT_FALSE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                  *pattern2));
  GetDocument().SetURL(to);
  route_map.SetCommitted();
  EXPECT_FALSE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                  *pattern1));
  EXPECT_TRUE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                 *pattern2));
  NavigationState::AttemptFinishNavigationAndDestroy(&GetDocument());

  EXPECT_FALSE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                  *pattern1));
  EXPECT_FALSE(route_map.MatchesCurrentNavigation(NavigationPreposition::kAt,
                                                  *pattern2));
}

}  // anonymous namespace

}  // namespace blink
