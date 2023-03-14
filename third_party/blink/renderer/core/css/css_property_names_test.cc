// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(CSSPropertyNamesTest, AlternativeAnimationWithTimeline) {
  ScopedCSSAnimationDelayStartEndForTest start_end_enabled(false);

  {
    ScopedCSSScrollTimelineForTest scroll_timeline_enabled(false);
    EXPECT_EQ(
        CSSPropertyID::kAnimation,
        UnresolvedCSSPropertyID(/* execution_context */ nullptr, "animation"));
  }

  {
    ScopedCSSScrollTimelineForTest scroll_timeline_enabled(true);
    EXPECT_EQ(
        CSSPropertyID::kAlternativeAnimationWithTimeline,
        UnresolvedCSSPropertyID(/* execution_context */ nullptr, "animation"));
  }
}

TEST(CSSPropertyNamesTest, AlternativeAnimationWithDelayStartEnd) {
  // CSSAnimationDelayStartEnd depends on CSSScrollTimeline.
  ScopedCSSScrollTimelineForTest scroll_timeline_enabled(true);

  {
    ScopedCSSAnimationDelayStartEndForTest start_end_enabled(false);
    EXPECT_EQ(
        CSSPropertyID::kAlternativeAnimationWithTimeline,
        UnresolvedCSSPropertyID(/* execution_context */ nullptr, "animation"));
  }

  {
    ScopedCSSAnimationDelayStartEndForTest start_end_enabled(true);
    EXPECT_EQ(
        CSSPropertyID::kAlternativeAnimationWithDelayStartEnd,
        UnresolvedCSSPropertyID(/* execution_context */ nullptr, "animation"));
  }
}

TEST(CSSPropertyNamesTest, AlternativeAnimationDelay) {
  {
    ScopedCSSAnimationDelayStartEndForTest scoped_feature(false);
    EXPECT_EQ(CSSPropertyID::kAnimationDelay,
              UnresolvedCSSPropertyID(/* execution_context */ nullptr,
                                      "animation-delay"));
  }

  {
    ScopedCSSAnimationDelayStartEndForTest scoped_feature(true);
    EXPECT_EQ(CSSPropertyID::kAlternativeAnimationDelay,
              UnresolvedCSSPropertyID(/* execution_context */ nullptr,
                                      "animation-delay"));
  }
}

}  // namespace blink
