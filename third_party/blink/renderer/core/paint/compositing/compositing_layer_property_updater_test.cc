// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_layer_property_updater.h"

#include "cc/layers/picture_layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

namespace blink {

class CompositingLayerPropertyUpdaterTest : public RenderingTest {
 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

TEST_F(CompositingLayerPropertyUpdaterTest, MaskLayerState) {
  SetBodyInnerHTML(R"HTML(
    <svg width="0" height="0">
      <defs>
        <clipPath id="text">
          <text>Text</text>
        </clipPath>
      </defs>
    </svg>
    <style>
      .clip-path-mask { clip-path: url(#text); }
      .clip-path-path { clip-path: circle(50%); }
      .mask-image { -webkit-mask-image: linear-gradient(black, transparent); }
    </style>
    <div id=target class="clip-path-mask"
         style="position: absolute; will-change: transform;
                clip: rect(9px, -1px, -1px, 96px)">
    </div>
    )HTML");

  auto* target_element = GetDocument().getElementById("target");
  auto* target = target_element->GetLayoutBoxModelObject();
  EXPECT_EQ(kPaintsIntoOwnBacking, target->GetCompositingState());
  auto* mapping = target->Layer()->GetCompositedLayerMapping();
  auto* mask_layer = mapping->MaskLayer();

  auto* paint_properties = target->FirstFragment().PaintProperties();
  EXPECT_TRUE(paint_properties->CssClip());
  EXPECT_TRUE(paint_properties->MaskClip());
  EXPECT_EQ(paint_properties->MaskClip(),
            &mask_layer->GetPropertyTreeState().Clip());
  EXPECT_FALSE(paint_properties->Mask());
  EXPECT_EQ(paint_properties->ClipPathMask(),
            &mask_layer->GetPropertyTreeState().Effect());

  target_element->setAttribute(html_names::kClassAttr,
                               "clip-path-mask mask-image");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(mapping, target->Layer()->GetCompositedLayerMapping());
  ASSERT_EQ(mask_layer, mapping->MaskLayer());
  ASSERT_EQ(paint_properties, target->FirstFragment().PaintProperties());
  EXPECT_FALSE(paint_properties->ClipPathClip());
  EXPECT_EQ(paint_properties->MaskClip(),
            &mask_layer->GetPropertyTreeState().Clip());
  EXPECT_TRUE(paint_properties->ClipPathMask());
  EXPECT_EQ(paint_properties->Mask(),
            &mask_layer->GetPropertyTreeState().Effect());

  target_element->setAttribute(html_names::kClassAttr, "mask-image");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(mapping, target->Layer()->GetCompositedLayerMapping());
  ASSERT_EQ(mask_layer, mapping->MaskLayer());
  ASSERT_EQ(paint_properties, target->FirstFragment().PaintProperties());
  EXPECT_EQ(paint_properties->MaskClip(),
            &mask_layer->GetPropertyTreeState().Clip());
  EXPECT_FALSE(paint_properties->ClipPathMask());
  EXPECT_EQ(paint_properties->Mask(),
            &mask_layer->GetPropertyTreeState().Effect());

  target_element->setAttribute(html_names::kClassAttr,
                               "clip-path-path mask-image");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(mapping, target->Layer()->GetCompositedLayerMapping());
  ASSERT_EQ(mask_layer, mapping->MaskLayer());
  ASSERT_EQ(paint_properties, target->FirstFragment().PaintProperties());
  EXPECT_TRUE(paint_properties->ClipPathClip());
  EXPECT_EQ(paint_properties->MaskClip(),
            &mask_layer->GetPropertyTreeState().Clip());
  EXPECT_FALSE(paint_properties->ClipPathMask());
  EXPECT_EQ(paint_properties->Mask(),
            &mask_layer->GetPropertyTreeState().Effect());

  target_element->setAttribute(html_names::kClassAttr, "clip-path-path");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(mapping, target->Layer()->GetCompositedLayerMapping());
  EXPECT_FALSE(mapping->MaskLayer());
  ASSERT_EQ(paint_properties, target->FirstFragment().PaintProperties());
  EXPECT_TRUE(paint_properties->ClipPathClip());
  EXPECT_FALSE(paint_properties->ClipPathMask());
  EXPECT_FALSE(paint_properties->Mask());
}

TEST_F(CompositingLayerPropertyUpdaterTest,
       EnsureOverlayScrollbarLayerHasEffectNode) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  ASSERT_TRUE(GetDocument()
                  .GetFrame()
                  ->GetPage()
                  ->GetScrollbarTheme()
                  .UsesOverlayScrollbars());
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        width: 100px;
        height: 100px;
        overflow: scroll;
      }
      #big {
        width: 200px;
        height: 200px;
      }
    </style>
    <div id='scroller'>
      <div id='big'></div>
    </div>
  )HTML");

  PaintLayer* scroller_layer = GetPaintLayerByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area =
      scroller_layer->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);

  auto* horizontal_scrollbar_layer =
      scrollable_area->GraphicsLayerForHorizontalScrollbar();
  auto* vertical_scrollbar_layer =
      scrollable_area->GraphicsLayerForVerticalScrollbar();
  ASSERT_TRUE(horizontal_scrollbar_layer);
  ASSERT_TRUE(vertical_scrollbar_layer);

  auto* paint_properties =
      scroller_layer->GetLayoutObject().FirstFragment().PaintProperties();

  // Ensure each overlay scrollbar has its own effect node and effect node has
  // correct element_id.
  EXPECT_EQ(paint_properties->HorizontalScrollbarEffect(),
            &horizontal_scrollbar_layer->GetPropertyTreeState().Effect());
  EXPECT_EQ(paint_properties->VerticalScrollbarEffect(),
            &vertical_scrollbar_layer->GetPropertyTreeState().Effect());
  EXPECT_NE(&horizontal_scrollbar_layer->GetPropertyTreeState().Effect(),
            &vertical_scrollbar_layer->GetPropertyTreeState().Effect());
  EXPECT_NE(
      ToUnaliased(horizontal_scrollbar_layer->GetPropertyTreeState().Effect())
          .GetCompositorElementId(),
      ToUnaliased(vertical_scrollbar_layer->GetPropertyTreeState().Effect())
          .GetCompositorElementId());
  EXPECT_EQ(
      ToUnaliased(horizontal_scrollbar_layer->GetPropertyTreeState().Effect())
          .GetCompositorElementId(),
      horizontal_scrollbar_layer->ContentsLayer()->element_id());
  EXPECT_EQ(
      ToUnaliased(vertical_scrollbar_layer->GetPropertyTreeState().Effect())
          .GetCompositorElementId(),
      vertical_scrollbar_layer->ContentsLayer()->element_id());
}

TEST_F(CompositingLayerPropertyUpdaterTest,
       RootScrollbarShouldUseParentOfOverscrollNodeAsTransformNode) {
  auto& document = GetDocument();
  document.GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  document.SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>
    ::-webkit-scrollbar {
      width: 12px;
      background: darkblue;
    }
    ::-webkit-scrollbar-thumb {
      background: white;
    }
    #scroller {
      height: 100px;
      overflow-y: scroll;
    }
    .big {
      height: 1000px;
    }
    </style>

    <div class='big'></div>
    <div id='scroller'>
      <div class='big'></div>
    </div>
  )HTML");

  {
    const auto* root_scrollable = document.View()->LayoutViewport();
    const auto& visual_viewport =
        document.View()->GetPage()->GetVisualViewport();

    auto* vertical_scrollbar_layer =
        root_scrollable->GraphicsLayerForVerticalScrollbar();
    ASSERT_TRUE(vertical_scrollbar_layer);
    EXPECT_EQ(&vertical_scrollbar_layer->GetPropertyTreeState().Transform(),
              visual_viewport.GetOverscrollElasticityTransformNode()->Parent());
  }

  // Non root scrollbar should use scroller's transform node.
  {
    PaintLayer* scroller_layer = GetPaintLayerByElementId("scroller");
    PaintLayerScrollableArea* scrollable_area =
        scroller_layer->GetScrollableArea();
    ASSERT_TRUE(scrollable_area);

    auto* vertical_scrollbar_layer =
        scrollable_area->GraphicsLayerForVerticalScrollbar();
    ASSERT_TRUE(vertical_scrollbar_layer);

    auto paint_properties = scroller_layer->GetLayoutObject()
                                .FirstFragment()
                                .LocalBorderBoxProperties();

    EXPECT_EQ(&vertical_scrollbar_layer->GetPropertyTreeState().Transform(),
              &paint_properties.Transform());
  }
}

TEST_F(CompositingLayerPropertyUpdaterTest, OverflowControlsClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { width: 20px; }
      #container {
        width: 5px;
        height: 100px;
      }
      #target {
        overflow: scroll;
        will-change: transform;
        width: 100%;
        height: 100%;
      }
    </style>
    <div id="container">
      <div id="target"></div>
    </div>
  )HTML");

  // Initially the vertical scrollbar overflows the narrow border box.
  auto* container = GetDocument().getElementById("container");
  auto* target = GetLayoutBoxByElementId("target");
  auto* scrollbar_layer =
      target->GetScrollableArea()->GraphicsLayerForVerticalScrollbar();
  auto target_state = target->FirstFragment().LocalBorderBoxProperties();
  auto scrollbar_state = target_state;
  auto* overflow_controls_clip =
      target->FirstFragment().PaintProperties()->OverflowControlsClip();
  ASSERT_TRUE(overflow_controls_clip);
  scrollbar_state.SetClip(*overflow_controls_clip);
  EXPECT_EQ(scrollbar_state, scrollbar_layer->GetPropertyTreeState());

  // Widen target to make the vertical scrollbar contained by the border box.
  container->setAttribute(html_names::kStyleAttr, "width: 100px");
  UpdateAllLifecyclePhasesForTest();
  LOG(ERROR) << target->Size();
  EXPECT_FALSE(
      target->FirstFragment().PaintProperties()->OverflowControlsClip());
  EXPECT_EQ(target_state, scrollbar_layer->GetPropertyTreeState());

  // Narrow down target back.
  container->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  scrollbar_state = target_state;
  overflow_controls_clip =
      target->FirstFragment().PaintProperties()->OverflowControlsClip();
  ASSERT_TRUE(overflow_controls_clip);
  scrollbar_state.SetClip(*overflow_controls_clip);
  EXPECT_EQ(scrollbar_state, scrollbar_layer->GetPropertyTreeState());
}

}  // namespace blink
