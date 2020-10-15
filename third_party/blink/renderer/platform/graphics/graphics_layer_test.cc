/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"

#include <memory>
#include <utility>

#include "cc/layers/picture_layer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

using ::testing::ElementsAre;

namespace blink {

class GraphicsLayerTest : public PaintControllerTestBase {
 protected:
  const RasterInvalidator* GetInternalRasterInvalidator(
      const GraphicsLayer& layer) {
    return layer.raster_invalidator_.get();
  }

  RasterInvalidator& EnsureRasterInvalidator(GraphicsLayer& layer) {
    return layer.EnsureRasterInvalidator();
  }

  const PaintController* GetInternalPaintController(
      const GraphicsLayer& layer) {
    return layer.paint_controller_.get();
  }
};

TEST_F(GraphicsLayerTest, PaintRecursively) {
  FakeGraphicsLayerClient client;
  GraphicsLayer root(client);
  root.SetPaintsHitTest(true);
  root.SetLayerState(PropertyTreeState::Root(), IntPoint());

  // Initially layer1 doesn't draw content.
  GraphicsLayer layer1(client);
  EXPECT_FALSE(layer1.DrawsContent());
  auto t1 = Create2DTranslation(t0(), 10, 20);
  PropertyTreeState layer1_state(*t1, c0(), e0());
  layer1.SetLayerState(layer1_state, IntPoint());
  root.AddChild(&layer1);

  GraphicsLayer layer2(client);
  layer2.SetDrawsContent(true);
  auto t2 = Create2DTranslation(t0(), 10, 20);
  PropertyTreeState layer2_state(*t2, c0(), e0());
  layer2.SetLayerState(layer2_state, IntPoint());
  root.AddChild(&layer2);

  client.SetPainter([&](const GraphicsLayer* layer, GraphicsContext& context,
                        GraphicsLayerPaintingPhase, const IntRect&) {
    if (layer == &root) {
      context.GetPaintController().RecordHitTestData(
          *layer, IntRect(1, 2, 3, 4), TouchAction::kNone);
    } else if (layer == &layer1) {
      ScopedPaintChunkProperties properties(
          context.GetPaintController(), layer1_state, *layer, kBackgroundType);
      PaintControllerTestBase::DrawRect(context, *layer, kBackgroundType,
                                        IntRect(2, 3, 4, 5));
    } else if (layer == &layer2) {
      ScopedPaintChunkProperties properties(
          context.GetPaintController(), layer2_state, *layer, kBackgroundType);
      PaintControllerTestBase::DrawRect(context, *layer, kBackgroundType,
                                        IntRect(3, 4, 5, 6));
    }
  });

  GraphicsContext context(GetPaintController());
  client.SetNeedsRepaint(true);
  Vector<PreCompositedLayerInfo> pre_composited_layers;
  EXPECT_TRUE(root.PaintRecursively(context, pre_composited_layers));
  EXPECT_TRUE(root.Repainted());
  root.GetPaintController().FinishCycle();
  EXPECT_FALSE(layer1.Repainted());
  EXPECT_TRUE(layer2.Repainted());
  layer2.GetPaintController().FinishCycle();

  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {{IntRect(1, 2, 3, 4)}};
  ASSERT_EQ(2u, pre_composited_layers.size());
  EXPECT_EQ(&root, pre_composited_layers[0].graphics_layer);
  EXPECT_THAT(
      pre_composited_layers[0].chunks,
      ElementsAre(IsPaintChunk(
          0, 0, PaintChunk::Id(root, DisplayItem::kHitTest),
          PropertyTreeState::Root(), &hit_test_data, IntRect(1, 2, 3, 4))));
  EXPECT_THAT(pre_composited_layers[0].chunks.begin().DisplayItems(),
              ElementsAre());
  EXPECT_EQ(&layer2, pre_composited_layers[1].graphics_layer);
  EXPECT_THAT(
      pre_composited_layers[1].chunks,
      ElementsAre(IsPaintChunk(0, 1, PaintChunk::Id(layer2, kBackgroundType),
                               layer2_state, nullptr, IntRect(3, 4, 5, 6))));
  EXPECT_THAT(pre_composited_layers[1].chunks.begin().DisplayItems(),
              ElementsAre(IsSameId(&layer2, kBackgroundType)));

  // Paint again with nothing changed.
  client.SetNeedsRepaint(false);
  pre_composited_layers.clear();
  EXPECT_FALSE(root.PaintRecursively(context, pre_composited_layers));
  EXPECT_FALSE(root.Repainted());
  EXPECT_FALSE(layer1.Repainted());
  EXPECT_FALSE(layer2.Repainted());
  EXPECT_EQ(2u, pre_composited_layers.size());

  // Paint again with layer1 drawing content.
  layer1.SetDrawsContent(true);
  pre_composited_layers.clear();
  EXPECT_TRUE(root.PaintRecursively(context, pre_composited_layers));
  EXPECT_FALSE(root.Repainted());
  EXPECT_TRUE(layer1.Repainted());
  layer1.GetPaintController().FinishCycle();
  EXPECT_FALSE(layer2.Repainted());

  EXPECT_EQ(3u, pre_composited_layers.size());
  EXPECT_EQ(&root, pre_composited_layers[0].graphics_layer);
  EXPECT_THAT(
      pre_composited_layers[0].chunks,
      ElementsAre(IsPaintChunk(
          0, 0, PaintChunk::Id(root, DisplayItem::kHitTest),
          PropertyTreeState::Root(), &hit_test_data, IntRect(1, 2, 3, 4))));
  EXPECT_THAT(pre_composited_layers[0].chunks.begin().DisplayItems(),
              ElementsAre());
  EXPECT_EQ(&layer1, pre_composited_layers[1].graphics_layer);
  EXPECT_THAT(
      pre_composited_layers[1].chunks,
      ElementsAre(IsPaintChunk(0, 1, PaintChunk::Id(layer1, kBackgroundType),
                               layer1_state, nullptr, IntRect(2, 3, 4, 5))));
  EXPECT_THAT(pre_composited_layers[1].chunks.begin().DisplayItems(),
              ElementsAre(IsSameId(&layer1, kBackgroundType)));
  EXPECT_EQ(&layer2, pre_composited_layers[2].graphics_layer);
  EXPECT_THAT(
      pre_composited_layers[2].chunks,
      ElementsAre(IsPaintChunk(0, 1, PaintChunk::Id(layer2, kBackgroundType),
                               layer2_state, nullptr, IntRect(3, 4, 5, 6))));
  EXPECT_THAT(pre_composited_layers[2].chunks.begin().DisplayItems(),
              ElementsAre(IsSameId(&layer2, kBackgroundType)));
}

TEST_F(GraphicsLayerTest, SetDrawsContentFalse) {
  FakeGraphicsLayerClient client;
  GraphicsLayer layer(client);
  layer.SetDrawsContent(true);

  layer.GetPaintController();
  EXPECT_NE(nullptr, GetInternalPaintController(layer));
  EnsureRasterInvalidator(layer);
  EXPECT_NE(nullptr, GetInternalRasterInvalidator(layer));

  layer.SetDrawsContent(false);
  EXPECT_EQ(nullptr, GetInternalPaintController(layer));
  EXPECT_EQ(nullptr, GetInternalRasterInvalidator(layer));
}

TEST_F(GraphicsLayerTest, ContentsLayer) {
  FakeGraphicsLayerClient client;
  GraphicsLayer graphics_layer(client);
  auto contents_layer = cc::Layer::Create();
  graphics_layer.SetContentsToCcLayer(contents_layer, true);
  EXPECT_TRUE(graphics_layer.HasContentsLayer());
  EXPECT_EQ(contents_layer.get(), graphics_layer.ContentsLayer());
  graphics_layer.SetContentsToCcLayer(nullptr, true);
  EXPECT_FALSE(graphics_layer.HasContentsLayer());
  EXPECT_EQ(nullptr, graphics_layer.ContentsLayer());
}

}  // namespace blink
