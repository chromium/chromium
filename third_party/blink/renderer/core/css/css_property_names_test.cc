// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(CSSPropertyNamesTest, AlternativeAnimationWithTimeline) {
  {
    ScopedScrollTimelineForTest scroll_timeline_enabled(false);
    ScopedScrollTimelineCurrentTimeForTest current_time_enabled(false);
    EXPECT_EQ(
        CSSPropertyID::kAnimation,
        UnresolvedCSSPropertyID(/* execution_context */ nullptr, "animation"));
  }

  {
    ScopedScrollTimelineForTest scroll_timeline_enabled(true);
    EXPECT_EQ(
        CSSPropertyID::kAlternativeAnimationWithTimeline,
        UnresolvedCSSPropertyID(/* execution_context */ nullptr, "animation"));
  }
}

TEST(CSSPropertyNamesTest, WebkitMaskSize) {
  CSSPropertyID property_id = UnresolvedCSSPropertyID(
      /* execution_context */ nullptr, "-webkit-mask-size");
  EXPECT_EQ(CSSPropertyID::kAliasWebkitMaskSize, property_id);
  EXPECT_TRUE(IsPropertyAlias(property_id));
  EXPECT_EQ(CSSPropertyID::kMaskSize, ResolveCSSPropertyID(property_id));
}

TEST(CSSPropertyNamesTest, WebkitMask) {
  CSSPropertyID property_id = UnresolvedCSSPropertyID(
      /* execution_context */ nullptr, "-webkit-mask");
  EXPECT_EQ(CSSPropertyID::kAliasWebkitMask, property_id);
}

}  // namespace blink
