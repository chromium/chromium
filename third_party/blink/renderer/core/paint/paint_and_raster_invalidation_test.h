// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AND_RASTER_INVALIDATION_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AND_RASTER_INVALIDATION_TEST_H_

#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"

namespace blink {

class PaintAndRasterInvalidationTest : public PaintControllerPaintTest {
 public:
  PaintAndRasterInvalidationTest()
      : PaintControllerPaintTest(SingleChildLocalFrameClient::Create()) {}

 protected:
  cc::Layer* GetCcLayer(size_t index = 0) const {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      return GetDocument()
          .View()
          ->GetPaintArtifactCompositorForTesting()
          ->RootLayer()
          ->children()[index]
          .get();
    }
    return GetLayoutView().Layer()->GraphicsLayerBacking()->ContentLayer();
  }

  cc::LayerClient* GetCcLayerClient(size_t index = 0) const {
    return GetCcLayer(index)->GetLayerClientForTesting();
  }

  const RasterInvalidationTracking* GetRasterInvalidationTracking(
      size_t index = 0) const {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      return static_cast<ContentLayerClientImpl*>(GetCcLayerClient(index))
          ->GetRasterInvalidator()
          .GetTracking();
    }
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking()
        ->GetRasterInvalidationTracking();
  }

  void SetUp() override {
    PaintControllerPaintTest::SetUp();

    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      layer_tree_ = std::make_unique<LayerTreeHostEmbedder>();
      layer_tree_->layer_tree_host()->SetRootLayer(
          GetDocument()
              .View()
              ->GetPaintArtifactCompositorForTesting()
              ->RootLayer());
    }
  }

  void SetPreferCompositingToLCDText(bool enable) {
    GetDocument()
        .GetFrame()
        ->GetSettings()
        ->SetPreferCompositingToLCDTextEnabled(enable);
  }

 private:
  std::unique_ptr<LayerTreeHostEmbedder> layer_tree_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AND_RASTER_INVALIDATION_TEST_H_
