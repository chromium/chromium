// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/outline_painter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using OutlinePainterTest = RenderingTest;

TEST_F(OutlinePainterTest, FocusRingOutset) {
  auto initial_style = ComputedStyle::CreateInitialStyleSingleton();
  auto style = ComputedStyle::Clone(*initial_style);
  style->SetOutlineStyle(EBorderStyle::kSolid);
  style->SetOutlineStyleIsAuto(true);
  EXPECT_EQ(2, OutlinePainter::OutlineOutsetExtent(*style));
  style->SetEffectiveZoom(4.75);
  EXPECT_EQ(4, OutlinePainter::OutlineOutsetExtent(*style));
  style->SetEffectiveZoom(10);
  EXPECT_EQ(8, OutlinePainter::OutlineOutsetExtent(*style));
}

TEST_F(OutlinePainterTest, HugeOutlineWidthOffset) {
  SetBodyInnerHTML(R"HTML(
    <div id=target
         style="outline: 900000000px solid black; outline-offset: 900000000px">
    </div>
  )HTML");
  const auto& style = GetLayoutObjectByElementId("target")->StyleRef();
  EXPECT_TRUE(style.HasOutline());
  EXPECT_EQ(LayoutUnit::Max().ToInt() * 2,
            OutlinePainter::OutlineOutsetExtent(style));
}

// Actually this is not a test for OutlinePainter itself, but it ensures
// that the style logic OutlinePainter depending on is correct.
TEST_F(OutlinePainterTest, OutlineWidthLessThanOne) {
  SetBodyInnerHTML("<div id=target style='outline: 0.2px solid black'></div>");
  const auto& style = GetLayoutObjectByElementId("target")->StyleRef();
  EXPECT_TRUE(style.HasOutline());
  EXPECT_EQ(LayoutUnit(1), style.OutlineWidth());
  EXPECT_EQ(1, OutlinePainter::OutlineOutsetExtent(style));
}

}  // namespace blink
