// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

class CullRectUpdaterTest : public PaintControllerPaintTestBase {
 protected:
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

TEST_F(CullRectUpdaterTest, PerspectiveDescendants) {
  SetBodyInnerHTML(R"HTML(
    <div style="perspective: 1000px">
      <div style="height: 300px; transform-style: preserve-3d; contain: strict">
        <div id="target" style="transform: rotateX(20deg)">TARGET</div>
      </div>
    </div>
  )HTML");
  EXPECT_TRUE(GetCullRect("target").IsInfinite());
}

class CullRectUpdateOnPaintPropertyChangeTest : public CullRectUpdaterTest {
 protected:
  void Check(const String& old_style,
             const String& new_style,
             bool expected_needs_repaint,
             bool expected_needs_cull_rect_update,
             bool expected_needs_repaint_after_cull_rect_update) {
    UpdateAllLifecyclePhasesExceptPaint(/*update_cull_rects*/ false);
    const auto* target_layer = GetPaintLayerByElementId("target");
    EXPECT_EQ(expected_needs_repaint, target_layer->SelfNeedsRepaint())
        << old_style << " -> " << new_style;
    EXPECT_EQ(expected_needs_cull_rect_update,
              target_layer->NeedsCullRectUpdate())
        << old_style << " -> " << new_style;
    UpdateCullRects();
    EXPECT_EQ(expected_needs_repaint_after_cull_rect_update,
              target_layer->SelfNeedsRepaint())
        << old_style << " -> " << new_style;
  }

  void TestTargetChange(const AtomicString& old_style,
                        const AtomicString& new_style,
                        bool expected_needs_repaint,
                        bool expected_needs_cull_rect_update,
                        bool expected_needs_repaint_after_cull_rect_update) {
    SetBodyInnerHTML(html_);
    auto* target = GetDocument().getElementById("target");
    target->setAttribute(html_names::kStyleAttr, old_style);
    UpdateAllLifecyclePhasesForTest();
    target->setAttribute(html_names::kStyleAttr, new_style);
    Check(old_style, new_style, expected_needs_repaint,
          expected_needs_cull_rect_update,
          expected_needs_repaint_after_cull_rect_update);
  }

  void TestChildChange(const AtomicString& old_style,
                       const AtomicString& new_style,
                       bool expected_needs_repaint,
                       bool expected_needs_cull_rect_update,
                       bool expected_needs_repaint_after_cull_rect_update) {
    SetBodyInnerHTML(html_);
    auto* child = GetDocument().getElementById("child");
    child->setAttribute(html_names::kStyleAttr, old_style);
    UpdateAllLifecyclePhasesForTest();
    child->setAttribute(html_names::kStyleAttr, new_style);
    Check(old_style, new_style, expected_needs_repaint,
          expected_needs_cull_rect_update,
          expected_needs_repaint_after_cull_rect_update);
  }

  void TestTargetScroll(const ScrollOffset& old_scroll_offset,
                        const ScrollOffset& new_scroll_offset,
                        bool expected_needs_repaint,
                        bool expected_needs_cull_rect_update,
                        bool expected_needs_repaint_after_cull_rect_update) {
    SetBodyInnerHTML(html_);
    auto* target = GetDocument().getElementById("target");
    target->scrollTo(old_scroll_offset.x(), old_scroll_offset.y()),
        UpdateAllLifecyclePhasesForTest();
    target->scrollTo(new_scroll_offset.x(), new_scroll_offset.y()),
        Check(String(old_scroll_offset.ToString()),
              String(new_scroll_offset.ToString()), expected_needs_repaint,
              expected_needs_cull_rect_update,
              expected_needs_repaint_after_cull_rect_update);
  }

  String html_ = R"HTML(
    <style>
      #target {
        width: 100px;
        height: 100px;
        position: relative;
        overflow: scroll;
        background: white;
      }
      #child { width: 1000px; height: 1000px; }
    </style>
    <div id="target">
      <div id="child">child</div>
    </div>"
  )HTML";
};

TEST_F(CullRectUpdateOnPaintPropertyChangeTest, Opacity) {
  TestTargetChange("opacity: 0.2", "opacity: 0.8", false, false, false);
  TestTargetChange("opacity: 0.5", "", true, false, true);
  TestTargetChange("", "opacity: 0.5", true, false, true);
  TestTargetChange("will-change: opacity", "will-change: opacity; opacity: 0.5",
                   false, false, false);
  TestTargetChange("will-change: opacity; opacity: 0.5", "will-change: opacity",
                   false, false, false);
}

TEST_F(CullRectUpdateOnPaintPropertyChangeTest, Filter) {
  TestTargetChange("filter: blur(5px)", "filter: blur(8px)", false, false,
                   false);
  TestTargetChange("filter: blur(5px)", "", true, false, true);
  TestTargetChange("", "filter: blur(5px)", true, false, true);
  TestTargetChange("will-change: filter; filter: blur(5px)",
                   "will-change: filter", false, false, false);
  TestTargetChange("will-change: filter",
                   "will-change: filter; filter: blur(5px)", false, false,
                   false);
}

TEST_F(CullRectUpdateOnPaintPropertyChangeTest, Transform) {
  TestTargetChange("transform: translateX(10px)", "transform: translateX(20px)",
                   false, true, false);
  TestTargetChange("transform: translateX(10px)", "", true, true, true);
  TestTargetChange("", "transform: translateX(10px)", true, true, true);
  TestTargetChange("will-change: transform; transform: translateX(10px)",
                   "will-change: transform", false, true, false);
  TestTargetChange("will-change: transform",
                   "will-change: transform; transform: translateX(10px)", false,
                   true, false);
}

TEST_F(CullRectUpdateOnPaintPropertyChangeTest, AnimatingTransform) {
  html_ = html_ + R"HTML(
    <style>
      @keyframes test {
        0% { transform: translateX(0); }
        100% { transform: translateX(200px); }
      }
      #target { animation: test 1s infinite; }
    </style>
  )HTML";
  TestTargetChange("transform: translateX(10px)", "transform: translateX(20px)",
                   false, false, false);
  TestTargetChange("transform: translateX(10px)", "", false, false, false);
  TestTargetChange("", "transform: translateX(10px)", false, false, false);
}

TEST_F(CullRectUpdateOnPaintPropertyChangeTest, ScrollContentsSizeChange) {
  TestChildChange("", "width: 3000px", true, true, true);
  TestChildChange("", "height: 3000px", true, true, true);
  TestChildChange("", "width: 50px; height: 50px", true, true, true);
}

TEST_F(CullRectUpdateOnPaintPropertyChangeTest, SmallContentsScroll) {
  // TODO(wangxianzhu): Optimize for scrollers with small contents.
  TestTargetScroll(ScrollOffset(), ScrollOffset(100, 200), false, true, false);
  TestTargetScroll(ScrollOffset(100, 200), ScrollOffset(1000, 1000), false,
                   true, false);
  TestTargetScroll(ScrollOffset(1000, 1000), ScrollOffset(), false, true,
                   false);
}

TEST_F(CullRectUpdateOnPaintPropertyChangeTest, LargeContentsScroll) {
  html_ = html_ + "<style>#child { width: 10000px; height: 10000px; }</style>";
  // TODO(wangxianzhu): Optimize for small scroll delta.
  TestTargetScroll(ScrollOffset(), ScrollOffset(100, 200), false, true, false);
  TestTargetScroll(ScrollOffset(100, 200), ScrollOffset(8000, 8000), false,
                   true, true);
  TestTargetScroll(ScrollOffset(8000, 8000), ScrollOffset(), false, true, true);
}

}  // namespace blink
