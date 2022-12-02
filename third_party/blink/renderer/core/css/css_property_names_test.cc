// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(CSSPropertyNamesTest, AlternativeAnimation) {
  {
    ScopedCSSScrollTimelineForTest scoped_feature(false);
    EXPECT_EQ(
        CSSPropertyID::kAnimation,
        UnresolvedCSSPropertyID(/* execution_context */ nullptr, "animation"));
  }

  {
    ScopedCSSScrollTimelineForTest scoped_feature(true);
    EXPECT_EQ(
        CSSPropertyID::kAlternativeAnimation,
        UnresolvedCSSPropertyID(/* execution_context */ nullptr, "animation"));
  }
}

TEST(CSSPropertyNamesTest, AlternativeAnimationDelay) {
  {
    ScopedCSSScrollTimelineForTest scoped_feature(false);
    EXPECT_EQ(CSSPropertyID::kAnimationDelay,
              UnresolvedCSSPropertyID(/* execution_context */ nullptr,
                                      "animation-delay"));
  }

  {
    ScopedCSSScrollTimelineForTest scoped_feature(true);
    EXPECT_EQ(CSSPropertyID::kAlternativeAnimationDelay,
              UnresolvedCSSPropertyID(/* execution_context */ nullptr,
                                      "animation-delay"));
  }
}

}  // namespace blink
