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
  ScopedCSSAnimationDelayStartEndForTest start_end_enabled(false);

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

TEST(CSSPropertyNamesTest, AlternativeAnimationWithDelayStartEnd) {
  // CSSAnimationDelayStartEnd depends on CSSScrollTimeline.
  ScopedScrollTimelineForTest scroll_timeline_enabled(true);

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

TEST(CSSPropertyNamesTest, AlternativeViewTimelineWithInset) {
  {
    ScopedCSSViewTimelineInsetShorthandForTest feature(false);
    EXPECT_EQ(CSSPropertyID::kViewTimeline,
              UnresolvedCSSPropertyID(/* execution_context */ nullptr,
                                      "view-timeline"));
  }

  {
    ScopedCSSViewTimelineInsetShorthandForTest feature(true);
    EXPECT_EQ(CSSPropertyID::kAlternativeViewTimelineWithInset,
              UnresolvedCSSPropertyID(/* execution_context */ nullptr,
                                      "view-timeline"));
  }
}

TEST(CSSPropertyNamesTest, WebkitAlternativeMaskSize) {
  {
    ScopedCSSMaskingInteropForTest scoped_feature(false);
    CSSPropertyID property_id = UnresolvedCSSPropertyID(
        /* execution_context */ nullptr, "-webkit-mask-size");
    EXPECT_EQ(CSSPropertyID::kWebkitMaskSize, property_id);
    EXPECT_FALSE(IsPropertyAlias(property_id));
  }

  {
    ScopedCSSMaskingInteropForTest scoped_feature(true);
    CSSPropertyID property_id = UnresolvedCSSPropertyID(
        /* execution_context */ nullptr, "-webkit-mask-size");
    EXPECT_EQ(CSSPropertyID::kAliasWebkitAlternativeMaskSize, property_id);
    EXPECT_TRUE(IsPropertyAlias(property_id));
    EXPECT_EQ(CSSPropertyID::kMaskSize, ResolveCSSPropertyID(property_id));
  }
}

TEST(CSSPropertyNamesTest, AlternativeMask) {
  {
    ScopedCSSMaskingInteropForTest scoped_feature(false);
    CSSPropertyID property_id = UnresolvedCSSPropertyID(
        /* execution_context */ nullptr, "-webkit-mask");
    EXPECT_EQ(CSSPropertyID::kWebkitMask, property_id);
  }

  {
    ScopedCSSMaskingInteropForTest scoped_feature(true);
    CSSPropertyID property_id = UnresolvedCSSPropertyID(
        /* execution_context */ nullptr, "-webkit-mask");
    EXPECT_EQ(CSSPropertyID::kAliasWebkitAlternativeMask, property_id);
  }
}

}  // namespace blink
