// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_compositing_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class WebGLTestPlatform : public GpuCompositingTestPlatform {
 public:
  std::unique_ptr<WebGraphicsContext3DProvider>
  CreateWebGLGraphicsContextProvider(
      bool prefer_low_power_gpu,
      bool fail_if_major_performance_caveat,
      Platform::WebGLContextType context_type,
      const WebURL& document_url,
      Platform::WebGLContextInfo* gl_info) override {
    if (gl_info) {
      gl_info->using_gpu_compositing = true;
    }
    auto provider = std::make_unique<WebGraphicsContext3DProviderForTests>(
        std::make_unique<testing::NiceMock<GLES2InterfaceForTests>>());
    provider->GetMutableGpuFeatureInfo()
        .enabled_gpu_driver_bug_workarounds.push_back(
            gpu::DISABLE_GL_RGB_FORMAT);
    return provider;
  }
};

class CountingEventListener final : public NativeEventListener {
 public:
  explicit CountingEventListener(bool prevent_default)
      : prevent_default_(prevent_default) {}

  void Invoke(ExecutionContext*, Event* event) override {
    ++count_;
    if (prevent_default_) {
      event->preventDefault();
    }
  }

  int Count() const { return count_; }

 private:
  const bool prevent_default_;
  int count_ = 0;
};

}  // namespace

class WebGLRenderingContextBaseTest : public testing::Test {
 protected:
  // Initialize features before `task_environment_` starts ThreadPool threads.
  WebGLRenderingContextBaseTest()
      : feature_list_({blink::features::kWebGLDiscardBackBuffer,
                       ::features::kWebGLDeleteBuffersInBackground},
                      {}) {}

  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::CreateRaster();
    InitializeSharedGpuContext(test_context_provider_.get());

    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    page_holder_->GetDocument()
        .documentElement()
        ->SetInnerHTMLWithoutTrustedTypes(
            "<body><canvas id='c' width='100' height='100'></canvas></body>");
    canvas_ = To<HTMLCanvasElement>(
        page_holder_->GetDocument().getElementById(AtomicString("c")));
    ASSERT_NE(canvas_, nullptr);
  }

  void TearDown() override {
    CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
    canvas_ = nullptr;
    page_holder_.reset();
    SharedGpuContext::Reset();
  }

  WebGLRenderingContextBase* CreateWebGLContext(
      const CanvasContextCreationAttributesCore& attrs) {
    auto* webgl =
        DynamicTo<WebGLRenderingContextBase>(canvas_->GetCanvasRenderingContext(
            page_holder_->GetDocument().GetExecutionContext(), "webgl", attrs));
    if (!webgl) {
      return nullptr;
    }
    // Present once so contents_changed_ is cleared and hiding discards buffers.
    viz::TransferableResource resource;
    viz::ReleaseCallback release_callback;
    if (webgl->GetDrawingBuffer()->PrepareTransferableResource(
            &resource, &release_callback) &&
        release_callback) {
      std::move(release_callback).Run(gpu::SyncToken(), /*is_lost=*/false);
    }
    return webgl;
  }

  void SetPageHidden(bool hidden) {
    page_holder_->GetDocument().GetPage()->SetVisibilityState(
        hidden ? mojom::blink::PageVisibilityState::kHidden
               : mojom::blink::PageVisibilityState::kVisible,
        /*is_initial_state=*/false);
  }

  static GLES2InterfaceForTests* ContextGL(WebGLRenderingContextBase* gl_ctx) {
    return static_cast<GLES2InterfaceForTests*>(
        gl_ctx->GetDrawingBuffer()->ContextGL());
  }

  static SharedImageInterfaceForTests* SII(WebGLRenderingContextBase* gl_ctx) {
    return static_cast<SharedImageInterfaceForTests*>(
        gl_ctx->SharedImageInterface());
  }

  // Before the task environment because it does not support getting destructed
  // concurrently to base::Feature accesses.
  base::test::ScopedFeatureList feature_list_;
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingPlatformSupport<WebGLTestPlatform> platform_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<HTMLCanvasElement> canvas_;
};

// Regression test for crbug.com/514593915: when EnsureBuffers() fails while
// hidden, same-turn clear (with RGB emulation), copyTexImage2D, resize, and
// snapshot calls unwind cleanly without re-entrant destruction, dispatch a
// single webglcontextlost, and auto-recover once visible.
TEST_F(WebGLRenderingContextBaseTest,
       EnsureBuffersFailureUnwindsAndAutoRecovers) {
  auto* lost_listener =
      MakeGarbageCollected<CountingEventListener>(/*prevent_default=*/true);
  auto* restored_listener =
      MakeGarbageCollected<CountingEventListener>(/*prevent_default=*/false);
  canvas_->addEventListener(event_type_names::kWebglcontextlost, lost_listener);
  canvas_->addEventListener(event_type_names::kWebglcontextrestored,
                            restored_listener);

  CanvasContextCreationAttributesCore attrs;
  attrs.alpha = false;
  attrs.depth = true;
  attrs.stencil = true;
  attrs.preserve_drawing_buffer = false;
  WebGLRenderingContextBase* gl_ctx = CreateWebGLContext(attrs);
  ASSERT_NE(gl_ctx, nullptr);

  WebGLTexture* tex = gl_ctx->createTexture();
  ASSERT_NE(tex, nullptr);
  gl_ctx->bindTexture(GL_TEXTURE_2D, tex);

  SetPageHidden(true);
  // Fail on the second CheckFramebufferStatus call
  // (EnsureMSAADepthStencilBuffers after EnsureBackColorBuffer succeeds).
  ContextGL(gl_ctx)->SetFramebufferStatus(0, /*fail_on_nth_call=*/2);

  scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        gl_ctx->clear(GL_COLOR_BUFFER_BIT);
        gl_ctx->copyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 10, 10, 0);
        canvas_->SetSize(gfx::Size(200, 200));
        CanvasRenderingContext* rendering_ctx = gl_ctx;
        EXPECT_EQ(nullptr,
                  rendering_ctx->PaintRenderingResultsToSnapshot(kFrontBuffer));
        // Context loss is posted rather than executed re-entrantly mid-call.
        EXPECT_FALSE(gl_ctx->isContextLost());
      }));
  test::RunPendingTasks();

  EXPECT_TRUE(gl_ctx->isContextLost());
  EXPECT_EQ(1, lost_listener->Count());

  SetPageHidden(false);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, restored_listener->Count());
  EXPECT_FALSE(gl_ctx->isContextLost());
}

// Verifies that gl.bindFramebuffer(gl.FRAMEBUFFER, null) still unbinds a user
// WebGLFramebuffer when EnsureBuffers() fails, and that an earlier context loss
// running ahead of the posted task does not synthesize GL_INVALID_OPERATION.
TEST_F(WebGLRenderingContextBaseTest,
       BindDefaultFramebufferFailureUnbindsUserFboAndSkipsDuplicateLoss) {
  auto* lost_listener =
      MakeGarbageCollected<CountingEventListener>(/*prevent_default=*/false);
  canvas_->addEventListener(event_type_names::kWebglcontextlost, lost_listener);

  CanvasContextCreationAttributesCore attrs;
  attrs.depth = true;
  attrs.preserve_drawing_buffer = false;
  WebGLRenderingContextBase* gl_ctx = CreateWebGLContext(attrs);
  ASSERT_NE(gl_ctx, nullptr);

  WebGLFramebuffer* user_fbo = gl_ctx->createFramebuffer();
  ASSERT_NE(user_fbo, nullptr);
  const GLuint user_fbo_id = user_fbo->Object();

  SetPageHidden(true);
  gl_ctx->bindFramebuffer(GL_FRAMEBUFFER, user_fbo);
  GLES2InterfaceForTests* gl = ContextGL(gl_ctx);
  SII(gl_ctx)->SetFailSharedImageCreation(true);

  gl_ctx->bindFramebuffer(GL_FRAMEBUFFER, nullptr);
  GLint bound_fbo = 0;
  gl->GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bound_fbo);
  EXPECT_NE(static_cast<GLint>(user_fbo_id), bound_fbo);
  EXPECT_NE(0, bound_fbo);

  // Simulate CommandBufferProxyImpl's lost-context callback running ahead of
  // ForceLostContextFromDrawingBuffer.
  gl_ctx->ForceLostContext(WebGLRenderingContextBase::kRealLostContext,
                           WebGLRenderingContextBase::kAuto);
  test::RunPendingTasks();

  EXPECT_EQ(static_cast<GLenum>(GC3D_CONTEXT_LOST_WEBGL), gl_ctx->getError());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_ctx->getError());
  EXPECT_EQ(1, lost_listener->Count());
}

}  // namespace blink
