// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/apply_dark_mode.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

using ApplyDarkModeCheckTest = RenderingTest;

TEST_F(ApplyDarkModeCheckTest, LightSolidBackgroundAlwaysFiltered) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                                               CSSValueID::kWhite);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                              GetLayoutView()));
}

TEST_F(ApplyDarkModeCheckTest, DarkSolidBackgroundFilteredIfPolicyIsFilterAll) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                                               CSSValueID::kBlack);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                              GetLayoutView()));
}

TEST_F(ApplyDarkModeCheckTest, DarkTransparentBackgroundAlwaysFiltered) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                                               CSSValueID::kTransparent);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                              GetLayoutView()));
}

TEST_F(ApplyDarkModeCheckTest, BackgroundColorNotDefinedAlwaysFiltered) {
  GetDocument().body()->RemoveInlineStyleProperty(
      CSSPropertyID::kBackgroundColor);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_TRUE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                              GetLayoutView()));
}

TEST_F(ApplyDarkModeCheckTest, MetaColorSchemeDark) {
  ScopedCSSColorSchemeForTest css_feature_scope(true);
  ScopedMetaColorSchemeForTest meta_feature_scope(true);
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  ColorSchemeHelper color_scheme_helper;
  color_scheme_helper.SetPreferredColorScheme(GetDocument(),
                                              PreferredColorScheme::kDark);
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  // Opting out of forced darkening when dark is among the supported color
  // schemes for the page.
  EXPECT_FALSE(ShouldApplyDarkModeFilterToPage(
      DarkModePagePolicy::kFilterByBackground, GetLayoutView()));
  EXPECT_FALSE(ShouldApplyDarkModeFilterToPage(DarkModePagePolicy::kFilterAll,
                                               GetLayoutView()));
}

}  // namespace
}  // namespace blink
