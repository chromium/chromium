// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/html_canvas_painter.h"

#include <memory>
#include <utility>

#include "cc/layers/layer.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/stub_chrome_client_for_spv2.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"

// Integration tests of canvas painting code (in SPv2 mode).

namespace blink {

class HTMLCanvasPainterTestForSPv2 : public PaintControllerPaintTest {
 public:
  HTMLCanvasPainterTestForSPv2()
      : chrome_client_(new StubChromeClientForSPv2) {}

 protected:
  void SetUp() override {
    auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      gl->SetIsContextLost(false);
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
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

  ChromeClient& GetChromeClient() const override { return *chrome_client_; }

  bool HasLayerAttached(const cc::Layer& layer) {
    return chrome_client_->HasLayer(layer);
  }

  std::unique_ptr<Canvas2DLayerBridge> MakeCanvas2DLayerBridge(
      const IntSize& size) {
    return std::make_unique<Canvas2DLayerBridge>(
        size, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams());
  }

 private:
  Persistent<StubChromeClientForSPv2> chrome_client_;
  FakeGLES2Interface gl_;
};

INSTANTIATE_SPV2_TEST_CASE_P(HTMLCanvasPainterTestForSPv2);

TEST_P(HTMLCanvasPainterTestForSPv2, Canvas2DLayerAppearsInLayerTree) {
  // Insert a <canvas> and force it into accelerated mode.
  // Not using SetBodyInnerHTML() because we need to test before document
  // lifecyle update.
  GetDocument().body()->SetInnerHTMLFromString("<canvas width=300 height=200>");
  HTMLCanvasElement* element =
      ToHTMLCanvasElement(GetDocument().body()->firstChild());
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
  element->FinalizeFrame();
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Fetch the layer associated with the <canvas>, and check that it was
  // correctly configured in the layer tree.
  const cc::Layer* layer = context->CcLayer();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(HasLayerAttached(*layer));
  EXPECT_EQ(gfx::Size(300, 200), layer->bounds());
}

}  // namespace blink
