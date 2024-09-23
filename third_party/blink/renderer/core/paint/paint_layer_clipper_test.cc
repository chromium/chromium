// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_clipper.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class PaintLayerClipperTest : public RenderingTest {
 public:
  PaintLayerClipperTest()
      : RenderingTest(MakeGarbageCollected<EmptyLocalFrameClient>()) {}
};

TEST_F(PaintLayerClipperTest, ParentBackgroundClipRectSubpixelAccumulation) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div style="overflow: hidden; width: 300px;">
      <div id=target style='position: relative; width: 200px; height: 300px'>
    </div>
  )HTML");

  PaintLayer* target_paint_layer = GetPaintLayerByElementId("target");
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           &GetDocument().GetLayoutView()->FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip,
                           PhysicalOffset(LayoutUnit(0.25), LayoutUnit(0.35)));

  ClipRect background_rect_gm;
  target_paint_layer->Clipper().CalculateBackgroundClipRect(context,
                                                            background_rect_gm);

  EXPECT_EQ(PhysicalRect(LayoutUnit(8.25), LayoutUnit(8.34375), LayoutUnit(300),
                         LayoutUnit(300)),
            background_rect_gm.Rect());
}

TEST_F(PaintLayerClipperTest, BackgroundClipRectSubpixelAccumulation) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id=target width=200 height=300 style='position: relative'>
  )HTML");

  PaintLayer* target_paint_layer = GetPaintLayerByElementId("target");
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           &GetDocument().GetLayoutView()->FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip,
                           PhysicalOffset(LayoutUnit(0.25), LayoutUnit(0.35)));

  ClipRect background_rect_gm;
  target_paint_layer->Clipper().CalculateBackgroundClipRect(context,
                                                            background_rect_gm);

  EXPECT_TRUE(background_rect_gm.IsInfinite()) << background_rect_gm;
}

TEST_F(PaintLayerClipperTest, SVGBackgroundClipRectSubpixelAccumulation) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <svg id=target width=200 height=300 style='position: relative'>
      <rect width=400 height=500 fill='blue'/>
    </svg>
  )HTML");

  PaintLayer* target_paint_layer = GetPaintLayerByElementId("target");
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           &GetDocument().GetLayoutView()->FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip,
                           PhysicalOffset(LayoutUnit(0.25), LayoutUnit(0.35)));

  ClipRect background_rect_gm;
  target_paint_layer->Clipper().CalculateBackgroundClipRect(context,
                                                            background_rect_gm);

  EXPECT_TRUE(background_rect_gm.IsInfinite()) << background_rect_gm;
}

TEST_F(PaintLayerClipperTest, LayoutSVGRoot) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <svg id=target width=200 height=300 style='position: relative'>
      <rect width=400 height=500 fill='blue'/>
    </svg>
  )HTML");

  PaintLayer* target_paint_layer = GetPaintLayerByElementId("target");
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           &GetDocument().GetLayoutView()->FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip,
                           PhysicalOffset(LayoutUnit(0.25), LayoutUnit(0.35)));
  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;

  target_paint_layer->Clipper().CalculateRects(
      context, target_paint_layer->GetLayoutObject().FirstFragment(),
      layer_offset, background_rect, foreground_rect);

  EXPECT_EQ(PhysicalRect(LayoutUnit(8.25), LayoutUnit(8.35), LayoutUnit(200),
                         LayoutUnit(300)),
            background_rect.Rect());
  EXPECT_EQ(PhysicalRect(LayoutUnit(8.25), LayoutUnit(8.35), LayoutUnit(200),
                         LayoutUnit(300)),
            foreground_rect.Rect());
  EXPECT_EQ(PhysicalOffset(LayoutUnit(8.25), LayoutUnit(8.35)), layer_offset);
}

TEST_F(PaintLayerClipperTest, ControlClip) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <input id=target style='position:absolute; width: 200px; height: 300px'
        type=button>
  )HTML");

  PaintLayer* target_paint_layer = GetPaintLayerByElementId("target");
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           &GetDocument().GetLayoutView()->FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;

  target_paint_layer->Clipper().CalculateRects(
      context, target_paint_layer->GetLayoutObject().FirstFragment(),
      layer_offset, background_rect, foreground_rect);
  // If the PaintLayer clips overflow, the background rect is intersected with
  // the PaintLayer bounds...
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), background_rect.Rect());
  // and the foreground rect is intersected with the control clip in this case.
  EXPECT_EQ(PhysicalRect(10, 10, 196, 296), foreground_rect.Rect());
  EXPECT_EQ(PhysicalOffset(8, 8), layer_offset);
}

TEST_F(PaintLayerClipperTest, RoundedClip) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id='target' style='position:absolute; width: 200px; height: 300px;
        overflow: hidden; border-radius: 1px'>
    </div>
  )HTML");

  PaintLayer* target_paint_layer = GetPaintLayerByElementId("target");
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           &GetDocument().GetLayoutView()->FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip);

  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;

  target_paint_layer->Clipper().CalculateRects(
      context, target_paint_layer->GetLayoutObject().FirstFragment(),
      layer_offset, background_rect, foreground_rect);

  // Only the foreground rect gets hasRadius set for overflow clipping
  // of descendants.
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), background_rect.Rect());
  EXPECT_FALSE(background_rect.HasRadius());
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), foreground_rect.Rect());
  EXPECT_TRUE(foreground_rect.HasRadius());
  EXPECT_EQ(PhysicalOffset(8, 8), layer_offset);
}

TEST_F(PaintLayerClipperTest, RoundedClipNested) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id='parent' style='position:absolute; width: 200px; height: 300px;
        overflow: hidden; border-radius: 1px'>
      <div id='child' style='position: relative; width: 500px;
           height: 500px'>
      </div>
    </div>
  )HTML");

  PaintLayer* parent_paint_layer = GetPaintLayerByElementId("parent");

  PaintLayer* child_paint_layer = GetPaintLayerByElementId("child");

  ClipRectsContext context(
      parent_paint_layer,
      &parent_paint_layer->GetLayoutObject().FirstFragment());

  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;

  child_paint_layer->Clipper().CalculateRects(
      context, child_paint_layer->GetLayoutObject().FirstFragment(),
      layer_offset, background_rect, foreground_rect);

  EXPECT_EQ(PhysicalRect(0, 0, 200, 300), background_rect.Rect());
  EXPECT_TRUE(background_rect.HasRadius());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 300), foreground_rect.Rect());
  EXPECT_TRUE(foreground_rect.HasRadius());
  EXPECT_EQ(PhysicalOffset(), layer_offset);
}

TEST_F(PaintLayerClipperTest, ControlClipSelect) {
  SetBodyInnerHTML(R"HTML(
    <select id='target' style='position: relative; width: 100px;
        background: none; border: none; padding: 0px 15px 0px 5px;'>
      <option>
        Test long texttttttttttttttttttttttttttttttt
      </option>
    </select>
  )HTML");
  LayoutBox* target = GetLayoutBoxByElementId("target");
  PaintLayer* target_paint_layer = target->Layer();
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           &GetDocument().GetLayoutView()->FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip);

  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;

  target_paint_layer->Clipper().CalculateRects(
      context, target_paint_layer->GetLayoutObject().FirstFragment(),
      layer_offset, background_rect, foreground_rect);

  PhysicalRect content_box_rect = target->PhysicalContentBoxRect();
  EXPECT_GT(foreground_rect.Rect().X(),
            content_box_rect.X() + target->PhysicalLocation().left);
  EXPECT_LE(foreground_rect.Rect().Width(), content_box_rect.Width());
}

TEST_F(PaintLayerClipperTest, LayoutSVGRootChild) {
  SetBodyInnerHTML(R"HTML(
    <svg width=200 height=300 style='position: relative'>
      <foreignObject width=400 height=500>
        <div id=target xmlns='http://www.w3.org/1999/xhtml'
    style='position: relative'></div>
      </foreignObject>
    </svg>
  )HTML");

  PaintLayer* target_paint_layer = GetPaintLayerByElementId("target");
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           &GetDocument().GetLayoutView()->FirstFragment());
  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;

  target_paint_layer->Clipper().CalculateRects(
      context, target_paint_layer->GetLayoutObject().FirstFragment(),
      layer_offset, background_rect, foreground_rect);
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), foreground_rect.Rect());
  EXPECT_EQ(PhysicalOffset(8, 8), layer_offset);
}

TEST_F(PaintLayerClipperTest, ContainPaintClip) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
        style='contain: paint; width: 200px; height: 200px; overflow: auto'>
      <div style='height: 400px'></div>
    </div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  ClipRectsContext context(layer, &layer->GetLayoutObject().FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;

  layer->Clipper().CalculateRects(
      context, layer->GetLayoutObject().FirstFragment(), layer_offset,
      background_rect, foreground_rect);
  EXPECT_TRUE(background_rect.IsInfinite()) << background_rect;
  EXPECT_EQ(background_rect.Rect(), foreground_rect.Rect());
  EXPECT_EQ(PhysicalOffset(), layer_offset);

  ClipRectsContext context_clip(layer,
                                &layer->GetLayoutObject().FirstFragment());

  layer->Clipper().CalculateRects(
      context_clip, layer->GetLayoutObject().FirstFragment(), layer_offset,
      background_rect, foreground_rect);
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), foreground_rect.Rect());
  EXPECT_EQ(PhysicalOffset(), layer_offset);
}

TEST_F(PaintLayerClipperTest, NestedContainPaintClip) {
  SetBodyInnerHTML(R"HTML(
    <div style='contain: paint; width: 200px; height: 200px; overflow:
    auto'>
      <div id='target' style='contain: paint; height: 400px'>
      </div>
    </div>
  )HTML");

  PaintLayer* layer = GetPaintLayerByElementId("target");
  ClipRectsContext context(layer->Parent(),
                           &layer->Parent()->GetLayoutObject().FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;

  layer->Clipper().CalculateRects(
      context, layer->GetLayoutObject().FirstFragment(), layer_offset,
      background_rect, foreground_rect);
  EXPECT_EQ(PhysicalRect(0, 0, 200, 400), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 400), foreground_rect.Rect());
  EXPECT_EQ(PhysicalOffset(), layer_offset);

  ClipRectsContext context_clip(
      layer->Parent(), &layer->Parent()->GetLayoutObject().FirstFragment());

  layer->Clipper().CalculateRects(
      context_clip, layer->GetLayoutObject().FirstFragment(), layer_offset,
      background_rect, foreground_rect);
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), foreground_rect.Rect());
  EXPECT_EQ(PhysicalOffset(), layer_offset);
}

TEST_F(PaintLayerClipperTest, CSSClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 400px; height: 400px; position: absolute;
        clip: rect(0, 50px, 100px, 0);
        clip-path: inset(0%);
      }
    </style>
    <div id='target'></div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");
  ClipRectsContext context(target, &target->GetLayoutObject().FirstFragment());
  PhysicalRect infinite_rect(InfiniteIntRect());
  PhysicalOffset layer_offset = infinite_rect.offset;
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper().CalculateRects(
      context, target->GetLayoutObject().FirstFragment(), layer_offset,
      background_rect, foreground_rect);

  EXPECT_EQ(PhysicalRect(0, 0, 50, 100), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 50, 100), foreground_rect.Rect());
}

TEST_F(PaintLayerClipperTest, Filter) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0 }
      #target {
        filter: drop-shadow(0 3px 4px #333); overflow: hidden;
        width: 100px; height: 200px; border: 40px solid blue; margin: 50px;
      }
    </style>
    <div id='target'></div>
  )HTML");

  PaintLayer* target = GetPaintLayerByElementId("target");

  // First test clip rects in the target layer itself.
  ClipRectsContext context(target, &target->GetLayoutObject().FirstFragment());
  PhysicalRect infinite_rect(InfiniteIntRect());
  PhysicalOffset layer_offset = infinite_rect.offset;
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper().CalculateRects(
      context, target->GetLayoutObject().FirstFragment(), layer_offset,
      background_rect, foreground_rect);

  // The background rect is used to clip stacking context (layer) output.
  // In this case, nothing is above us, thus the infinite rect. However we do
  // clip to the layer's after-filter visual rect as an optimization.
  EXPECT_EQ(PhysicalRect(-12, -9, 204, 304), background_rect.Rect());
  // The foreground rect is used to clip the normal flow contents of the
  // stacking context (layer) thus including the overflow clip.
  EXPECT_EQ(PhysicalRect(40, 40, 100, 200), foreground_rect.Rect());

  // Test mapping to the root layer.
  ClipRectsContext root_context(GetLayoutView().Layer(),
                                &GetLayoutView().FirstFragment());
  background_rect = infinite_rect;
  foreground_rect = infinite_rect;
  target->Clipper().CalculateRects(
      root_context, target->GetLayoutObject().FirstFragment(), layer_offset,
      background_rect, foreground_rect);
  // This includes the filter effect because it's applied before mapping the
  // background rect to the root layer.
  EXPECT_EQ(PhysicalRect(38, 41, 204, 304), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(90, 90, 100, 200), foreground_rect.Rect());
}

TEST_F(PaintLayerClipperTest, IgnoreRootLayerClipWithCSSClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #root {
        width: 400px; height: 400px;
        position: absolute; clip: rect(0, 50px, 100px, 0);
      }
      #target {
        position: relative;
      }
    </style>
    <div id='root'>
      <div id='target'></div>
    </div>
  )HTML");

  PaintLayer* root = GetPaintLayerByElementId("root");
  PaintLayer* target = GetPaintLayerByElementId("target");
  ClipRectsContext context(root, &root->GetLayoutObject().FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalRect infinite_rect(InfiniteIntRect());
  PhysicalOffset layer_offset = infinite_rect.offset;
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper().CalculateRects(
      context, target->GetLayoutObject().FirstFragment(), layer_offset,
      background_rect, foreground_rect);

  EXPECT_TRUE(background_rect.IsInfinite());
  EXPECT_TRUE(foreground_rect.IsInfinite());
}

TEST_F(PaintLayerClipperTest, IgnoreRootLayerClipWithOverflowClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #root {
        width: 400px; height: 400px;
        overflow: hidden;
      }
      #target {
        position: relative;
      }
    </style>
    <div id='root'>
      <div id='target'></div>
    </div>
  )HTML");

  PaintLayer* root = GetPaintLayerByElementId("root");
  PaintLayer* target = GetPaintLayerByElementId("target");
  ClipRectsContext context(root, &root->GetLayoutObject().FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalOffset layer_offset(InfiniteIntRect().origin());
  ClipRect background_rect;
  ClipRect foreground_rect;
  target->Clipper().CalculateRects(
      context, target->GetLayoutObject().FirstFragment(), layer_offset,
      background_rect, foreground_rect);

  EXPECT_TRUE(background_rect.IsInfinite());
  EXPECT_TRUE(foreground_rect.IsInfinite());
}

TEST_F(PaintLayerClipperTest, IgnoreRootLayerClipWithBothClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #root {
        width: 400px; height: 400px;
        position: absolute; clip: rect(0, 50px, 100px, 0);
        overflow: hidden;
      }
      #target {
        position: relative;
      }
    </style>
    <div id='root'>
      <div id='target'></div>
    </div>
  )HTML");

  PaintLayer* root = GetPaintLayerByElementId("root");
  PaintLayer* target = GetPaintLayerByElementId("target");
  ClipRectsContext context(root, &root->GetLayoutObject().FirstFragment(),
                           kIgnoreOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalRect infinite_rect(InfiniteIntRect());
  PhysicalOffset layer_offset = infinite_rect.offset;
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper().CalculateRects(
      context, target->GetLayoutObject().FirstFragment(), layer_offset,
      background_rect, foreground_rect);

  EXPECT_TRUE(background_rect.IsInfinite());
  EXPECT_TRUE(foreground_rect.IsInfinite());
}

TEST_F(PaintLayerClipperTest, Fragmentation) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id=root style='position: relative; width: 200px; height: 100px;
                        columns: 2; column-gap: 0'>
      <div id=target style='width: 100px; height: 200px;
          background: lightblue; position: relative'>
      </div
    </div>
  )HTML");

  PaintLayer* root_paint_layer = GetPaintLayerByElementId("root");
  ClipRectsContext context(root_paint_layer,
                           &root_paint_layer->GetLayoutObject().FirstFragment(),
                           kIgnoreOverlayScrollbarSize);
  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;

  PaintLayer* target_paint_layer = GetPaintLayerByElementId("target");
  FragmentDataIterator iterator(target_paint_layer->GetLayoutObject());
  ASSERT_TRUE(iterator.Advance());
  const FragmentData* second_fragment = iterator.GetFragmentData();
  EXPECT_FALSE(iterator.Advance());

  target_paint_layer->Clipper().CalculateRects(
      context, target_paint_layer->GetLayoutObject().FirstFragment(),
      layer_offset, background_rect, foreground_rect);

  EXPECT_TRUE(background_rect.IsInfinite());
  EXPECT_TRUE(foreground_rect.IsInfinite());
  EXPECT_EQ(PhysicalOffset(), layer_offset);

  target_paint_layer->Clipper().CalculateRects(context, *second_fragment,
                                               layer_offset, background_rect,
                                               foreground_rect);

  EXPECT_TRUE(background_rect.IsInfinite());
  EXPECT_TRUE(foreground_rect.IsInfinite());
  EXPECT_EQ(PhysicalOffset(100, 0), layer_offset);
}

TEST_F(PaintLayerClipperTest, ScrollbarClipBehaviorChild) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id='parent' style='position:absolute; width: 200px; height: 300px;
        overflow: scroll;'>
      <div id='child' style='position: relative; width: 500px;
           height: 500px'>
      </div>
    </div>
  )HTML");

  PaintLayer* parent_paint_layer = GetPaintLayerByElementId("parent");
  PaintLayer* child_paint_layer = GetPaintLayerByElementId("child");
  parent_paint_layer->GetScrollableArea()->SetScrollbarsHiddenIfOverlay(false);
  UpdateAllLifecyclePhasesForTest();

  ClipRectsContext context(
      parent_paint_layer,
      &parent_paint_layer->GetLayoutObject().FirstFragment(),
      kExcludeOverlayScrollbarSizeForHitTesting);

  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;
  child_paint_layer->Clipper().CalculateRects(
      context, child_paint_layer->GetLayoutObject().FirstFragment(),
      layer_offset, background_rect, foreground_rect);

  // The background and foreground rect are clipped by the scrollbar size.
  EXPECT_EQ(PhysicalRect(0, 0, 193, 293), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 193, 293), foreground_rect.Rect());
  EXPECT_EQ(PhysicalOffset(), layer_offset);
}

TEST_F(PaintLayerClipperTest, ScrollbarClipBehaviorChildScrollBetween) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id='parent' style='position:absolute; width: 200px; height: 300px;
        overflow: scroll;'>
      <div id='child' style='position: relative; width: 500px;
           height: 500px'>
      </div>
    </div>
  )HTML");

  Element* parent = GetDocument().getElementById(AtomicString("parent"));
  PaintLayer* root_paint_layer = parent->GetLayoutObject()->View()->Layer();
  PaintLayer* child_paint_layer = GetPaintLayerByElementId("child");
  parent->GetLayoutBox()->GetScrollableArea()->SetScrollbarsHiddenIfOverlay(
      false);
  UpdateAllLifecyclePhasesForTest();

  ClipRectsContext context(root_paint_layer,
                           &root_paint_layer->GetLayoutObject().FirstFragment(),
                           kExcludeOverlayScrollbarSizeForHitTesting);

  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;
  child_paint_layer->Clipper().CalculateRects(
      context, child_paint_layer->GetLayoutObject().FirstFragment(),
      layer_offset, background_rect, foreground_rect);

  // The background and foreground rect are clipped by the scrollbar size.
  EXPECT_EQ(PhysicalRect(8, 8, 193, 293), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(8, 8, 193, 293), foreground_rect.Rect());
  EXPECT_EQ(PhysicalOffset(8, 8), layer_offset);
}

TEST_F(PaintLayerClipperTest, ScrollbarClipBehaviorParent) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id='parent' style='position:absolute; width: 200px; height: 300px;
        overflow: scroll;'>
      <div id='child' style='position: relative; width: 500px;
           height: 500px'>
      </div>
    </div>
  )HTML");

  PaintLayer* parent_paint_layer = GetPaintLayerByElementId("parent");
  parent_paint_layer->GetScrollableArea()->SetScrollbarsHiddenIfOverlay(false);
  UpdateAllLifecyclePhasesForTest();

  ClipRectsContext context(
      parent_paint_layer,
      &parent_paint_layer->GetLayoutObject().FirstFragment(),
      kExcludeOverlayScrollbarSizeForHitTesting);

  PhysicalOffset layer_offset;
  ClipRect background_rect, foreground_rect;
  parent_paint_layer->Clipper().CalculateRects(
      context, parent_paint_layer->GetLayoutObject().FirstFragment(),
      layer_offset, background_rect, foreground_rect);

  // Only the foreground is clipped by the scrollbar size, because we
  // called CalculateRects on the root layer.
  EXPECT_EQ(PhysicalRect(0, 0, 200, 300), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 193, 293), foreground_rect.Rect());
  EXPECT_EQ(PhysicalOffset(), layer_offset);
}

}  // namespace blink
