// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_clipper.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
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

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(
      GetDocument().GetLayoutView()->Layer(),
      &GetDocument().GetLayoutView()->FirstFragment(), kUncachedClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip,
      PhysicalOffset(LayoutUnit(0.25), LayoutUnit(0.35)));

  ClipRect background_rect_gm;
  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateBackgroundClipRect(context, background_rect_gm);

  EXPECT_EQ(PhysicalRect(LayoutUnit(8.25), LayoutUnit(8.34375), LayoutUnit(300),
                         LayoutUnit(300)),
            background_rect_gm.Rect());

  ClipRect background_rect_nogm;
  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .CalculateBackgroundClipRect(context, background_rect_nogm);

  EXPECT_EQ(PhysicalRect(LayoutUnit(8.25), LayoutUnit(8.34375), LayoutUnit(300),
                         LayoutUnit(300)),
            background_rect_nogm.Rect());
}

TEST_F(PaintLayerClipperTest, BackgroundClipRectSubpixelAccumulation) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id=target width=200 height=300 style='position: relative'>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(
      GetDocument().GetLayoutView()->Layer(),
      &GetDocument().GetLayoutView()->FirstFragment(), kUncachedClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip,
      PhysicalOffset(LayoutUnit(0.25), LayoutUnit(0.35)));

  ClipRect background_rect_gm;
  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateBackgroundClipRect(context, background_rect_gm);

  EXPECT_GE(background_rect_gm.Rect().Width().ToInt(), 33554422);
  EXPECT_GE(background_rect_gm.Rect().Height().ToInt(), 33554422);

  ClipRect background_rect_nogm;
  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .CalculateBackgroundClipRect(context, background_rect_nogm);

  EXPECT_GE(background_rect_nogm.Rect().Width().ToInt(), 33554422);
  EXPECT_GE(background_rect_nogm.Rect().Height().ToInt(), 33554422);
}

TEST_F(PaintLayerClipperTest, SVGBackgroundClipRectSubpixelAccumulation) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <svg id=target width=200 height=300 style='position: relative'>
      <rect width=400 height=500 fill='blue'/>
    </svg>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(
      GetDocument().GetLayoutView()->Layer(),
      &GetDocument().GetLayoutView()->FirstFragment(), kUncachedClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip,
      PhysicalOffset(LayoutUnit(0.25), LayoutUnit(0.35)));

  ClipRect background_rect_gm;
  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateBackgroundClipRect(context, background_rect_gm);

  EXPECT_GE(background_rect_gm.Rect().Width().ToInt(), 33554422);
  EXPECT_GE(background_rect_gm.Rect().Height().ToInt(), 33554422);

  ClipRect background_rect_nogm;
  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .CalculateBackgroundClipRect(context, background_rect_nogm);

  EXPECT_GE(background_rect_nogm.Rect().Width().ToInt(), 33554422);
  EXPECT_GE(background_rect_nogm.Rect().Height().ToInt(), 33554422);
}

TEST_F(PaintLayerClipperTest, LayoutSVGRoot) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <svg id=target width=200 height=300 style='position: relative'>
      <rect width=400 height=500 fill='blue'/>
    </svg>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  ClipRectsContext context(
      GetDocument().GetLayoutView()->Layer(),
      &GetDocument().GetLayoutView()->FirstFragment(), kUncachedClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip,
      PhysicalOffset(LayoutUnit(0.25), LayoutUnit(0.35)));
  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  EXPECT_EQ(PhysicalRect(LayoutUnit(8.25), LayoutUnit(8.35), LayoutUnit(200),
                         LayoutUnit(300)),
            background_rect.Rect());
  EXPECT_EQ(PhysicalRect(LayoutUnit(8.25), LayoutUnit(8.35), LayoutUnit(200),
                         LayoutUnit(300)),
            foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(LayoutUnit(8.25), LayoutUnit(8.35), LayoutUnit(200),
                         LayoutUnit(300)),
            layer_bounds);
}

TEST_F(PaintLayerClipperTest, ControlClip) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <input id=target style='position:absolute; width: 200px; height: 300px'
        type=button>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  // When RLS is enabled, the LayoutView will have a composited scrolling layer,
  // so don't apply an overflow clip.
  ClipRectsContext context(
      GetDocument().GetLayoutView()->Layer(),
      &GetDocument().GetLayoutView()->FirstFragment(), kUncachedClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);
#if defined(OS_MACOSX)
  // If the PaintLayer clips overflow, the background rect is intersected with
  // the PaintLayer bounds...
  EXPECT_EQ(PhysicalRect(3, 4, 210, 28), background_rect.Rect());
  // and the foreground rect is intersected with the control clip in this case.
  EXPECT_EQ(PhysicalRect(8, 8, 200, 18), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(8, 8, 200, 18), layer_bounds);
#else
  // If the PaintLayer clips overflow, the background rect is intersected with
  // the PaintLayer bounds...
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), background_rect.Rect());
  // and the foreground rect is intersected with the control clip in this case.
  EXPECT_EQ(PhysicalRect(10, 10, 196, 296), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), layer_bounds);
#endif
}

TEST_F(PaintLayerClipperTest, RoundedClip) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id='target' style='position:absolute; width: 200px; height: 300px;
        overflow: hidden; border-radius: 1px'>
    </div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(
      GetDocument().GetLayoutView()->Layer(),
      &GetDocument().GetLayoutView()->FirstFragment(), kUncachedClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip);

  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  // Only the foreground rect gets hasRadius set for overflow clipping
  // of descendants.
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), background_rect.Rect());
  EXPECT_FALSE(background_rect.HasRadius());
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), foreground_rect.Rect());
  EXPECT_TRUE(foreground_rect.HasRadius());
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), layer_bounds);
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

  Element* parent = GetDocument().getElementById("parent");
  PaintLayer* parent_paint_layer =
      ToLayoutBoxModelObject(parent->GetLayoutObject())->Layer();

  Element* child = GetDocument().getElementById("child");
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();

  ClipRectsContext context(
      parent_paint_layer,
      &parent_paint_layer->GetLayoutObject().FirstFragment(),
      kUncachedClipRects);

  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  child_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context,
                      &child_paint_layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  EXPECT_EQ(PhysicalRect(0, 0, 200, 300), background_rect.Rect());
  EXPECT_TRUE(background_rect.HasRadius());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 300), foreground_rect.Rect());
  EXPECT_TRUE(foreground_rect.HasRadius());
  EXPECT_EQ(PhysicalRect(0, 0, 500, 500), layer_bounds);
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
  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  PaintLayer* target_paint_layer = target->Layer();
  ClipRectsContext context(
      GetDocument().GetLayoutView()->Layer(),
      &GetDocument().GetLayoutView()->FirstFragment(), kUncachedClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip);

  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  PhysicalRect content_box_rect = target->PhysicalContentBoxRect();
  EXPECT_GT(foreground_rect.Rect().X(),
            content_box_rect.X() + target->Location().X());
  EXPECT_LT(foreground_rect.Rect().Width(), content_box_rect.Width());
}  // namespace blink

TEST_F(PaintLayerClipperTest, LayoutSVGRootChild) {
  SetBodyInnerHTML(R"HTML(
    <svg width=200 height=300 style='position: relative'>
      <foreignObject width=400 height=500>
        <div id=target xmlns='http://www.w3.org/1999/xhtml'
    style='position: relative'></div>
      </foreignObject>
    </svg>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  ClipRectsContext context(GetDocument().GetLayoutView()->Layer(),
                           &GetDocument().GetLayoutView()->FirstFragment(),
                           kUncachedClipRects);
  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(8, 8, 200, 300), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(8, 8, 400, 0), layer_bounds);
}

TEST_F(PaintLayerClipperTest, ContainPaintClip) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
        style='contain: paint; width: 200px; height: 200px; overflow: auto'>
      <div style='height: 400px'></div>
    </div>
  )HTML");

  PaintLayer* layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(
      layer, &layer->GetLayoutObject().FirstFragment(), kPaintingClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  layer->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context, &layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);
  EXPECT_GE(background_rect.Rect().Width().ToInt(), 33554422);
  EXPECT_GE(background_rect.Rect().Height().ToInt(), 33554422);
  EXPECT_EQ(background_rect.Rect(), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), layer_bounds);

  ClipRectsContext context_clip(
      layer, &layer->GetLayoutObject().FirstFragment(), kUncachedClipRects);

  layer->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context_clip, &layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), layer_bounds);
}

TEST_F(PaintLayerClipperTest, NestedContainPaintClip) {
  SetBodyInnerHTML(R"HTML(
    <div style='contain: paint; width: 200px; height: 200px; overflow:
    auto'>
      <div id='target' style='contain: paint; height: 400px'>
      </div>
    </div>
  )HTML");

  PaintLayer* layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(
      layer->Parent(), &layer->Parent()->GetLayoutObject().FirstFragment(),
      kPaintingClipRects, kIgnorePlatformOverlayScrollbarSize,
      kIgnoreOverflowClip);
  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  layer->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context, &layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);
  EXPECT_EQ(PhysicalRect(0, 0, 200, 400), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 400), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 400), layer_bounds);

  ClipRectsContext context_clip(
      layer->Parent(), &layer->Parent()->GetLayoutObject().FirstFragment(),
      kUncachedClipRects);

  layer->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context_clip, &layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 400), layer_bounds);
}

TEST_F(PaintLayerClipperTest, LocalClipRectFixedUnderTransform) {
  SetBodyInnerHTML(R"HTML(
    <div id='transformed'
        style='will-change: transform; width: 100px; height: 100px;
        overflow: hidden'>
      <div id='fixed'
          style='position: fixed; width: 100px; height: 100px;
          top: -50px'>
       </div>
    </div>
  )HTML");

  PaintLayer* transformed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("transformed"))
          ->Layer();
  PaintLayer* fixed =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("fixed"))->Layer();

  EXPECT_EQ(
      PhysicalRect(0, 0, 100, 100),
      transformed->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
          .LocalClipRect(*transformed));
  EXPECT_EQ(PhysicalRect(0, 50, 100, 100),
            fixed->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
                .LocalClipRect(*transformed));
}

TEST_F(PaintLayerClipperTest, ClearClipRectsRecursive) {
  // CAP will re-use a global GeometryMapper, so this
  // logic does not apply.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 5px; height: 5px; background: blue; overflow: hidden;
      position: relative;
    }
    </style>
    <div id='parent'>
      <div id='child'>
        <div id='grandchild'></div>
      </div>
    </div>
  )HTML");

  PaintLayer* parent =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("parent"))->Layer();
  PaintLayer* child =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("child"))->Layer();

  EXPECT_TRUE(parent->GetClipRectsCache());
  EXPECT_TRUE(child->GetClipRectsCache());

  parent->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .ClearClipRectsIncludingDescendants();

  EXPECT_FALSE(parent->GetClipRectsCache());
  EXPECT_FALSE(child->GetClipRectsCache());
}

TEST_F(PaintLayerClipperTest, ClearClipRectsRecursiveChild) {
  // CAP will re-use a global GeometryMapper, so this
  // logic does not apply.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 5px; height: 5px; background: blue;
      position: relative;
    }
    </style>
    <div id='parent'>
      <div id='child'>
        <div id='grandchild'></div>
      </div>
    </div>
  )HTML");

  PaintLayer* parent =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("parent"))->Layer();
  PaintLayer* child =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("child"))->Layer();

  EXPECT_TRUE(parent->GetClipRectsCache());
  EXPECT_TRUE(child->GetClipRectsCache());

  child->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .ClearClipRectsIncludingDescendants();

  EXPECT_TRUE(parent->GetClipRectsCache());
  EXPECT_FALSE(child->GetClipRectsCache());
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

  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(target, &target->GetLayoutObject().FirstFragment(),
                           kUncachedClipRects);
  PhysicalRect infinite_rect(LayoutRect::InfiniteIntRect());
  PhysicalRect layer_bounds(infinite_rect);
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context, &target->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

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

  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();

  // First test clip rects in the target layer itself.
  ClipRectsContext context(target, &target->GetLayoutObject().FirstFragment(),
                           kUncachedClipRects);
  PhysicalRect infinite_rect(LayoutRect::InfiniteIntRect());
  PhysicalRect layer_bounds(infinite_rect);
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context, &target->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  // The background rect is used to clip stacking context (layer) output.
  // In this case, nothing is above us, thus the infinite rect. However we do
  // clip to the layer's after-filter visual rect as an optimization.
  EXPECT_EQ(PhysicalRect(-12, -9, 204, 304), background_rect.Rect());
  // The foreground rect is used to clip the normal flow contents of the
  // stacking context (layer) thus including the overflow clip.
  EXPECT_EQ(PhysicalRect(40, 40, 100, 200), foreground_rect.Rect());

  // Test without GeometryMapper.
  background_rect = infinite_rect;
  foreground_rect = infinite_rect;
  target->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .CalculateRects(context, nullptr, nullptr, layer_bounds, background_rect,
                      foreground_rect);
  // The non-GeometryMapper path applies the immediate filter effect in
  // background rect.
  EXPECT_EQ(PhysicalRect(-12, -9, 204, 304), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(40, 40, 100, 200), foreground_rect.Rect());

  // Test mapping to the root layer.
  ClipRectsContext root_context(GetLayoutView().Layer(),
                                &GetLayoutView().FirstFragment(),
                                kUncachedClipRects);
  background_rect = infinite_rect;
  foreground_rect = infinite_rect;
  target->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(root_context, &target->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);
  // This includes the filter effect because it's applied before mapping the
  // background rect to the root layer.
  EXPECT_EQ(PhysicalRect(38, 41, 204, 304), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(90, 90, 100, 200), foreground_rect.Rect());

  // Test mapping to the root layer without GeometryMapper.
  background_rect = infinite_rect;
  foreground_rect = infinite_rect;
  target->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .CalculateRects(root_context, nullptr, nullptr, layer_bounds,
                      background_rect, foreground_rect);
  EXPECT_EQ(PhysicalRect(38, 41, 204, 304), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(90, 90, 100, 200), foreground_rect.Rect());
}

// Computed infinite clip rects may not match LayoutRect::InfiniteIntRect()
// due to floating point errors.
static bool IsInfinite(const PhysicalRect& rect) {
  return rect.X().Round() < -10000000 && rect.Right().Round() > 10000000 &&
         rect.Y().Round() < -10000000 && rect.Bottom().Round() > 10000000;
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

  PaintLayer* root =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("root"))->Layer();
  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(
      root, &root->GetLayoutObject().FirstFragment(), kPaintingClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalRect infinite_rect(LayoutRect::InfiniteIntRect());
  PhysicalRect layer_bounds(infinite_rect);
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context, &target->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  EXPECT_TRUE(IsInfinite(background_rect.Rect()));
  EXPECT_TRUE(IsInfinite(foreground_rect.Rect()));
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

  PaintLayer* root =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("root"))->Layer();
  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(
      root, &root->GetLayoutObject().FirstFragment(), kPaintingClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalRect infinite_rect(LayoutRect::InfiniteIntRect());
  PhysicalRect layer_bounds(infinite_rect);
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context, &target->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  EXPECT_TRUE(IsInfinite(background_rect.Rect()));
  EXPECT_TRUE(IsInfinite(foreground_rect.Rect()));
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

  PaintLayer* root =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("root"))->Layer();
  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  ClipRectsContext context(
      root, &root->GetLayoutObject().FirstFragment(), kPaintingClipRects,
      kIgnorePlatformOverlayScrollbarSize, kIgnoreOverflowClip);
  PhysicalRect infinite_rect(LayoutRect::InfiniteIntRect());
  PhysicalRect layer_bounds(infinite_rect);
  ClipRect background_rect(infinite_rect);
  ClipRect foreground_rect(infinite_rect);
  target->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context, &target->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  EXPECT_TRUE(IsInfinite(background_rect.Rect()));
  EXPECT_TRUE(IsInfinite(foreground_rect.Rect()));
}

TEST_F(PaintLayerClipperTest, Fragmentation) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id=root style='width: 200px; height: 100px; columns: 2;
    column-gap: 0'>
      <div id=target style='width: 100px; height: 200px;
          background: lightblue; position: relative'>
      </div
    </div>
  )HTML");

  Element* root = GetDocument().getElementById("root");
  PaintLayer* root_paint_layer =
      ToLayoutBoxModelObject(root->GetLayoutObject())->Layer();
  ClipRectsContext context(
      root_paint_layer, &root_paint_layer->GetLayoutObject().FirstFragment(),
      kUncachedClipRects, kIgnorePlatformOverlayScrollbarSize);
  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_paint_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();
  EXPECT_TRUE(
      target_paint_layer->GetLayoutObject().FirstFragment().NextFragment());
  EXPECT_FALSE(target_paint_layer->GetLayoutObject()
                   .FirstFragment()
                   .NextFragment()
                   ->NextFragment());

  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context,
                      &target_paint_layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  EXPECT_EQ(PhysicalRect(-1000000, -1000000, 2000000, 1000100),
            background_rect.Rect());
  EXPECT_EQ(PhysicalRect(-1000000, -1000000, 2000000, 1000100),
            foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 200), layer_bounds);

  target_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(
          context,
          target_paint_layer->GetLayoutObject().FirstFragment().NextFragment(),
          nullptr, layer_bounds, background_rect, foreground_rect);

  EXPECT_EQ(PhysicalRect(-999900, 0, 2000000, 999900), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(-999900, 0, 2000000, 999900), foreground_rect.Rect());
  // Layer bounds adjusted for pagination offset of second fragment.
  EXPECT_EQ(PhysicalRect(100, -100, 100, 200), layer_bounds);
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

  Element* parent = GetDocument().getElementById("parent");
  PaintLayer* parent_paint_layer =
      ToLayoutBoxModelObject(parent->GetLayoutObject())->Layer();

  Element* child = GetDocument().getElementById("child");
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();

  ClipRectsContext context(
      parent_paint_layer,
      &parent_paint_layer->GetLayoutObject().FirstFragment(),
      kUncachedClipRects, kExcludeOverlayScrollbarSizeForHitTesting);

  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;
  child_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context,
                      &child_paint_layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  // The background and foreground rect are clipped by the scrollbar size.
  EXPECT_EQ(PhysicalRect(0, 0, 193, 293), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 193, 293), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 500, 500), layer_bounds);

  child_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .CalculateRects(context, nullptr, nullptr, layer_bounds, background_rect,
                      foreground_rect);

  // The background and foreground rect are clipped by the scrollbar size.
  EXPECT_EQ(PhysicalRect(0, 0, 193, 293), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 193, 293), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 500, 500), layer_bounds);
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

  Element* parent = GetDocument().getElementById("parent");
  PaintLayer* root_paint_layer = parent->GetLayoutObject()->View()->Layer();

  Element* child = GetDocument().getElementById("child");
  PaintLayer* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();

  ClipRectsContext context(
      root_paint_layer, &root_paint_layer->GetLayoutObject().FirstFragment(),
      kUncachedClipRects, kExcludeOverlayScrollbarSizeForHitTesting);

  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;
  child_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context,
                      &child_paint_layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  // The background and foreground rect are clipped by the scrollbar size.
  EXPECT_EQ(PhysicalRect(8, 8, 193, 293), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(8, 8, 193, 293), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(8, 8, 500, 500), layer_bounds);

  child_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .CalculateRects(context, nullptr, nullptr, layer_bounds, background_rect,
                      foreground_rect);

  // The background and foreground rect are clipped by the scrollbar size.
  EXPECT_EQ(PhysicalRect(8, 8, 193, 293), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(8, 8, 193, 293), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(8, 8, 500, 500), layer_bounds);
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

  Element* parent = GetDocument().getElementById("parent");
  PaintLayer* parent_paint_layer =
      ToLayoutBoxModelObject(parent->GetLayoutObject())->Layer();

  ClipRectsContext context(
      parent_paint_layer,
      &parent_paint_layer->GetLayoutObject().FirstFragment(),
      kUncachedClipRects, kExcludeOverlayScrollbarSizeForHitTesting);

  PhysicalRect layer_bounds;
  ClipRect background_rect, foreground_rect;
  parent_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(context,
                      &parent_paint_layer->GetLayoutObject().FirstFragment(),
                      nullptr, layer_bounds, background_rect, foreground_rect);

  // Only the foreground is clipped by the scrollbar size, because we
  // called CalculateRects on the root layer.
  EXPECT_EQ(PhysicalRect(0, 0, 200, 300), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 193, 293), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 300), layer_bounds);

  parent_paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .CalculateRects(context, nullptr, nullptr, layer_bounds, background_rect,
                      foreground_rect);

  // Only the foreground is clipped by the scrollbar size, because we
  // called CalculateRects on the root layer.
  EXPECT_EQ(PhysicalRect(0, 0, 200, 300), background_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 193, 293), foreground_rect.Rect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 300), layer_bounds);
}

TEST_F(PaintLayerClipperTest, FixedLayerClipRectInDocumentSpace) {
  SetBodyInnerHTML(R"HTML(
    <div style="position:fixed; left:100px; top:200px; width:300px; height:400px; overflow:hidden;">
      <div id="target" style="position:relative;"></div>
    </div>
    <div style="height:3000px;"></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();

  GetDocument().domWindow()->scrollTo(0, 50);
  GetDocument()
      .GetLayoutView()
      ->Layer()
      ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .ClearClipRectsIncludingDescendants();

  {
    ClipRect clip_rect;
    target_layer
        ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
        .CalculateBackgroundClipRect(
            ClipRectsContext(GetDocument().GetLayoutView()->Layer(),
                             &GetDocument().GetLayoutView()->FirstFragment(),
                             kAbsoluteClipRectsIgnoringViewportClip,
                             kIgnorePlatformOverlayScrollbarSize,
                             kIgnoreOverflowClipAndScroll),
            clip_rect);
    EXPECT_EQ(PhysicalRect(100, 250, 300, 400), clip_rect.Rect());
  }

  GetDocument().domWindow()->scrollTo(0, 100);

  {
    ClipRect clip_rect;
    target_layer
        ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
        .CalculateBackgroundClipRect(
            ClipRectsContext(GetDocument().GetLayoutView()->Layer(),
                             &GetDocument().GetLayoutView()->FirstFragment(),
                             kAbsoluteClipRectsIgnoringViewportClip,
                             kIgnorePlatformOverlayScrollbarSize,
                             kIgnoreOverflowClipAndScroll),
            clip_rect);
    EXPECT_EQ(PhysicalRect(100, 300, 300, 400), clip_rect.Rect());
  }
}

TEST_F(PaintLayerClipperTest,
       FixedLayerClipRectInDocumentSpaceWithNestedScroller) {
  SetBodyInnerHTML(R"HTML(
    <div style="position:fixed; left:100px; top:200px; width:300px; height:400px; overflow:scroll;">
      <div style="width:200px; height:300px; overflow:hidden;">
        <div id="target" style="position:relative;"></div>
      </div>
      <div style="height:3000px;"></div>
    </div>
    <div style="height:3000px;"></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();

  GetDocument().domWindow()->scrollTo(0, 50);
  GetDocument()
      .GetLayoutView()
      ->Layer()
      ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .ClearClipRectsIncludingDescendants();

  {
    ClipRect clip_rect;
    target_layer
        ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
        .CalculateBackgroundClipRect(
            ClipRectsContext(GetDocument().GetLayoutView()->Layer(),
                             &GetDocument().GetLayoutView()->FirstFragment(),
                             kAbsoluteClipRectsIgnoringViewportClip,
                             kIgnorePlatformOverlayScrollbarSize,
                             kIgnoreOverflowClipAndScroll),
            clip_rect);
    EXPECT_EQ(PhysicalRect(100, 250, 200, 300), clip_rect.Rect());
  }

  GetDocument().domWindow()->scrollTo(0, 100);

  {
    ClipRect clip_rect;
    target_layer
        ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
        .CalculateBackgroundClipRect(
            ClipRectsContext(GetDocument().GetLayoutView()->Layer(),
                             &GetDocument().GetLayoutView()->FirstFragment(),
                             kAbsoluteClipRectsIgnoringViewportClip,
                             kIgnorePlatformOverlayScrollbarSize,
                             kIgnoreOverflowClipAndScroll),
            clip_rect);
    EXPECT_EQ(PhysicalRect(100, 300, 200, 300), clip_rect.Rect());
  }
}

TEST_F(PaintLayerClipperTest,
       StickyLayerClipRectInDocumentSpaceWithNestedScroller) {
  SetBodyInnerHTML(R"HTML(
    <div style="width:200px; height:200px;"></div>
    <div style="position:sticky; left:100px; top:100px; width:300px; height:400px; overflow:scroll;">
      <div style="width:200px; height:300px; overflow:hidden;">
        <div id="target" style="position:relative;"></div>
      </div>
      <div style="height:3000px;"></div>
    </div>
    <div style="height:3000px;"></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  PaintLayer* target_layer =
      ToLayoutBoxModelObject(target->GetLayoutObject())->Layer();

  // At 10, target is still scrolling - clip_rect shouldn't change.
  GetDocument().domWindow()->scrollTo(0, 10);
  GetDocument()
      .GetLayoutView()
      ->Layer()
      ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .ClearClipRectsIncludingDescendants();

  {
    ClipRect clip_rect;
    target_layer
        ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
        .CalculateBackgroundClipRect(
            ClipRectsContext(GetDocument().GetLayoutView()->Layer(),
                             &GetDocument().GetLayoutView()->FirstFragment(),
                             kUncachedClipRects,
                             kIgnorePlatformOverlayScrollbarSize,
                             kIgnoreOverflowClipAndScroll),
            clip_rect);
    EXPECT_EQ(PhysicalRect(100, 208, 200, 300), clip_rect.Rect());
  }

  // At 50, target is still scrolling - clip_rect shouldn't change.
  GetDocument().domWindow()->scrollTo(0, 50);
  {
    ClipRect clip_rect;
    target_layer
        ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
        .CalculateBackgroundClipRect(
            ClipRectsContext(GetDocument().GetLayoutView()->Layer(),
                             &GetDocument().GetLayoutView()->FirstFragment(),
                             kUncachedClipRects,
                             kIgnorePlatformOverlayScrollbarSize,
                             kIgnoreOverflowClipAndScroll),
            clip_rect);
    EXPECT_EQ(PhysicalRect(100, 208, 200, 300), clip_rect.Rect());
  }

  // At 150, target is fixed - clip_rect should now increase.
  GetDocument().domWindow()->scrollTo(0, 150);
  {
    ClipRect clip_rect;
    target_layer
        ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
        .CalculateBackgroundClipRect(
            ClipRectsContext(GetDocument().GetLayoutView()->Layer(),
                             &GetDocument().GetLayoutView()->FirstFragment(),
                             kUncachedClipRects,
                             kIgnorePlatformOverlayScrollbarSize,
                             kIgnoreOverflowClipAndScroll),
            clip_rect);
    EXPECT_EQ(PhysicalRect(100, 250, 200, 300), clip_rect.Rect());
  }

  // At 250, target is still fixed - clip_rect should keep increasing.
  GetDocument().domWindow()->scrollTo(0, 250);
  {
    ClipRect clip_rect;
    target_layer
        ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
        .CalculateBackgroundClipRect(
            ClipRectsContext(GetDocument().GetLayoutView()->Layer(),
                             &GetDocument().GetLayoutView()->FirstFragment(),
                             kUncachedClipRects,
                             kIgnorePlatformOverlayScrollbarSize,
                             kIgnoreOverflowClipAndScroll),
            clip_rect);
    EXPECT_EQ(PhysicalRect(100, 350, 200, 300), clip_rect.Rect());
  }
}

}  // namespace blink
