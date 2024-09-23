// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme.h"

#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutThemeTest : public PageTestBase {
 protected:
  void SetHtmlInnerHTML(const char* html_content);
};

void LayoutThemeTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  UpdateAllLifecyclePhasesForTest();
}

inline Color OutlineColor(Element* element) {
  return element->GetComputedStyle()->VisitedDependentColor(
      GetCSSPropertyOutlineColor());
}

inline EBorderStyle OutlineStyle(Element* element) {
  return element->GetComputedStyle()->OutlineStyle();
}

TEST_F(LayoutThemeTest, ChangeFocusRingColor) {
  SetHtmlInnerHTML("<span id=span tabIndex=0>Span</span>");

  Element* span = GetElementById("span");
  EXPECT_NE(nullptr, span);
  EXPECT_NE(nullptr, span->GetLayoutObject());

  Color custom_color = Color::FromRGB(123, 145, 167);

  // Checking unfocused style.
  EXPECT_EQ(EBorderStyle::kNone, OutlineStyle(span));
  EXPECT_NE(custom_color, OutlineColor(span));

  // Do focus.
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  span->Focus();
  UpdateAllLifecyclePhasesForTest();

  // Checking focused style.
  EXPECT_NE(EBorderStyle::kNone, OutlineStyle(span));
  EXPECT_NE(custom_color, OutlineColor(span));

  // Change focus ring color.
  LayoutTheme::GetTheme().SetCustomFocusRingColor(custom_color);
  UpdateAllLifecyclePhasesForTest();

  // Check that the focus ring color is updated.
  EXPECT_NE(EBorderStyle::kNone, OutlineStyle(span));
  EXPECT_EQ(custom_color, OutlineColor(span));
}

// The expectations in the tests below are relying on LayoutThemeDefault.
// LayoutThemeMac doesn't inherit from that class.
#if !BUILDFLAG(IS_MAC)
TEST_F(LayoutThemeTest, SystemColorWithColorScheme) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      #dark {
        color: buttonface;
        color-scheme: light dark;
      }
    </style>
    <div id="dark"></div>
  )HTML");

  Element* dark_element = GetElementById("dark");
  ASSERT_TRUE(dark_element);

  const ComputedStyle* style = dark_element->GetComputedStyle();
  EXPECT_EQ(mojom::blink::ColorScheme::kLight, style->UsedColorScheme());
  EXPECT_EQ(Color(0xef, 0xef, 0xef),
            style->VisitedDependentColor(GetCSSPropertyColor()));

  // Change color scheme to dark.
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  UpdateAllLifecyclePhasesForTest();

  style = dark_element->GetComputedStyle();
  EXPECT_EQ(mojom::blink::ColorScheme::kDark, style->UsedColorScheme());
  EXPECT_EQ(Color(0x6b, 0x6b, 0x6b),
            style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(LayoutThemeTest, SetSelectionColors) {
  LayoutTheme::GetTheme().SetSelectionColors(Color::kBlack, Color::kBlack,
                                             Color::kBlack, Color::kBlack);
  EXPECT_EQ(Color::kBlack,
            LayoutTheme::GetTheme().ActiveSelectionForegroundColor(
                mojom::blink::ColorScheme::kLight));
  {
    // Enabling MobileLayoutTheme switches which instance is returned from
    // LayoutTheme::GetTheme(). Devtools expect SetSelectionColors() to affect
    // both LayoutTheme instances.
    ScopedMobileLayoutThemeForTest scope(true);
    EXPECT_EQ(Color::kBlack,
              LayoutTheme::GetTheme().ActiveSelectionForegroundColor(
                  mojom::blink::ColorScheme::kLight));

    LayoutTheme::GetTheme().SetSelectionColors(Color::kWhite, Color::kWhite,
                                               Color::kWhite, Color::kWhite);
    EXPECT_EQ(Color::kWhite,
              LayoutTheme::GetTheme().ActiveSelectionForegroundColor(
                  mojom::blink::ColorScheme::kLight));
  }
  EXPECT_EQ(Color::kWhite,
            LayoutTheme::GetTheme().ActiveSelectionForegroundColor(
                mojom::blink::ColorScheme::kLight));
}

TEST_F(LayoutThemeTest, SetSelectionColorsNoInvalidation) {
  LayoutTheme::GetTheme().SetSelectionColors(Color::kWhite, Color::kWhite,
                                             Color::kWhite, Color::kWhite);

  SetHtmlInnerHTML("<body>");
  EXPECT_EQ(GetDocument().documentElement()->GetStyleChangeType(),
            StyleChangeType::kNoStyleChange);
  EXPECT_EQ(Color::kWhite,
            LayoutTheme::GetTheme().ActiveSelectionForegroundColor(
                mojom::blink::ColorScheme::kLight));

  // Setting selection colors to the same values should not cause style
  // recalculation.
  LayoutTheme::GetTheme().SetSelectionColors(Color::kWhite, Color::kWhite,
                                             Color::kWhite, Color::kWhite);
  EXPECT_EQ(GetDocument().documentElement()->GetStyleChangeType(),
            StyleChangeType::kNoStyleChange);
}
#endif  // !BUILDFLAG(IS_MAC)

}  // namespace blink
