// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AND_RASTER_INVALIDATION_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AND_RASTER_INVALIDATION_TEST_H_

#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"

namespace blink {

const RasterInvalidationTracking* GetRasterInvalidationTracking(
    const LocalFrameView& root_frame_view,
    wtf_size_t index = 0);

class PaintAndRasterInvalidationTest : public PaintControllerPaintTest {
 public:
  PaintAndRasterInvalidationTest()
      : PaintControllerPaintTest(
            MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  const RasterInvalidationTracking* GetRasterInvalidationTracking(
      wtf_size_t index = 0) const {
    return blink::GetRasterInvalidationTracking(*GetDocument().View(), index);
  }

  void SetUp() override {
    PaintControllerPaintTest::SetUp();

    layer_tree_ = std::make_unique<LayerTreeHostEmbedder>();
    layer_tree_->layer_tree_host()->SetRootLayer(
        GetDocument().View()->GetPaintArtifactCompositor()->RootLayer());
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
