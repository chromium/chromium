// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutSVGInlineTest : public RenderingTest {};

TEST_F(LayoutSVGInlineTest, IsChildAllowed) {
  SetBodyInnerHTML(R"HTML(
<svg>
<text>
<textPath><a id="anchor"><textPath />)HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  auto* a = GetLayoutObjectByElementId("anchor");
  // The second <textPath> is not added.
  EXPECT_FALSE(a->SlowFirstChild());
}

TEST_F(LayoutSVGInlineTest, LocalToAncestorPoint) {
  SetBodyInnerHTML(R"HTML(
<style>body { margin:0; }</style>
<div style="height:3px"></div>
<svg width="200" height="100">
<text>
<tspan id="container">abc<a id="target">foo</a></tspan>
</text>
</svg>)HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutSVGInline* container =
      To<LayoutSVGInline>(GetLayoutObjectByElementId("container"));
  EXPECT_NE(target->LocalToAbsolutePoint(PhysicalOffset()),
            target->LocalToAncestorPoint(PhysicalOffset(), container));
}

}  // namespace blink
