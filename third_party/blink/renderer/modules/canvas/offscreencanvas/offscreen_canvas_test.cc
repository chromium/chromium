// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_draw_listener.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/html_canvas_element_module.h"
#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_embedded_frame_sink_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

using ::testing::_;
using ::testing::Combine;
using ::testing::ValuesIn;

namespace blink {

namespace {
constexpr uint32_t kClientId = 2;
constexpr uint32_t kSinkId = 1;

struct TestParams {
  bool alpha;
  bool desynchronized;
};

class AcceleratedCompositingTestPlatform
    : public blink::TestingPlatformSupport {
 public:
  bool IsGpuCompositingDisabled() const override { return false; }
};

class TestCanvasDrawListener : public GarbageCollected<TestCanvasDrawListener>,
                               public CanvasDrawListener {
 public:
  TestCanvasDrawListener() = default;

  NewFrameCallback GetNewFrameCallback() override { return {}; }

  bool CanDiscardAlpha() const override { return false; }

  bool NeedsNewFrame() const override { return true; }

  void RequestFrame() override {}
};

}  // unnamed namespace

class OffscreenCanvasTest : public ::testing::Test,
                            public ::testing::WithParamInterface<TestParams> {
 protected:
  OffscreenCanvasTest();
  void SetUp() override;
  void TearDown() override;

  OffscreenCanvas& offscreen_canvas() const { return *offscreen_canvas_; }
  CanvasResourceDispatcher* Dispatcher() const {
    return offscreen_canvas_->GetOrCreateResourceDispatcher();
  }
  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }

  LocalDOMWindow* GetWindow() const {
    return web_view_helper_->GetWebView()
        ->MainFrameImpl()
        ->GetFrame()
        ->DomWindow();
  }

  Document& GetDocument() const { return *GetWindow()->document(); }

  base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
  shared_image_interface_provider() {
    return test_web_shared_image_interface_provider_->GetWeakPtr();
  }

  HTMLCanvasElement* GetCanvasElement() const { return canvas_element_; }

  FakeGLES2Interface* GetGLInterface() { return &gl_; }

  static uint32_t FrameGenerationOf(const UniqueFontSelector& selector) {
    return selector.frame_generation_;
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
  Persistent<OffscreenCanvas> offscreen_canvas_;
  Persistent<OffscreenCanvasRenderingContext2D> context_;
  Persistent<HTMLCanvasElement> canvas_element_;
  FakeGLES2Interface gl_;
  std::unique_ptr<
      ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>
      accelerated_compositing_scope_;
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider_;
};

OffscreenCanvasTest::OffscreenCanvasTest() = default;

void OffscreenCanvasTest::SetUp() {
  auto factory = [](FakeGLES2Interface* gl)
      -> std::unique_ptr<WebGraphicsContext3DProvider> {
    gl->SetIsContextLost(false);
    return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
  };
  SharedGpuContext::SetContextProviderFactoryForTesting(
      BindRepeating(factory, Unretained(&gl_)));

  web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
  web_view_helper_->Initialize();
  accelerated_compositing_scope_ = std::make_unique<
      ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>();

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      String::FromUTF8("<body><canvas id='c'></canvas></body>"));

  canvas_element_ =
      To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));

  DummyExceptionStateForTesting exception_state;
  offscreen_canvas_ = HTMLCanvasElementModule::transferControlToOffscreen(
      ToScriptStateForMainWorld(GetWindow()->GetFrame()), *canvas_element_,
      exception_state);
  // |offscreen_canvas_| should inherit the FrameSinkId from |canvas_element|s
  // SurfaceLayerBridge, but in tests this id is zero; fill it up by hand.
  offscreen_canvas_->SetFrameSinkId(kClientId, kSinkId);

  CanvasContextCreationAttributesCore attrs;
  if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
    attrs.alpha = GetParam().alpha;
    attrs.desynchronized = GetParam().desynchronized;
  }
  context_ = static_cast<OffscreenCanvasRenderingContext2D*>(
      offscreen_canvas_->GetCanvasRenderingContext(
          GetWindow(), CanvasRenderingContext::CanvasRenderingAPI::k2D, attrs));

  test_web_shared_image_interface_provider_ =
      TestWebGraphicsSharedImageInterfaceProvider::Create();
}

void OffscreenCanvasTest::TearDown() {
  SharedGpuContext::Reset();
  // destruction order matters due to nested TestPlatformSupport instance.
  accelerated_compositing_scope_ = nullptr;
  web_view_helper_ = nullptr;
}

TEST_F(OffscreenCanvasTest, AnimationNotInitiallySuspended) {
  ScriptState::Scope scope(GetScriptState());
  EXPECT_FALSE(Dispatcher()->IsAnimationSuspended());
}

TEST_F(OffscreenCanvasTest, AnimationUsesSyntheticTimerWhenHidden) {
  ScriptState::Scope scope(GetScriptState());
  ScopedAllowSyntheticTimingForCanvasCaptureForTest timer_feature(true);

  // We need a resource dispatcher, else this all gets queued until we get one.
  offscreen_canvas().GetOrCreateResourceDispatcher();

  // Cause the page to be hidden.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden, /*is_initial_state=*/false);
  EXPECT_EQ(GetDocument().GetPage()->GetVisibilityState(),
            mojom::blink::PageVisibilityState::kHidden);

  // Without capture, animation should be suspended.
  EXPECT_EQ(GetCanvasElement()->GetAnimationStateForTesting(),
            CanvasResourceDispatcher::AnimationState::kSuspended);

  // Cause the canvas to believe that it's being captured, and verify that we're
  // now using synthetic timing.
  auto* listener = MakeGarbageCollected<TestCanvasDrawListener>();
  GetCanvasElement()->AddListener(listener);
  EXPECT_EQ(
      GetCanvasElement()->GetAnimationStateForTesting(),
      CanvasResourceDispatcher::AnimationState::kActiveWithSyntheticTiming);
  GetCanvasElement()->RemoveListener(listener);
}

TEST_F(OffscreenCanvasTest, SwitchFrameByCanvasImageSource) {
  auto* canvas = MakeGarbageCollected<OffscreenCanvas>(
      GetDocument().GetExecutionContext(), gfx::Size(100, 100));
  // Make sure the canvas has the context.
  ASSERT_TRUE(canvas->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(),
      CanvasRenderingContext::CanvasRenderingAPI::k2D, {}));
  auto* selector = canvas->GetFontSelector();
  uint32_t original_generation = FrameGenerationOf(*selector);

  // GetSourceImageForCanvas() should call UniqueFontSelector::DidSwitchFrame().
  SourceImageStatus source_image_status;
  canvas->GetSourceImageForCanvas(&source_image_status, {100, 100});
  EXPECT_GT(FrameGenerationOf(*selector), original_generation);
}

TEST_F(OffscreenCanvasTest, SwitchFrameByImageBitmapSource) {
  auto* canvas = MakeGarbageCollected<OffscreenCanvas>(
      GetDocument().GetExecutionContext(), gfx::Size(100, 100));
  // Make sure the canvas has the context.
  ASSERT_TRUE(canvas->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(),
      CanvasRenderingContext::CanvasRenderingAPI::k2D, {}));
  auto* selector = canvas->GetFontSelector();
  uint32_t original_generation = FrameGenerationOf(*selector);

  // The ImageBitmap constructor should call
  // UniqueFontSelector::DidSwitchFrame().
  MakeGarbageCollected<ImageBitmap>(canvas, std::nullopt);
  EXPECT_GT(FrameGenerationOf(*selector), original_generation);
}

// Verifies that an offscreen_canvas()s PushFrame() has the appropriate
// opacity/blending information sent to the CompositorFrameSink.
TEST_P(OffscreenCanvasTest, CompositorFrameOpacity) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  ScriptState::Scope scope(GetScriptState());
  ::testing::InSequence s;

  // To intercept SubmitCompositorFrame messages sent by OffscreenCanvas's
  // CanvasResourceDispatcher, we have to override the Mojo
  // EmbeddedFrameSinkProvider interface impl and its
  // CompositorFrameSinkClient.
  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Receiver<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_receiver(&mock_embedded_frame_sink_provider);
  auto override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_receiver);

  // Call here DidDraw() to simulate having drawn something before PushFrame();
  // DidDraw() will in turn cause a CanvasResourceDispatcher to be created and
  // a CreateCompositorFrameSink() to be issued; this sink will get a
  // SetNeedsBeginFrame() message sent upon construction.
  mock_embedded_frame_sink_provider
      .set_num_expected_set_needs_begin_frame_on_sink_construction(1);
  EXPECT_CALL(mock_embedded_frame_sink_provider,
              CreateCompositorFrameSink_(viz::FrameSinkId(kClientId, kSinkId)));
  offscreen_canvas().DidDraw();
  platform->RunUntilIdle();

  const bool context_alpha = GetParam().alpha;

  auto canvas_resource = CanvasResourceSharedImage::CreateSoftware(
      offscreen_canvas().Size(), viz::SinglePlaneFormat::kBGRA_8888,
      kPremul_SkAlphaType, gfx::ColorSpace::CreateSRGB(),
      /*provider=*/nullptr, shared_image_interface_provider());
  EXPECT_TRUE(!!canvas_resource);

  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SubmitCompositorFrame_(_))
      .WillOnce(::testing::WithArg<0>(
          [context_alpha](const viz::CompositorFrame* frame) {
            ASSERT_EQ(frame->render_pass_list.size(), 1u);

            const auto& quad_list = frame->render_pass_list[0]->quad_list;
            ASSERT_EQ(quad_list.size(), 1u);
            EXPECT_EQ(quad_list.front()->needs_blending, context_alpha);

            const auto& shared_quad_state_list =
                frame->render_pass_list[0]->shared_quad_state_list;
            ASSERT_EQ(shared_quad_state_list.size(), 1u);
            EXPECT_NE(shared_quad_state_list.front()->are_contents_opaque,
                      context_alpha);
          }));
  offscreen_canvas().PushFrame(std::move(canvas_resource),
                               SkIRect::MakeWH(10, 10));
  platform->RunUntilIdle();
}

TEST_P(OffscreenCanvasTest, GetRasterModeAutoRecovery) {
  // Verifies that after a context loss, getting the raster mode from the
  // canvas will restore the context and succeed.
  GetGLInterface()->SetIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoringForTesting());
  offscreen_canvas().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  EXPECT_EQ(offscreen_canvas().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoringForTesting());
}

const TestParams kTestCases[] = {
    {false /* alpha */, false /* desynchronized */},
    {false, true},
    {true, false},
    {true, true}};

INSTANTIATE_TEST_SUITE_P(All, OffscreenCanvasTest, ValuesIn(kTestCases));
}  // namespace blink
