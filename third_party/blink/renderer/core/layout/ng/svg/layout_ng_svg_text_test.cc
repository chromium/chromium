// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

class LayoutNGSVGTextTest : public NGLayoutTest {
 public:
  LayoutNGSVGTextTest() = default;

 private:
  ScopedSVGTextNGForTest svg_text_ng_{true};
};

// DevTools element overlay uses AbsoluteQuads().
TEST_F(LayoutNGSVGTextTest, AbsoluteQuads) {
  SetBodyInnerHTML(R"HTML(
<style>
body { margin:0; padding: 0; }
</style>
<svg xmlns="http://www.w3.org/2000/svg" width="400" height="400">
  <text id="t" font-size="16" x="7" textLength="300">Foo</text>
</svg>)HTML");
  UpdateAllLifecyclePhasesForTest();

  Vector<gfx::QuadF> quads;
  auto* object = GetLayoutObjectByElementId("t");
  object->AbsoluteQuads(quads, 0);
  EXPECT_EQ(1u, quads.size());
  gfx::RectF bounding = quads.back().BoundingBox();
  EXPECT_EQ(7.0f, bounding.x());
  EXPECT_EQ(307.0f, bounding.right());
}

TEST_F(LayoutNGSVGTextTest, LocalVisualRect) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
<style>
body { margin:0; padding: 0; }
text { font-family: Ahem; }
</style>
<svg xmlns="http://www.w3.org/2000/svg" width="400" height="400">
  <text id="t" font-size="20" y="32" rotate="45">Foo</text>
</svg>)HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* object = GetLayoutObjectByElementId("t");
  // The descent of the font is 4px.  The bottom of the visual rect should
  // be greater than 32 + 4 if rotate is specified.
  EXPECT_GT(object->LocalVisualRect().Bottom(), LayoutUnit(36));
}

TEST_F(LayoutNGSVGTextTest, ObjectBoundingBox) {
  SetBodyInnerHTML(R"HTML(
<html>
<body>
<svg xmlns="http://www.w3.org/2000/svg" width="100%" height="100%" viewBox="0 0 480 360">
<text text-anchor="middle" x="240" y="25" font-size="16" id="t">
qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq</text>
</svg>
</body><style>
* { scale: 4294967108 33 -0.297499; }
</style>)HTML");
  UpdateAllLifecyclePhasesForTest();

  gfx::RectF box = GetLayoutObjectByElementId("t")->ObjectBoundingBox();
  EXPECT_FALSE(std::isinf(box.origin().x()));
  EXPECT_FALSE(std::isinf(box.origin().y()));
  EXPECT_FALSE(std::isinf(box.width()));
  EXPECT_FALSE(std::isinf(box.height()));
}

// crbug.com/1285666
TEST_F(LayoutNGSVGTextTest, SubtreeLayout) {
  SetBodyInnerHTML(R"HTML(
<body>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 480 360">
<text x="240" y="25" font-size="16" id="t">foo</text>
<text x="240" y="50" font-size="16" id="t2">bar</text>
</svg>
</body>)HTML");
  UpdateAllLifecyclePhasesForTest();
  LocalFrameView* frame_view = GetFrame().View();
  LayoutView& view = GetLayoutView();
  ASSERT_FALSE(view.NeedsLayout());

  GetElementById("t")->setAttribute("transform", "scale(0.5)");
  GetDocument().UpdateStyleAndLayoutTreeForThisDocument();
  EXPECT_TRUE(frame_view->IsSubtreeLayout());

  ;
  uint32_t pre_layout_count = frame_view->BlockLayoutCountForTesting();
  UpdateAllLifecyclePhasesForTest();
  // Only the <text> and its parent <svg> should be laid out again.
  EXPECT_EQ(2u, frame_view->BlockLayoutCountForTesting() - pre_layout_count);
}

// crbug.com/1320615
TEST_F(LayoutNGSVGTextTest, WillBeRemovedFromTree) {
  SetHtmlInnerHTML(R"HTML(
<body>
<div id="to_be_skipped">
<div id="d">
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 480 360" id="svg">
<text id="t">foo</text>
</svg>
</div>
</div>
</body>)HTML");
  // The <text> is registered to #d, #to_be_skipped, body, ...
  UpdateAllLifecyclePhasesForTest();

  // #d's containing block will be the LayoutView.
  GetElementById("d")->setAttribute("style", "position:absolute;");
  UpdateAllLifecyclePhasesForTest();

  // The <text> should be unregistered from all of ancestors.
  GetElementById("svg")->remove();
  GetElementById("to_be_skipped")
      ->setAttribute("style", "transform:rotate(20deg)");
  UpdateAllLifecyclePhasesForTest();
}

}  // namespace blink
