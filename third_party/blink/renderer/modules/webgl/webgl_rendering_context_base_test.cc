// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "cc/layers/layer.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_compositing_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "v8/include/cppgc/common.h"
#include "v8/include/v8-cppgc.h"

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
    return std::make_unique<WebGraphicsContext3DProviderForTests>(
        std::make_unique<testing::NiceMock<GLES2InterfaceForTests>>());
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

// Verifies that while a detached WebGL context remains reachable, GC preserves
// its DrawingBuffer and compositor layer updates, and once unreachable, Oilpan
// pre-finalization tears down the DrawingBuffer and detaches its layer client
// before incremental sweeping finalizes the C++ object.
TEST_F(WebGLRenderingContextBaseTest,
       DetachesTextureLayerBeforeIncrementalSweepingFinalizesContext) {
  LayerTreeHostEmbedder layer_tree;
  scoped_refptr<cc::Layer> retained_layer;
  scoped_refptr<DrawingBuffer> retained_buffer;
  WeakPersistent<WebGLRenderingContextBase> weak_ctx;

  v8::CppHeap& cpp_heap = ThreadState::Current()->cpp_heap();
  cpp_heap.SetForceIncrementalSweepingForTesting(true);
  base::ScopedClosureRunner restore_sweeping(base::BindLambdaForTesting([&]() {
    cpp_heap.FinishSweepingForTesting();
    cpp_heap.SetForceIncrementalSweepingForTesting(false);
  }));

  {
    Persistent<WebGLRenderingContextBase> strong_ctx;
    {
      CanvasContextCreationAttributesCore attrs;
      WebGLRenderingContextBase* gl_ctx = CreateWebGLContext(attrs);
      ASSERT_NE(gl_ctx, nullptr);
      strong_ctx = gl_ctx;
      weak_ctx = gl_ctx;

      WebGLTexture* tex = gl_ctx->createTexture();
      ASSERT_NE(tex, nullptr);
      gl_ctx->bindTexture(GL_TEXTURE_2D, tex);

      retained_buffer = gl_ctx->GetDrawingBuffer();
      ASSERT_NE(retained_buffer, nullptr);
      retained_layer = static_cast<CanvasRenderingContext*>(gl_ctx)->CcLayer();
      ASSERT_NE(retained_layer, nullptr);
      layer_tree.layer_tree_host()->SetRootLayer(retained_layer);
    }

    // Detach the canvas from DOM while `strong_ctx` remains reachable. GC must
    // preserve the DrawingBuffer and allow normal compositor layer updates.
    canvas_->remove();
    canvas_ = nullptr;
    cpp_heap.CollectGarbageForTesting(
        cppgc::EmbedderStackState::kNoHeapPointers);
    cpp_heap.FinishSweepingForTesting();

    ASSERT_FALSE(strong_ctx->isContextLost());
    EXPECT_FALSE(retained_buffer->destroyed());
    EXPECT_TRUE(retained_layer->draws_content());
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE, base::BindLambdaForTesting(
                       [&]() { strong_ctx->clear(GL_COLOR_BUFFER_BIT); }));
    test::RunPendingTasks();
    EXPECT_TRUE(retained_layer->Update());

    // Dirty the back buffer again, then drop the remaining strong reference
    // while `layer_tree` and `retained_buffer` stay alive.
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE, base::BindLambdaForTesting(
                       [&]() { strong_ctx->clear(GL_COLOR_BUFFER_BIT); }));
    test::RunPendingTasks();
  }

  // Trigger a single garbage collection with incremental sweeping forced so
  // pre-finalizers execute while C++ destructor finalization remains deferred.
  cpp_heap.CollectGarbageForTesting(cppgc::EmbedderStackState::kNoHeapPointers);

  ASSERT_EQ(nullptr, weak_ctx.Get());
  EXPECT_TRUE(retained_buffer->destroyed());
  EXPECT_EQ(nullptr, retained_buffer->client());
  EXPECT_EQ(nullptr, retained_buffer->CcLayer());
  EXPECT_FALSE(retained_layer->draws_content());
  EXPECT_FALSE(retained_layer->Update());

  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  EXPECT_FALSE(retained_buffer->PrepareTransferableResource(&resource,
                                                            &release_callback));
}

// Verifies that synchronous non-GC Dispose() marks the context lost, detaches
// its DrawingBuffer and layer client, removes it from ActiveContexts() across
// subsequent context evictions, and remains idempotent across GC.
TEST_F(WebGLRenderingContextBaseTest,
       ManualDisposeDeactivatesContextAndIsIdempotentAcrossGC) {
  LayerTreeHostEmbedder layer_tree;
  scoped_refptr<cc::Layer> retained_layer;
  scoped_refptr<DrawingBuffer> retained_buffer;
  WeakPersistent<WebGLRenderingContextBase> weak_disposed_ctx;

  {
    CanvasContextCreationAttributesCore attrs;
    WebGLRenderingContextBase* gl_ctx = CreateWebGLContext(attrs);
    ASSERT_NE(gl_ctx, nullptr);
    weak_disposed_ctx = gl_ctx;

    CanvasRenderingContext* rendering_ctx = gl_ctx;
    retained_buffer = gl_ctx->GetDrawingBuffer();
    ASSERT_NE(retained_buffer, nullptr);
    retained_layer = rendering_ctx->CcLayer();
    ASSERT_NE(retained_layer, nullptr);
    layer_tree.layer_tree_host()->SetRootLayer(retained_layer);

    scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE, base::BindLambdaForTesting(
                       [gl_ctx]() { gl_ctx->clear(GL_COLOR_BUFFER_BIT); }));
    test::RunPendingTasks();
    rendering_ctx->DisposeForTesting();

    EXPECT_TRUE(gl_ctx->isContextLost());
    EXPECT_EQ(nullptr, gl_ctx->GetDrawingBuffer());
    EXPECT_EQ(nullptr, rendering_ctx->CcLayer());
    EXPECT_EQ(0, gl_ctx->drawingBufferWidth());
    EXPECT_TRUE(retained_buffer->destroyed());
    EXPECT_EQ(nullptr, retained_buffer->client());
    EXPECT_EQ(nullptr, retained_buffer->CcLayer());
    EXPECT_FALSE(retained_layer->Update());

    HeapVector<Member<HTMLCanvasElement>> extra_canvases;
    for (int i = 0; i < 17; ++i) {
      auto* extra_canvas = To<HTMLCanvasElement>(
          page_holder_->GetDocument().CreateRawElement(html_names::kCanvasTag));
      extra_canvases.push_back(extra_canvas);
      EXPECT_NE(nullptr, extra_canvas->GetCanvasRenderingContext(
                             page_holder_->GetDocument().GetExecutionContext(),
                             "webgl", attrs));
    }
    test::RunPendingTasks();

    canvas_->remove();
    canvas_ = nullptr;
  }

  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_EQ(nullptr, weak_disposed_ctx.Get());
}

// Verifies that evicted contexts are scheduled for restoration during Oilpan
// pre-finalization (both when an active context is collected and when a
// stopped context is collected) before deferred sweeping completes.
TEST_F(WebGLRenderingContextBaseTest,
       RestoresEvictedContextDuringPreFinalizerDisposal) {
  auto* lost_listener =
      MakeGarbageCollected<CountingEventListener>(/*prevent_default=*/true);
  auto* restored_listener =
      MakeGarbageCollected<CountingEventListener>(/*prevent_default=*/false);
  canvas_->addEventListener(event_type_names::kWebglcontextlost, lost_listener);
  canvas_->addEventListener(event_type_names::kWebglcontextrestored,
                            restored_listener);

  CanvasContextCreationAttributesCore attrs;
  WebGLRenderingContextBase* evicted_ctx = CreateWebGLContext(attrs);
  ASSERT_NE(evicted_ctx, nullptr);

  evicted_ctx->ForceLostContext(
      WebGLRenderingContextBase::kSyntheticLostContext,
      WebGLRenderingContextBase::kWhenAvailable);
  ASSERT_TRUE(evicted_ctx->isContextLost());
  test::RunPendingTasks();
  EXPECT_EQ(1, lost_listener->Count());
  EXPECT_EQ(0, restored_listener->Count());

  v8::CppHeap& cpp_heap = ThreadState::Current()->cpp_heap();
  cpp_heap.SetForceIncrementalSweepingForTesting(true);
  base::ScopedClosureRunner restore_sweeping(base::BindLambdaForTesting([&]() {
    cpp_heap.FinishSweepingForTesting();
    cpp_heap.SetForceIncrementalSweepingForTesting(false);
  }));

  {
    auto* extra =
        MakeGarbageCollected<HTMLCanvasElement>(page_holder_->GetDocument());
    extra->SetSize(gfx::Size(16, 16));
    page_holder_->GetDocument().body()->AppendChild(extra);
    ASSERT_NE(
        extra->GetCanvasRenderingContext(
            page_holder_->GetDocument().GetExecutionContext(), "webgl", attrs),
        nullptr);
    extra->remove();
  }

  cpp_heap.CollectGarbageForTesting(cppgc::EmbedderStackState::kNoHeapPointers);
  test::RunPendingTasks();

  EXPECT_EQ(1, restored_listener->Count());
  EXPECT_FALSE(evicted_ctx->isContextLost());

  // Also verify recovery when the collected context had already been stopped.
  evicted_ctx->ForceLostContext(
      WebGLRenderingContextBase::kSyntheticLostContext,
      WebGLRenderingContextBase::kWhenAvailable);
  ASSERT_TRUE(evicted_ctx->isContextLost());
  test::RunPendingTasks();
  EXPECT_EQ(2, lost_listener->Count());
  EXPECT_EQ(1, restored_listener->Count());

  {
    auto* stopped_canvas =
        MakeGarbageCollected<HTMLCanvasElement>(page_holder_->GetDocument());
    stopped_canvas->SetSize(gfx::Size(16, 16));
    page_holder_->GetDocument().body()->AppendChild(stopped_canvas);
    auto* stopped_ctx = DynamicTo<WebGLRenderingContextBase>(
        stopped_canvas->GetCanvasRenderingContext(
            page_holder_->GetDocument().GetExecutionContext(), "webgl", attrs));
    ASSERT_NE(stopped_ctx, nullptr);
    static_cast<CanvasRenderingContext*>(stopped_ctx)->Stop();
    EXPECT_EQ(nullptr, stopped_ctx->GetDrawingBuffer());
    stopped_canvas->remove();
  }

  cpp_heap.CollectGarbageForTesting(cppgc::EmbedderStackState::kNoHeapPointers);
  test::RunPendingTasks();

  EXPECT_EQ(2, restored_listener->Count());
  EXPECT_FALSE(evicted_ctx->isContextLost());
}

// Verifies that Worker heap termination (which clears thread-local Persistents
// before invoking pre-finalizers under DisallowGarbageCollectionScope) cleanly
// disposes both active and forcibly-evicted OffscreenCanvas WebGL contexts.
TEST_F(WebGLRenderingContextBaseTest,
       WorkerTerminationWithActiveAndEvictedWebGLContexts) {
  MockWorkerReportingProxy reporting_proxy;
  auto worker_thread = std::make_unique<WorkerThreadForTest>(reporting_proxy);
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::Create(KURL("http://fake.url/"));
  worker_thread->StartWithSourceCode(security_origin.get(), "// worker");
  reporting_proxy.WaitUntilDidEvaluateTopLevelScript();

  bool created_contexts = false;
  base::RunLoop run_loop;
  PostCrossThreadTask(
      *worker_thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(
          [](WorkerThreadForTest* thread, bool* out_created,
             CrossThreadOnceClosure quit_closure) {
            ExecutionContext* execution_context = thread->GlobalScope();
            CanvasContextCreationAttributesCore attrs;
            auto* canvas1 = MakeGarbageCollected<OffscreenCanvas>(
                execution_context, gfx::Size(32, 32), 0, 0, kInvalidDOMNodeId);
            auto* gl1 = DynamicTo<WebGLRenderingContextBase>(
                canvas1->GetCanvasRenderingContext(
                    execution_context,
                    CanvasRenderingContext::CanvasRenderingAPI::kWebgl, attrs));
            auto* canvas2 = MakeGarbageCollected<OffscreenCanvas>(
                execution_context, gfx::Size(32, 32), 0, 0, kInvalidDOMNodeId);
            auto* gl2 = DynamicTo<WebGLRenderingContextBase>(
                canvas2->GetCanvasRenderingContext(
                    execution_context,
                    CanvasRenderingContext::CanvasRenderingAPI::kWebgl, attrs));
            if (gl1 && gl2) {
              gl2->ForceLostContext(
                  WebGLRenderingContextBase::kSyntheticLostContext,
                  WebGLRenderingContextBase::kWhenAvailable);
              *out_created = !gl1->isContextLost() && gl2->isContextLost();
            }
            PostCrossThreadTask(
                *scheduler::GetSingleThreadTaskRunnerForTesting(), FROM_HERE,
                std::move(quit_closure));
          },
          CrossThreadUnretained(worker_thread.get()),
          CrossThreadUnretained(&created_contexts),
          CrossThreadOnceClosure(run_loop.QuitClosure())));
  run_loop.Run();
  EXPECT_TRUE(created_contexts);

  worker_thread->Terminate();
  worker_thread->WaitForShutdownForTesting();
}

}  // namespace blink
