// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/htmlcanvas/html_canvas_element_module.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_embedded_frame_sink_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

using ::testing::_;
using ::testing::Values;

namespace blink {

namespace {

// This class allows for overriding GenerateFrameSinkId() so that the
// HTMLCanvasElement's SurfaceLayerBridge will get a syntactically correct
// FrameSinkId.  It also returns a valid GpuMemoryBufferManager so that low
// latency mode is enabled.
class LowLatencyTestPlatform : public GpuMemoryBufferTestPlatform {
 public:
  viz::FrameSinkId GenerateFrameSinkId() override {
    // Doesn't matter what we return as long as is not zero.
    constexpr uint32_t kClientId = 2;
    constexpr uint32_t kSinkId = 1;
    return viz::FrameSinkId(kClientId, kSinkId);
  }
};

}  // unnamed namespace

class HTMLCanvasElementModuleTest : public ::testing::Test,
                                    public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    web_view_helper_.Initialize();
    GetDocument().documentElement()->setInnerHTML(
        String::FromUTF8("<body><canvas id='c'></canvas></body>"));
    canvas_element_ =
        To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
  }

  LocalDOMWindow* GetWindow() const {
    return web_view_helper_.GetWebView()
        ->MainFrameImpl()
        ->GetFrame()
        ->DomWindow();
  }

  Document& GetDocument() const { return *GetWindow()->document(); }

  HTMLCanvasElement& canvas_element() const { return *canvas_element_; }
  OffscreenCanvas* TransferControlToOffscreen(ExceptionState& exception_state) {
    return HTMLCanvasElementModule::TransferControlToOffscreenInternal(
        ToScriptStateForMainWorld(GetWindow()->GetFrame()), canvas_element(),
        exception_state);
  }

  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  Persistent<HTMLCanvasElement> canvas_element_;
  Persistent<CanvasRenderingContext> context_;
};

// Tests if the Canvas Id is associated correctly.
TEST_F(HTMLCanvasElementModuleTest, TransferControlToOffscreen) {
  NonThrowableExceptionState exception_state;
  const OffscreenCanvas* offscreen_canvas =
      TransferControlToOffscreen(exception_state);
  const DOMNodeId canvas_id = offscreen_canvas->PlaceholderCanvasId();
  EXPECT_EQ(canvas_id, canvas_element().GetDomNodeId());
}

// Verifies that a desynchronized canvas has the appropriate opacity/blending
// information sent to the CompositorFrameSink.
TEST_P(HTMLCanvasElementModuleTest, LowLatencyCanvasCompositorFrameOpacity) {
  // TODO(crbug.com/922218): enable desynchronized on Mac.
#if !BUILDFLAG(IS_MAC)
  // This test relies on GpuMemoryBuffers being supported and enabled for low
  // latency canvas.  The latter is true only on ChromeOS in production.
  ScopedTestingPlatformSupport<LowLatencyTestPlatform> platform;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kLowLatencyCanvas2dImageChromium);

  auto context_provider = viz::TestContextProvider::Create();
#if SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
  constexpr auto buffer_format = gfx::BufferFormat::BGRA_8888;
#elif SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
  constexpr auto buffer_format = gfx::BufferFormat::RGBA_8888;
#endif

  context_provider->UnboundTestContextGL()
      ->set_supports_gpu_memory_buffer_format(buffer_format, true);
  InitializeSharedGpuContextGLES2(context_provider.get());

  // To intercept SubmitCompositorFrame/SubmitCompositorFrameSync messages sent
  // by a canvas's CanvasResourceDispatcher, we have to override the Mojo
  // EmbeddedFrameSinkProvider interface impl and its CompositorFrameSinkClient.
  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Receiver<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_receiver(&mock_embedded_frame_sink_provider);
  auto override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_receiver);

  const bool context_alpha = GetParam();
  CanvasContextCreationAttributesCore attrs;
  attrs.alpha = context_alpha;
  attrs.desynchronized = true;
  EXPECT_CALL(mock_embedded_frame_sink_provider, CreateCompositorFrameSink_(_));
  context_ = canvas_element().GetCanvasRenderingContext(String("2d"), attrs);
  EXPECT_EQ(context_->CreationAttributes().alpha, attrs.alpha);
  EXPECT_TRUE(context_->CreationAttributes().desynchronized);
  EXPECT_TRUE(canvas_element().LowLatencyEnabled());
  EXPECT_TRUE(canvas_element().SurfaceLayerBridge());
  platform->RunUntilIdle();

  // This call simulates having drawn something before FinalizeFrame().
  canvas_element().DidDraw();

  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SubmitCompositorFrame_(_))
      .WillOnce(::testing::WithArg<0>(
          ::testing::Invoke([context_alpha](const viz::CompositorFrame* frame) {
            ASSERT_EQ(frame->render_pass_list.size(), 1u);

            const auto& quad_list = frame->render_pass_list[0]->quad_list;
            ASSERT_EQ(quad_list.size(), 1u);
            EXPECT_EQ(quad_list.front()->needs_blending, context_alpha);

            const auto& shared_quad_state_list =
                frame->render_pass_list[0]->shared_quad_state_list;
            ASSERT_EQ(shared_quad_state_list.size(), 1u);
            EXPECT_NE(shared_quad_state_list.front()->are_contents_opaque,
                      context_alpha);
          })));
  canvas_element().PreFinalizeFrame();
  context_->FinalizeFrame(FlushReason::kTesting);
  canvas_element().PostFinalizeFrame(FlushReason::kTesting);
  platform->RunUntilIdle();

  SharedGpuContext::Reset();
#endif
}

INSTANTIATE_TEST_SUITE_P(All, HTMLCanvasElementModuleTest, Values(true, false));
}  // namespace blink
