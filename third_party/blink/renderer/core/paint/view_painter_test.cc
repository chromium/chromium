// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/view_painter.h"

#include <gtest/gtest.h>
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"

namespace blink {

class ViewPainterTest : public PaintControllerPaintTest {
 protected:
  void RunFixedBackgroundTest(bool prefer_compositing_to_lcd_text);
};

INSTANTIATE_PAINT_TEST_CASE_P(ViewPainterTest);

void ViewPainterTest::RunFixedBackgroundTest(
    bool prefer_compositing_to_lcd_text) {
  // TODO(crbug.com/792577): Cull rect for frame scrolling contents is too
  // small.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  if (prefer_compositing_to_lcd_text) {
    Settings* settings = GetDocument().GetFrame()->GetSettings();
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body {
        margin: 0;
        width: 1200px;
        height: 900px;
        background: radial-gradient(
          circle at 100px 100px, blue, transparent 200px) fixed;
      }
    </style>
  )HTML");

  LocalFrameView* frame_view = GetDocument().View();
  ScrollableArea* layout_viewport = frame_view->LayoutViewport();

  ScrollOffset scroll_offset(200, 150);
  layout_viewport->SetScrollOffset(scroll_offset, kUserScroll);
  frame_view->UpdateAllLifecyclePhases();

  CompositedLayerMapping* clm =
      GetLayoutView().Layer()->GetCompositedLayerMapping();

  // If we prefer compositing to LCD text, the fixed background should go in a
  // different layer from the scrolling content; otherwise, it should go in the
  // same layer (i.e., the scrolling contents layer).
  GraphicsLayer* layer_for_background;
  if (prefer_compositing_to_lcd_text) {
    layer_for_background = clm->MainGraphicsLayer();
  } else {
    layer_for_background = clm->ScrollingContentsLayer();
  }
  const DisplayItemList& display_items =
      layer_for_background->GetPaintController().GetDisplayItemList();
  const DisplayItem& background = display_items[0];
  EXPECT_EQ(background.GetType(), kDocumentBackgroundType);
  const DisplayItemClient* expected_client;
  if (!prefer_compositing_to_lcd_text)
    expected_client = &ViewScrollingBackgroundClient();
  else
    expected_client = &GetLayoutView();
  EXPECT_EQ(&background.Client(), expected_client);

  sk_sp<const PaintRecord> record =
      static_cast<const DrawingDisplayItem&>(background).GetPaintRecord();
  ASSERT_EQ(record->size(), 2u);
  cc::PaintOpBuffer::Iterator it(record.get());
  ASSERT_EQ((*++it)->GetType(), cc::PaintOpType::DrawRect);

  // This is the dest_rect_ calculated by BackgroundImageGeometry.  For a fixed
  // background in scrolling contents layer, its location is the scroll offset.
  SkRect rect = static_cast<const cc::DrawRectOp*>(*it)->rect;
  ASSERT_EQ(prefer_compositing_to_lcd_text ? ScrollOffset() : scroll_offset,
            ScrollOffset(rect.fLeft, rect.fTop));
}

TEST_P(ViewPainterTest, DocumentFixedBackgroundLowDPI) {
  RunFixedBackgroundTest(false);
}

TEST_P(ViewPainterTest, DocumentFixedBackgroundHighDPI) {
  RunFixedBackgroundTest(true);
}

TEST_P(ViewPainterTest, DocumentBackgroundWithScroll) {
  // TODO(crbug.com/792577): Cull rect for frame scrolling contents is too
  // small.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML("<div style='height: 5000px'></div>");

  EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 1,
                      TestDisplayItem(ViewScrollingBackgroundClient(),
                                      kDocumentBackgroundType));

  const auto& chunks = RootPaintController().GetPaintArtifact().PaintChunks();
  EXPECT_EQ(1u, chunks.size());
  const auto& chunk = chunks[0];
  EXPECT_EQ(&ViewScrollingBackgroundClient(), &chunk.id.client);

  const auto& tree_state = chunk.properties;
  EXPECT_EQ(&EffectPaintPropertyNode::Root(), tree_state.Effect());
  const auto* properties = GetLayoutView().FirstFragment().PaintProperties();
  EXPECT_EQ(properties->ScrollTranslation(), tree_state.Transform());
  EXPECT_EQ(properties->OverflowClip(), tree_state.Clip());
}

}  // namespace blink
