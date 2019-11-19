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
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/viewport_layers_setup.h"
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/rotate_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"

namespace blink {

class GraphicsLayerTest : public testing::Test, public PaintTestConfigurations {
 public:
  GraphicsLayerTest() = default;
  ~GraphicsLayerTest() = default;

 protected:
  void CommitAndFinishCycle(GraphicsLayer& layer) {
    layer.GetPaintController().CommitNewDisplayItems();
    layer.GetPaintController().FinishCycle();
  }

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

  ViewportLayersSetup layers_;
};

INSTANTIATE_TEST_SUITE_P(All, GraphicsLayerTest, testing::Values(0));

TEST_P(GraphicsLayerTest, Paint) {
  IntRect interest_rect(1, 2, 3, 4);
  auto& layer = layers_.graphics_layer();
  EXPECT_TRUE(layer.PaintWithoutCommitForTesting(interest_rect));
  CommitAndFinishCycle(layer);

  layers_.graphics_layer_client().SetNeedsRepaint(true);
  EXPECT_TRUE(layer.PaintWithoutCommitForTesting(interest_rect));
  CommitAndFinishCycle(layer);

  layers_.graphics_layer_client().SetNeedsRepaint(false);
  EXPECT_FALSE(layer.PaintWithoutCommitForTesting(interest_rect));

  interest_rect.Move(IntSize(10, 20));
  EXPECT_TRUE(layer.PaintWithoutCommitForTesting(interest_rect));
  CommitAndFinishCycle(layer);
  EXPECT_FALSE(layer.PaintWithoutCommitForTesting(interest_rect));

  layers_.graphics_layer().SetNeedsDisplay();
  EXPECT_TRUE(layer.PaintWithoutCommitForTesting(interest_rect));
  CommitAndFinishCycle(layer);
  EXPECT_FALSE(layer.PaintWithoutCommitForTesting(interest_rect));
}

TEST_P(GraphicsLayerTest, PaintRecursively) {
  IntRect interest_rect(1, 2, 3, 4);
  const auto& transform_root = TransformPaintPropertyNode::Root();
  auto transform1 =
      CreateTransform(transform_root, TransformationMatrix().Translate(10, 20));
  auto transform2 =
      CreateTransform(*transform1, TransformationMatrix().Scale(2));

  layers_.graphics_layer_client().SetPainter(
      [&](const GraphicsLayer* layer, GraphicsContext& context,
          GraphicsLayerPaintingPhase, const IntRect&) {
        {
          ScopedPaintChunkProperties properties(context.GetPaintController(),
                                                *transform1, *layer,
                                                kBackgroundType);
          PaintControllerTestBase::DrawRect(context, *layer, kBackgroundType,
                                            interest_rect);
        }
        {
          ScopedPaintChunkProperties properties(context.GetPaintController(),
                                                *transform2, *layer,
                                                kForegroundType);
          PaintControllerTestBase::DrawRect(context, *layer, kForegroundType,
                                            interest_rect);
        }
      });

  transform1->Update(transform_root,
                     TransformPaintPropertyNode::State{FloatSize(20, 30)});
  EXPECT_TRUE(transform1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                  transform_root));
  EXPECT_TRUE(transform2->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                  transform_root));
  layers_.graphics_layer_client().SetNeedsRepaint(true);
  layers_.graphics_layer().PaintRecursively();
}

TEST_P(GraphicsLayerTest, SetDrawsContentFalse) {
  EXPECT_TRUE(layers_.graphics_layer().DrawsContent());
  layers_.graphics_layer().GetPaintController();
  EXPECT_NE(nullptr, GetInternalPaintController(layers_.graphics_layer()));
  EnsureRasterInvalidator(layers_.graphics_layer());
  EXPECT_NE(nullptr, GetInternalRasterInvalidator(layers_.graphics_layer()));

  layers_.graphics_layer().SetDrawsContent(false);
  EXPECT_EQ(nullptr, GetInternalPaintController(layers_.graphics_layer()));
  EXPECT_EQ(nullptr, GetInternalRasterInvalidator(layers_.graphics_layer()));
}

TEST_P(GraphicsLayerTest, ContentsLayer) {
  auto& graphics_layer = layers_.graphics_layer();
  auto contents_layer = cc::Layer::Create();
  GraphicsLayer::RegisterContentsLayer(contents_layer.get());
  graphics_layer.SetContentsToCcLayer(contents_layer.get(), true);
  EXPECT_TRUE(graphics_layer.HasContentsLayer());
  EXPECT_EQ(contents_layer.get(), graphics_layer.ContentsLayer());
  GraphicsLayer::UnregisterContentsLayer(contents_layer.get());
  EXPECT_FALSE(graphics_layer.HasContentsLayer());
  EXPECT_EQ(nullptr, graphics_layer.ContentsLayer());
}

}  // namespace blink
