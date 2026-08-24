// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/navigation_state.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

class NavigationStateTest : public PageTestBase {
 public:
  URLPattern* MakePattern(const String& pattern) {
    auto* input = MakeGarbageCollected<V8URLPatternInput>(pattern);
    return URLPattern::Create(GetDocument().GetExecutionContext()->GetIsolate(),
                              input, GetDocument().Url(), ASSERT_NO_EXCEPTION);
  }
};

TEST_F(NavigationStateTest, Match) {
  KURL start_url("https://example.com/foo");
  GetDocument().SetURL(start_url);

  const URLPattern* pattern1 = MakePattern("/foo");
  const URLPattern* pattern2 = MakePattern("/bar");
  ASSERT_TRUE(pattern1);
  ASSERT_TRUE(pattern2);

  // There should be no NavigationState when there's no active navigation.
  EXPECT_EQ(NavigationState::Get(&GetDocument()), nullptr);

  Element* source_element = nullptr;
  KURL from = start_url;
  KURL to = start_url;
  auto* state =
      NavigationState::Create(GetDocument(), from, to, source_element);
  EXPECT_EQ(NavigationState::Get(&GetDocument()), state);
  state->SetNavigationStarted();
  EXPECT_TRUE(state->Matches(NavigationPreposition::kAt, *pattern1));
  EXPECT_FALSE(state->Matches(NavigationPreposition::kAt, *pattern2));
  state->SetCommitted();
  EXPECT_TRUE(state->Matches(NavigationPreposition::kAt, *pattern1));
  EXPECT_FALSE(state->Matches(NavigationPreposition::kAt, *pattern2));
  NavigationState::AttemptFinishNavigationAndDestroy(&GetDocument());
  EXPECT_EQ(NavigationState::Get(&GetDocument()), nullptr);

  to = KURL("https://example.com/bar");
  state = NavigationState::Create(GetDocument(), from, to, source_element);
  EXPECT_EQ(NavigationState::Get(&GetDocument()), state);
  state->SetNavigationStarted();
  EXPECT_TRUE(state->Matches(NavigationPreposition::kAt, *pattern1));
  EXPECT_FALSE(state->Matches(NavigationPreposition::kAt, *pattern2));
  GetDocument().SetURL(to);
  state->SetCommitted();
  EXPECT_FALSE(state->Matches(NavigationPreposition::kAt, *pattern1));
  EXPECT_TRUE(state->Matches(NavigationPreposition::kAt, *pattern2));
  NavigationState::AttemptFinishNavigationAndDestroy(&GetDocument());
  EXPECT_EQ(NavigationState::Get(&GetDocument()), nullptr);
}

}  // anonymous namespace

}  // namespace blink
