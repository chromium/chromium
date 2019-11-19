// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/html_canvas_painter.h"

#include <memory>
#include <utility>

#include "cc/layers/layer.h"
#include "components/viz/test/test_context_provider.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"

// Integration tests of canvas painting code (in CAP mode).

namespace blink {

class HTMLCanvasPainterTestForCAP : public PaintControllerPaintTest {
 public:
  HTMLCanvasPainterTestForCAP() {}

 protected:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::Create();
    InitializeSharedGpuContext(test_context_provider_.get());
    PaintControllerPaintTest::SetUp();
  }

  void TearDown() override {
    SharedGpuContext::ResetForTesting();
    PaintControllerPaintTest::TearDown();
  }

  FrameSettingOverrideFunction SettingOverrider() const override {
    return [](Settings& settings) {
      // LayoutHTMLCanvas doesn't exist if script is disabled.
      settings.SetScriptEnabled(true);
    };
  }

  bool HasLayerAttached(const cc::Layer& layer) {
    return GetChromeClient().HasLayer(layer);
  }

  std::unique_ptr<Canvas2DLayerBridge> MakeCanvas2DLayerBridge(
      const IntSize& size) {
    return std::make_unique<Canvas2DLayerBridge>(
        size, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams());
  }

 private:
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
};

INSTANTIATE_CAP_TEST_SUITE_P(HTMLCanvasPainterTestForCAP);

TEST_P(HTMLCanvasPainterTestForCAP, Canvas2DLayerAppearsInLayerTree) {
  // Insert a <canvas> and force it into accelerated mode.
  // Not using SetBodyInnerHTML() because we need to test before document
  // lifecyle update.
  GetDocument().body()->SetInnerHTMLFromString("<canvas width=300 height=200>");
  auto* element = To<HTMLCanvasElement>(GetDocument().body()->firstChild());
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = true;
  CanvasRenderingContext* context =
      element->GetCanvasRenderingContext("2d", attributes);
  IntSize size(300, 200);
  std::unique_ptr<Canvas2DLayerBridge> bridge = MakeCanvas2DLayerBridge(size);
  element->SetResourceProviderForTesting(nullptr, std::move(bridge), size);
  ASSERT_EQ(context, element->RenderingContext());
  ASSERT_TRUE(context->IsComposited());
  ASSERT_TRUE(element->IsAccelerated());

  // Force the page to paint.
  element->PreFinalizeFrame();
  context->FinalizeFrame();
  element->PostFinalizeFrame();
  UpdateAllLifecyclePhasesForTest();

  // Fetch the layer associated with the <canvas>, and check that it was
  // correctly configured in the layer tree.
  const cc::Layer* layer = context->CcLayer();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(HasLayerAttached(*layer));
  EXPECT_EQ(gfx::Size(300, 200), layer->bounds());
}

}  // namespace blink
