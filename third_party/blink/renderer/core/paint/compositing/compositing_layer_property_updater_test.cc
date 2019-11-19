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
    <div id=target style="position: absolute;
        clip-path: polygon(-1px -1px, 86px 400px);
        clip: rect(9px, -1px, -1px, 96px); will-change: transform">
    </div>
    )HTML");

  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, target->GetCompositingState());
  auto* mapping = target->GetCompositedLayerMapping();
  auto* mask_layer = mapping->MaskLayer();

  auto* paint_properties =
      target->GetLayoutObject().FirstFragment().PaintProperties();
  EXPECT_TRUE(paint_properties->CssClip());
  EXPECT_TRUE(paint_properties->MaskClip());
  EXPECT_EQ(paint_properties->MaskClip(),
            &mask_layer->layer_state_->state.Clip());
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

  PaintLayer* scroller_layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"))->Layer();
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
  EXPECT_NE(horizontal_scrollbar_layer->GetPropertyTreeState()
                .Effect()
                .GetCompositorElementId(),
            vertical_scrollbar_layer->GetPropertyTreeState()
                .Effect()
                .GetCompositorElementId());
  EXPECT_EQ(horizontal_scrollbar_layer->GetPropertyTreeState()
                .Effect()
                .GetCompositorElementId(),
            horizontal_scrollbar_layer->ContentsLayer()->element_id());
  EXPECT_EQ(vertical_scrollbar_layer->GetPropertyTreeState()
                .Effect()
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
    PaintLayer* scroller_layer =
        ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"))->Layer();
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

}  // namespace blink
