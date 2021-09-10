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
  EXPECT_EQ(7.0f, bounding.MinXMinYCorner().X());
  EXPECT_EQ(307.0f, bounding.MaxXMinYCorner().X());
}

}  // namespace blink
