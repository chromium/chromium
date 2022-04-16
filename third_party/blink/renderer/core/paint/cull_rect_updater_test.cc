// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class CullRectUpdaterTest : public RenderingTest {
 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  CullRect GetCullRect(const char* id) {
    return GetLayoutObjectByElementId(id)->FirstFragment().GetCullRect();
  }

  CullRect GetContentsCullRect(const char* id) {
    return GetLayoutObjectByElementId(id)
        ->FirstFragment()
        .GetContentsCullRect();
  }
};

// TODO(wangxianzhu): Move other cull rect tests from PaintLayerPainterTest
// into this file.

CullRect GetCullRect(const PaintLayer& layer) {
  return layer.GetLayoutObject().FirstFragment().GetCullRect();
}

TEST_F(CullRectUpdaterTest, FixedPositionUnderClipPath) {
  GetDocument().View()->Resize(800, 600);
  SetBodyInnerHTML(R"HTML(
    <div style="height: 100vh"></div>
    <div style="width: 100px; height: 100px; clip-path: inset(0 0 0 0)">
      <div id="fixed" style="position: fixed; top: 0; left: 0; width: 1000px;
                             height: 1000px"></div>
    </div>
  )HTML");

  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), GetCullRect("fixed").Rect());

  GetDocument().GetFrame()->DomWindow()->scrollTo(0, 1000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), GetCullRect("fixed").Rect());

  GetDocument().View()->Resize(800, 1000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 1000), GetCullRect("fixed").Rect());
}

TEST_F(CullRectUpdaterTest, FixedPositionUnderClipPathWillChangeTransform) {
  GetDocument().View()->Resize(800, 600);
  SetBodyInnerHTML(R"HTML(
    <div style="height: 100vh"></div>
    <div style="width: 100px; height: 100px; clip-path: inset(0 0 0 0)">
      <div id="fixed" style="position: fixed; top: 0; left: 0; width: 1000px;
                             height: 1000px; will-change: transform"></div>
    </div>
  )HTML");

  EXPECT_EQ(gfx::Rect(-4000, -4000, 8800, 8600), GetCullRect("fixed").Rect());

  GetDocument().GetFrame()->DomWindow()->scrollTo(0, 1000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(-4000, -4000, 8800, 8600), GetCullRect("fixed").Rect());

  GetDocument().View()->Resize(800, 2000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(-4000, -4000, 8800, 10000), GetCullRect("fixed").Rect());
}

TEST_F(CullRectUpdaterTest, AbsolutePositionUnderNonContainingStackingContext) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  SetBodyInnerHTML(R"HTML(
    <div id="scroller" style="width: 200px; height: 200px; overflow: auto;
                              position: relative">
      <div style="height: 0; overflow: hidden; opacity: 0.5; margin: 250px">
        <div id="absolute"
             style="width: 100px; height: 100px; position: absolute;
                    background: green"></div>
      </div>
    </div>
  )HTML");

  EXPECT_EQ(gfx::Rect(0, 0, 200, 200), GetCullRect("absolute").Rect());

  GetDocument().getElementById("scroller")->scrollTo(200, 200);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(RuntimeEnabledFeatures::LayoutNGEnabled()
                ? gfx::Rect(200, 200, 200, 200)
                : gfx::Rect(150, 200, 200, 200),
            GetCullRect("absolute").Rect());
}

TEST_F(CullRectUpdaterTest, StackedChildOfNonStackingContextScroller) {
  SetBodyInnerHTML(R"HTML(
    <div id="scroller" style="width: 200px; height: 200px; overflow: auto;
                              background: white">
      <div id="child" style="height: 7000px; position: relative"></div>
    </div>
  )HTML");

  auto* scroller = GetDocument().getElementById("scroller");

  EXPECT_EQ(gfx::Rect(0, 0, 200, 4200), GetContentsCullRect("scroller").Rect());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 4200), GetCullRect("child").Rect());

  for (int i = 1000; i < 7000; i += 1000) {
    scroller->scrollTo(0, i);
    UpdateAllLifecyclePhasesForTest();
  }
  // When scrolled to 3800, the cull rect covers the whole scrolling contents.
  // Then we use this full cull rect on further scroll to avoid repaint.
  EXPECT_EQ(gfx::Rect(0, 0, 200, 7000), GetContentsCullRect("scroller").Rect());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 7000), GetCullRect("child").Rect());

  // The full cull rect still applies when the scroller scrolls to the top.
  scroller->scrollTo(0, 0);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 200, 7000), GetContentsCullRect("scroller").Rect());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 7000), GetCullRect("child").Rect());

  // When child needs repaint, it will recalculate its cull rect.
  GetPaintLayerByElementId("child")->SetNeedsRepaint();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 200, 7000), GetContentsCullRect("scroller").Rect());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 4200), GetCullRect("child").Rect());

  // Then scroll to the bottom, child should recalculate it cull rect again.
  scroller->scrollTo(0, 7000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 200, 7000), GetContentsCullRect("scroller").Rect());
  EXPECT_EQ(gfx::Rect(0, 2800, 200, 4200), GetCullRect("child").Rect());
}

TEST_F(CullRectUpdaterTest, ContentsCullRectCoveringWholeContentsRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div id="scroller" style="width: 400px; height: 400px; overflow: scroll">
      <div style="width: 600px; height: 8100px"></div>
      <div id="child" style="will-change: transform; height: 20px"></div>
    </div>
  )HTML");

  EXPECT_EQ(gfx::Rect(0, 0, 600, 4400), GetContentsCullRect("scroller").Rect());
  EXPECT_EQ(gfx::Rect(-4000, -8100, 8600, 4400), GetCullRect("child").Rect());

  auto* scroller = GetDocument().getElementById("scroller");
  scroller->scrollTo(0, 3600);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 600, 8000), GetContentsCullRect("scroller").Rect());
  EXPECT_EQ(gfx::Rect(-4000, -8100, 8600, 8000), GetCullRect("child").Rect());

  scroller->scrollTo(0, 3800);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 600, 8120), GetContentsCullRect("scroller").Rect());
  EXPECT_EQ(gfx::Rect(-4000, -8100, 8600, 8120), GetCullRect("child").Rect());

  scroller->scrollTo(0, 4000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 600, 8120), GetContentsCullRect("scroller").Rect());
  EXPECT_EQ(gfx::Rect(-4000, -8100, 8600, 8120), GetCullRect("child").Rect());
}

TEST_F(CullRectUpdaterTest, SVGForeignObject) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  SetBodyInnerHTML(R"HTML(
    <div id="scroller" style="width: 100px; height: 100px; overflow: scroll">
      <svg id="svg" style="width: 100px; height: 4000px">
        <foreignObject id="foreign" style="width: 500px; height: 1000px">
          <div id="child" style="position: relative">Child</div>
        </foreignObject>
      </svg>
    </div>
  )HTML");

  auto* child = GetPaintLayerByElementId("child");
  auto* foreign = GetPaintLayerByElementId("foreign");
  auto* svg = GetPaintLayerByElementId("svg");
  EXPECT_FALSE(child->NeedsCullRectUpdate());
  EXPECT_FALSE(foreign->DescendantNeedsCullRectUpdate());
  EXPECT_FALSE(svg->DescendantNeedsCullRectUpdate());

  GetDocument().getElementById("scroller")->scrollTo(0, 500);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(child->NeedsCullRectUpdate());
  EXPECT_FALSE(foreign->DescendantNeedsCullRectUpdate());
  EXPECT_FALSE(svg->DescendantNeedsCullRectUpdate());

  child->SetNeedsCullRectUpdate();
  EXPECT_TRUE(child->NeedsCullRectUpdate());
  EXPECT_TRUE(foreign->DescendantNeedsCullRectUpdate());
  EXPECT_TRUE(svg->DescendantNeedsCullRectUpdate());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(child->NeedsCullRectUpdate());
  EXPECT_FALSE(foreign->DescendantNeedsCullRectUpdate());
  EXPECT_FALSE(svg->DescendantNeedsCullRectUpdate());
}

TEST_F(CullRectUpdaterTest, LayerUnderSVGHiddenContainer) {
  SetBodyInnerHTML(R"HTML(
    <div id="div" style="display: contents">
      <svg id="svg1"></svg>
    </div>
    <svg id="svg2">
      <defs id="defs"/>
    </svg>
  )HTML");

  EXPECT_FALSE(GetCullRect("svg1").Rect().IsEmpty());

  GetDocument().getElementById("defs")->appendChild(
      GetDocument().getElementById("div"));
  // This should not crash.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetCullRect("svg1").Rect().IsEmpty());
}

}  // namespace blink
