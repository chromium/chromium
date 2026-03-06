// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/outline_painter.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"

namespace blink {

using OutlinePainterTest = RenderingTest;

TEST_F(OutlinePainterTest, FocusRingOutset) {
  const auto* initial_style = ComputedStyle::GetInitialStyleSingleton();
  ComputedStyleBuilder builder(*initial_style);
  builder.SetOutlineStyle(EBorderStyle::kSolid);
  builder.SetOutlineStyleIsAuto(true);
  const auto* style = builder.TakeStyle();
  LayoutObject::OutlineInfo info =
      LayoutObject::OutlineInfo::GetFromStyle(*style);
  EXPECT_EQ(2, OutlinePainter::OutlineOutsetExtent(*style, info));
  builder = ComputedStyleBuilder(*style);
  builder.SetEffectiveZoom(4.75);
  style = builder.TakeStyle();
  EXPECT_EQ(10, OutlinePainter::OutlineOutsetExtent(*style, info));
  builder = ComputedStyleBuilder(*style);
  builder.SetEffectiveZoom(10);
  style = builder.TakeStyle();
  EXPECT_EQ(20, OutlinePainter::OutlineOutsetExtent(*style, info));
}

TEST_F(OutlinePainterTest, HugeOutlineWidthOffset) {
  SetBodyInnerHTML(R"HTML(
    <div id=target
         style="outline: 900000000px solid black; outline-offset: 900000000px">
    </div>
  )HTML");
  LayoutObject::OutlineInfo info;
  GetLayoutObjectByElementId("target")->OutlineRects(
      &info, PhysicalOffset(), OutlineType::kDontIncludeBlockInkOverflow);
  const auto& style = GetLayoutObjectByElementId("target")->StyleRef();
  EXPECT_TRUE(style.HasOutline());
  EXPECT_EQ(LayoutUnit::Max().ToInt() * 2,
            OutlinePainter::OutlineOutsetExtent(style, info));
}

// Actually this is not a test for OutlinePainter itself, but it ensures
// that the style logic OutlinePainter depending on is correct.
TEST_F(OutlinePainterTest, OutlineWidthLessThanOne) {
  SetBodyInnerHTML("<div id=target style='outline: 0.2px solid black'></div>");
  const auto& style = GetLayoutObjectByElementId("target")->StyleRef();
  EXPECT_TRUE(style.HasOutline());
  EXPECT_EQ(LayoutUnit(1), style.OutlineWidth());
  LayoutObject::OutlineInfo info =
      LayoutObject::OutlineInfo::GetFromStyle(style);
  EXPECT_EQ(1, OutlinePainter::OutlineOutsetExtent(style, info));
}

TEST_F(OutlinePainterTest, IterateCollapsedPath) {
  const SkPath path = SkPathBuilder()
                          .moveTo(8, 12)
                          .lineTo(8, 4)
                          .lineTo(9, 4)
                          .lineTo(9, 0)
                          .lineTo(9, 0)
                          .lineTo(9, 4)
                          .lineTo(8, 4)
                          .close()
                          .detach();

  // Collapsed contour should not cause crash and should be ignored.
  OutlinePainter::IterateRightAnglePathForTesting(
      path,
      BindRepeating([](const Vector<OutlinePainter::Line>&) { NOTREACHED(); }));
}

TEST_F(OutlinePainterTest, FocusRingRespectsExplicitOutlineColorInDarkMode) {
#if !BUILDFLAG(IS_MAC)
  // Browser renderers set a dark custom focus ring color via prefs.
  LayoutTheme::GetTheme().SetCustomFocusRingColor(Color(0x10, 0x10, 0x10));
#endif

  SetBodyInnerHTML(R"HTML(
    <div id="explicit"
         style="color-scheme: dark; outline: red auto 5px; width: 100px;
                height: 50px;">
    </div>
    <div id="default"
         style="color-scheme: dark; outline: auto 5px; width: 100px;
                height: 50px;">
    </div>
    <div id="webkit"
         style="color-scheme: dark; outline: auto 5px -webkit-focus-ring-color;
                width: 100px; height: 50px;">
    </div>
  )HTML");

  // An explicit outline-color (e.g. red) must not be overridden.
  const auto& explicit_style =
      GetLayoutObjectByElementId("explicit")->StyleRef();
  EXPECT_TRUE(explicit_style.DarkColorScheme());
  Color explicit_color =
      explicit_style.VisitedDependentColor(GetCSSPropertyOutlineColor());
  EXPECT_EQ(Color(0xFF, 0, 0), explicit_color);

  // Default outline-color (currentColor) resolves to the text color,
  // which is white in dark mode.
  const auto& default_style = GetLayoutObjectByElementId("default")->StyleRef();
  EXPECT_TRUE(default_style.DarkColorScheme());
  EXPECT_TRUE(default_style.HasOutlineWithCurrentColor());

  // -webkit-focus-ring-color resolves via FocusRingColor(), which should
  // return white in dark mode on non-Mac platforms.
  const auto& webkit_style = GetLayoutObjectByElementId("webkit")->StyleRef();
  EXPECT_TRUE(webkit_style.DarkColorScheme());
#if !BUILDFLAG(IS_MAC)
  Color webkit_color =
      webkit_style.VisitedDependentColor(GetCSSPropertyOutlineColor());
  EXPECT_EQ(Color::kWhite, LayoutTheme::GetTheme().FocusRingColor(
                               mojom::blink::ColorScheme::kDark));
  EXPECT_EQ(Color::kWhite, webkit_color);
#endif
}

TEST_F(OutlinePainterTest, FocusRingDarkModeColorFlagCanDisableFix) {
#if !BUILDFLAG(IS_MAC)
  RuntimeEnabledFeaturesTestHelpers::
      ScopedFocusRingRespectExplicitOutlineColorInDarkMode scoped_feature(
          false);
  LayoutTheme::GetTheme().SetCustomFocusRingColor(Color(0x10, 0x10, 0x10));
  EXPECT_EQ(Color(0x10, 0x10, 0x10), LayoutTheme::GetTheme().FocusRingColor(
                                         mojom::blink::ColorScheme::kDark));
#endif
}

}  // namespace blink
