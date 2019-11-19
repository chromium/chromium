// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme.h"

#include <memory>
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
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

class LayoutThemeTest : public PageTestBase,
                        private ScopedCSSColorSchemeForTest {
 protected:
  LayoutThemeTest() : ScopedCSSColorSchemeForTest(true) {}
  void SetHtmlInnerHTML(const char* html_content);
};

void LayoutThemeTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->SetInnerHTMLFromString(
      String::FromUTF8(html_content));
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

  Element* span = GetDocument().getElementById(AtomicString("span"));
  EXPECT_NE(nullptr, span);
  EXPECT_NE(nullptr, span->GetLayoutObject());

  Color custom_color = MakeRGB(123, 145, 167);

  // Checking unfocused style.
  EXPECT_EQ(EBorderStyle::kNone, OutlineStyle(span));
  EXPECT_NE(custom_color, OutlineColor(span));

  // Do focus.
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  span->focus();
  UpdateAllLifecyclePhasesForTest();

  // Checking focused style.
  EXPECT_NE(EBorderStyle::kNone, OutlineStyle(span));
  EXPECT_NE(custom_color, OutlineColor(span));

  // Change focus ring color.
  LayoutTheme::GetTheme().SetCustomFocusRingColor(custom_color);
  Page::PlatformColorsChanged();
  UpdateAllLifecyclePhasesForTest();

  // Check that the focus ring color is updated.
  EXPECT_NE(EBorderStyle::kNone, OutlineStyle(span));
  EXPECT_EQ(custom_color, OutlineColor(span));
}

TEST_F(LayoutThemeTest, RootElementColor) {
  EXPECT_EQ(Color::kBlack,
            LayoutTheme::GetTheme().RootElementColor(WebColorScheme::kLight));
  EXPECT_EQ(Color::kWhite,
            LayoutTheme::GetTheme().RootElementColor(WebColorScheme::kDark));
}

TEST_F(LayoutThemeTest, RootElementColorChange) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      :root { color-scheme: light dark }
      #initial { color: initial }
    </style>
    <div id="initial"></div>
  )HTML");

  Element* initial = GetDocument().getElementById("initial");
  ASSERT_TRUE(initial);
  ASSERT_TRUE(GetDocument().documentElement());
  const ComputedStyle* document_element_style =
      GetDocument().documentElement()->GetComputedStyle();
  ASSERT_TRUE(document_element_style);
  EXPECT_EQ(Color::kBlack, document_element_style->VisitedDependentColor(
                               GetCSSPropertyColor()));

  const ComputedStyle* initial_style = initial->GetComputedStyle();
  ASSERT_TRUE(initial_style);
  EXPECT_EQ(Color::kBlack,
            initial_style->VisitedDependentColor(GetCSSPropertyColor()));

  // Change color scheme to dark.
  ColorSchemeHelper color_scheme_helper;
  color_scheme_helper.SetPreferredColorScheme(GetDocument(),
                                              PreferredColorScheme::kDark);
  UpdateAllLifecyclePhasesForTest();

  document_element_style = GetDocument().documentElement()->GetComputedStyle();
  ASSERT_TRUE(document_element_style);
  EXPECT_EQ(Color::kWhite, document_element_style->VisitedDependentColor(
                               GetCSSPropertyColor()));

  initial_style = initial->GetComputedStyle();
  ASSERT_TRUE(initial_style);
  // Theming does not change the initial value for color, only the UA style for
  // the root element.
  EXPECT_EQ(Color::kBlack,
            initial_style->VisitedDependentColor(GetCSSPropertyColor()));
}

// The expectations are based on LayoutThemeDefault::SystemColor.
// LayoutThemeMac doesn't use that code path.
#if !defined(OS_MACOSX)
TEST_F(LayoutThemeTest, SystemColorWithColorScheme) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      #dark {
        color: buttonface;
        color-scheme: dark;
      }
    </style>
    <div id="dark"></div>
  )HTML");

  Element* dark_element = GetDocument().getElementById("dark");
  ASSERT_TRUE(dark_element);

  const ComputedStyle* style = dark_element->GetComputedStyle();
  EXPECT_EQ(WebColorScheme::kLight, style->UsedColorScheme());
  EXPECT_EQ(Color(0xdd, 0xdd, 0xdd),
            style->VisitedDependentColor(GetCSSPropertyColor()));

  // Change color scheme to dark.
  ColorSchemeHelper color_scheme_helper;
  color_scheme_helper.SetPreferredColorScheme(GetDocument(),
                                              PreferredColorScheme::kDark);
  UpdateAllLifecyclePhasesForTest();

  style = dark_element->GetComputedStyle();
  EXPECT_EQ(WebColorScheme::kDark, style->UsedColorScheme());
  EXPECT_EQ(Color(0x44, 0x44, 0x44),
            style->VisitedDependentColor(GetCSSPropertyColor()));
}
#endif  // !defined(OS_MACOSX)

}  // namespace blink
