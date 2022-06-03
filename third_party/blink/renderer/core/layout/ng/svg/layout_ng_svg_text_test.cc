// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

class LayoutNGSVGTextTest : public NGLayoutTest {
 public:
  LayoutNGSVGTextTest() : svg_text_ng_(true) {}

 private:
  ScopedSVGTextNGForTest svg_text_ng_;
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

  Vector<FloatQuad> quads;
  auto* object = GetLayoutObjectByElementId("t");
  object->AbsoluteQuads(quads, 0);
  EXPECT_EQ(1u, quads.size());
  FloatRect bounding = quads.back().BoundingBox();
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

}  // namespace blink
