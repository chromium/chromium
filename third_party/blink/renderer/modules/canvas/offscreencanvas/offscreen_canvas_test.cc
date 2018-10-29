// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/html_canvas_element_module.h"
#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_embedded_frame_sink_provider.h"
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
  bool low_latency;
};
}  // unnamed namespace

class OffscreenCanvasTest : public PageTestBase,
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

 private:
  Persistent<OffscreenCanvas> offscreen_canvas_;
  Persistent<OffscreenCanvasRenderingContext2D> context_;
  FakeGLES2Interface gl_;
};

OffscreenCanvasTest::OffscreenCanvasTest() = default;

void OffscreenCanvasTest::SetUp() {
  auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
      -> std::unique_ptr<WebGraphicsContext3DProvider> {
    *gpu_compositing_disabled = false;
    gl->SetIsContextLost(false);
    return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
  };
  SharedGpuContext::SetContextProviderFactoryForTesting(
      WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
  PageTestBase::SetUp();
  SetHtmlInnerHTML("<body><canvas id='c'></canvas></body>");
  HTMLCanvasElement* canvas_element = ToHTMLCanvasElement(GetElementById("c"));

  DummyExceptionStateForTesting exception_state;
  offscreen_canvas_ = HTMLCanvasElementModule::transferControlToOffscreen(
      *canvas_element, exception_state);
  // |offscreen_canvas_| should inherit the FrameSinkId from |canvas_element|s
  // SurfaceLayerBridge, but in tests this id is zero; fill it up by hand.
  offscreen_canvas_->SetFrameSinkId(kClientId, kSinkId);

  CanvasContextCreationAttributesCore attrs;
  if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
    attrs.alpha = GetParam().alpha;
    attrs.low_latency = GetParam().low_latency;
  }
  context_ = static_cast<OffscreenCanvasRenderingContext2D*>(
      offscreen_canvas_->GetCanvasRenderingContext(&GetDocument(), String("2d"),
                                                   attrs));
}

void OffscreenCanvasTest::TearDown() {
  SharedGpuContext::ResetForTesting();
}

TEST_F(OffscreenCanvasTest, AnimationNotInitiallySuspended) {
  ScriptState::Scope scope(GetScriptState());
  EXPECT_FALSE(Dispatcher()->IsAnimationSuspended());
}

// Verifies that an offscreen_canvas()s PushFrame()/Commit() has the appropriate
// opacity/blending information sent to the CompositorFrameSink.
TEST_P(OffscreenCanvasTest, CompositorFrameOpacity) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  ScriptState::Scope scope(GetScriptState());
  ::testing::InSequence s;

  // To intercept SubmitCompositorFrame/SubmitCompositorFrameSync messages sent
  // by OffscreenCanvas's CanvasResourceDispatcher, we have to override the Mojo
  // EmbeddedFrameSinkProvider interface impl and its CompositorFrameSinkClient.
  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Binding<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_binding(&mock_embedded_frame_sink_provider);
  auto override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_binding);

  // Call here DidDraw() to simulate having drawn something before PushFrame()/
  // Commit(); DidDraw() will in turn cause a CanvasResourceDispatcher to be
  // created and a CreateCompositorFrameSink() to be issued; this sink will get
  // a SetNeedsBeginFrame() message sent upon construction.
  mock_embedded_frame_sink_provider
      .set_num_expected_set_needs_begin_frame_on_sink_construction(1);
  EXPECT_CALL(mock_embedded_frame_sink_provider,
              CreateCompositorFrameSink_(viz::FrameSinkId(kClientId, kSinkId)));
  offscreen_canvas().DidDraw();
  platform->RunUntilIdle();

  const bool context_alpha = GetParam().alpha;

  const auto canvas_resource = CanvasResourceSharedBitmap::Create(
      offscreen_canvas().Size(), CanvasColorParams(), nullptr /* provider */,
      kLow_SkFilterQuality);
  EXPECT_TRUE(!!canvas_resource);

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
  offscreen_canvas().PushFrame(canvas_resource, SkIRect::MakeWH(10, 10));
  platform->RunUntilIdle();

  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SubmitCompositorFrameSync_(_))
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
  offscreen_canvas().Commit(canvas_resource, SkIRect::MakeWH(10, 10));
  platform->RunUntilIdle();
}

const TestParams kTestCases[] = {{false /* alpha */, false /* low_latency */},
                                 {false, true},
                                 {true, false},
                                 {true, true}};

INSTANTIATE_TEST_CASE_P(, OffscreenCanvasTest, ValuesIn(kTestCases));
}  // namespace blink
