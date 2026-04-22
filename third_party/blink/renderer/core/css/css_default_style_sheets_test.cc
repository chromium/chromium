// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CSSDefaultStyleSheetsTest : public PageTestBase {};

// Verifies that ResetTextTrackStyleSheet rebuilds media controls RuleSets
// from scratch so that re-adding the text track sheet does not cause
// monotonically growing rule counts.
TEST_F(CSSDefaultStyleSheetsTest, ResetTextTrackPreservesRuleCounts) {
  GetDocument().GetSettings()->SetTextTrackBackgroundColor(
      String("red !important"));

  auto* video = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
  GetDocument().body()->AppendChild(video);

  UpdateAllLifecyclePhasesForTest();

  unsigned media_controls_count = CSSDefaultStyleSheets::Instance()
                                      .DefaultMediaControlsStyle()
                                      ->RuleCount();
  ASSERT_GT(media_controls_count, 0u);

  CSSDefaultStyleSheets::Instance().ResetTextTrackStyleSheet();

  // Ensure the text track sheet was reset. This should result in the text
  // track cue rule being removed from the rule count.
  EXPECT_LT(CSSDefaultStyleSheets::Instance()
                .DefaultMediaControlsStyle()
                ->RuleCount(),
            media_controls_count);

  // Force style recalc so EnsureDefaultStyleSheetsForElement re-creates
  // the text track sheet during the lifecycle update.
  GetDocument().GetStyleEngine().UAStyleChanged();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(CSSDefaultStyleSheets::Instance()
                .DefaultMediaControlsStyle()
                ->RuleCount(),
            media_controls_count);
}

#if DCHECK_IS_ON()  // Requires all_rules_, to find back the rules we add.

// Each test case identifies a Settings setter, the CSS property it maps to,
// and two distinct values. Values are stored without "!important".
struct TextTrackSettingParam {
  const char* name;
  void (SettingsBase::*setter)(const String&);
  CSSPropertyID property_id;
  const char* initial_value;
  const char* changed_value;
};

class TextTrackSettingsTest
    : public CSSDefaultStyleSheetsTest,
      public testing::WithParamInterface<TextTrackSettingParam> {
 protected:
  // Returns the text track video::cue rule. It is always the last rule in
  // AllRulesForTest() since the text track sheet is appended last.
  const StyleRule& TextTrackCueRule() {
    return *CSSDefaultStyleSheets::Instance()
                .DefaultMediaControlsStyle()
                ->AllRulesForTest()
                .back()
                .Rule();
  }

  void SetValue(const char* value) {
    (GetDocument().GetSettings()->*(GetParam().setter))(String(value) +
                                                        " !important");
  }

  void ClearValue() {
    (GetDocument().GetSettings()->*(GetParam().setter))(String());
  }
};

// Verifies that changing a text track setting rebuilds the UA stylesheet
// with the new value without monotonically growing RuleSets.
TEST_P(TextTrackSettingsTest,
       SettingsChangeTriggersTextTrackStyleInvalidation) {
  SetValue(GetParam().initial_value);

  auto* video = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
  GetDocument().body()->AppendChild(video);
  UpdateAllLifecyclePhasesForTest();

  unsigned initial_rule_count = CSSDefaultStyleSheets::Instance()
                                    .DefaultMediaControlsStyle()
                                    ->RuleCount();
  EXPECT_GT(initial_rule_count, 0u);

  EXPECT_EQ(
      TextTrackCueRule().Properties().GetPropertyValue(GetParam().property_id),
      GetParam().initial_value);

  SetValue(GetParam().changed_value);

  EXPECT_TRUE(CSSDefaultStyleSheets::Instance().RuleSetGroupCache().empty());

  UpdateAllLifecyclePhasesForTest();

  // Rule count should be stable (rebuild from scratch, not appending).
  EXPECT_EQ(CSSDefaultStyleSheets::Instance()
                .DefaultMediaControlsStyle()
                ->RuleCount(),
            initial_rule_count);

  EXPECT_EQ(
      TextTrackCueRule().Properties().GetPropertyValue(GetParam().property_id),
      GetParam().changed_value);
}

INSTANTIATE_TEST_SUITE_P(
    CSSDefaultStyleSheetsTest,
    TextTrackSettingsTest,
    testing::Values(
        TextTrackSettingParam{"BackgroundColor",
                              &Settings::SetTextTrackBackgroundColor,
                              CSSPropertyID::kBackgroundColor, "red", "blue"},
        TextTrackSettingParam{"FontFamily", &Settings::SetTextTrackFontFamily,
                              CSSPropertyID::kFontFamily, "serif",
                              "sans-serif"},
        TextTrackSettingParam{"FontStyle", &Settings::SetTextTrackFontStyle,
                              CSSPropertyID::kFontStyle, "normal", "italic"},
        TextTrackSettingParam{"FontVariant", &Settings::SetTextTrackFontVariant,
                              CSSPropertyID::kFontVariant, "normal",
                              "small-caps"},
        TextTrackSettingParam{"TextColor", &Settings::SetTextTrackTextColor,
                              CSSPropertyID::kColor, "green", "yellow"},
        TextTrackSettingParam{"TextShadow", &Settings::SetTextTrackTextShadow,
                              CSSPropertyID::kTextShadow, "black 1px 1px",
                              "white 2px 2px"},
        TextTrackSettingParam{"TextSize", &Settings::SetTextTrackTextSize,
                              CSSPropertyID::kFontSize, "100%", "120%"}),
    [](const testing::TestParamInfo<TextTrackSettingParam>& info) {
      return info.param.name;
    });

#endif  // DCHECK_IS_ON()

}  // namespace blink
