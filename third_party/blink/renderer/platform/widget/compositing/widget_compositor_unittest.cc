// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/widget_compositor.h"

#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/types/pass_key.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

class StubWidgetBaseClient : public WidgetBaseClient {
 public:
  void OnCommitRequested() override {}
  void BeginMainFrame(base::TimeTicks) override {}
  void UpdateLifecycle(WebLifecycleUpdate, DocumentUpdateReason) override {}
  std::unique_ptr<cc::LayerTreeFrameSink> AllocateNewLayerTreeFrameSink()
      override {
    return nullptr;
  }
  KURL GetURLForDebugTrace() override { return {}; }
  WebInputEventResult DispatchBufferedTouchEvents() override {
    return WebInputEventResult::kNotHandled;
  }
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override {
    return WebInputEventResult::kNotHandled;
  }
  bool SupportsBufferedTouchEvents() override { return false; }
  void WillHandleGestureEvent(const WebGestureEvent&, bool* suppress) override {
  }
  void WillHandleMouseEvent(const WebMouseEvent&) override {}
  void ObserveGestureEventAndResult(const WebGestureEvent&,
                                    const gfx::Vector2dF&,
                                    const cc::OverscrollBehavior&,
                                    bool) override {}
  void FocusChanged(mojom::blink::FocusState) override {}
  void UpdateVisualProperties(
      const VisualProperties& visual_properties) override {}
  const display::ScreenInfos& GetOriginalScreenInfos() override {
    return screen_infos_;
  }
  gfx::Rect ViewportVisibleRect() override { return gfx::Rect(); }

 private:
  display::ScreenInfos screen_infos_;
};

class FakeWidgetCompositor : public WidgetCompositor {
 public:
  static scoped_refptr<FakeWidgetCompositor> Create(
      cc::LayerTreeHost* layer_tree_host,
      base::WeakPtr<WidgetBase> widget_base,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      mojo::PendingReceiver<mojom::blink::WidgetCompositor> receiver) {
    auto compositor = base::MakeRefCounted<FakeWidgetCompositor>(
        WidgetCompositorPassKeyProvider::GetPassKey(), layer_tree_host,
        std::move(widget_base), std::move(main_task_runner),
        std::move(compositor_task_runner));
    compositor->BindOnThread(std::move(receiver));
    return compositor;
  }

  FakeWidgetCompositor(
      base::PassKey<WidgetCompositorPassKeyProvider> pass_key,
      cc::LayerTreeHost* layer_tree_host,
      base::WeakPtr<WidgetBase> widget_base,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
      : WidgetCompositor(std::move(pass_key),
                         widget_base,
                         std::move(main_task_runner),
                         std::move(compositor_task_runner)),
        layer_tree_host_(layer_tree_host) {}

  cc::LayerTreeHost* LayerTreeHost() const override { return layer_tree_host_; }

  raw_ptr<cc::LayerTreeHost> layer_tree_host_;

 private:
  friend class ThreadSafeRefCounted<FakeWidgetCompositor>;
  ~FakeWidgetCompositor() override = default;
};

class WidgetCompositorTest : public cc::LayerTreeTest {
 public:
  using CompositorMode = cc::CompositorMode;

  void BeginTest() override {
    mojo::AssociatedRemote<mojom::blink::Widget> widget_remote;
    mojo::PendingAssociatedReceiver<mojom::blink::Widget> widget_receiver =
        widget_remote.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<mojom::blink::WidgetHost> widget_host_remote;
    std::ignore = widget_host_remote.BindNewEndpointAndPassDedicatedReceiver();

    widget_base_ = std::make_unique<WidgetBase>(
        /*widget_base_client=*/&client_, widget_host_remote.Unbind(),
        std::move(widget_receiver),
        scheduler::GetSingleThreadTaskRunnerForTesting(),
        /*is_hidden=*/false,
        /*never_composited=*/false,
        /*is_for_child_local_root=*/false,
        /*is_for_scalable_page=*/true);

    widget_compositor_ = FakeWidgetCompositor::Create(
        layer_tree_host(), widget_base_->GetWeakPtr(),
        layer_tree_host()->GetTaskRunnerProvider()->MainThreadTaskRunner(),
        layer_tree_host()->GetTaskRunnerProvider()->ImplThreadTaskRunner(),
        remote_.BindNewPipeAndPassReceiver());

    remote_->VisualStateRequest(base::BindOnce(
        &WidgetCompositorTest::VisualStateResponse, base::Unretained(this)));
    PostSetNeedsCommitToMainThread();
  }

  void VisualStateResponse() {
    if (second_run_with_null_) {
      widget_base_.reset();
      remote_->VisualStateRequest(base::BindOnce(
          &WidgetCompositorTest::VisualStateResponse, base::Unretained(this)));
    }

    is_callback_run_ = true;
    widget_compositor_->Shutdown();
    widget_compositor_ = nullptr;
    EndTest();
  }

  void AfterTest() override { EXPECT_TRUE(is_callback_run_); }

 protected:
  void set_second_run_with_null() { second_run_with_null_ = true; }

 private:
  mojo::Remote<mojom::blink::WidgetCompositor> remote_;
  StubWidgetBaseClient client_;
  std::unique_ptr<WidgetBase> widget_base_;
  scoped_refptr<FakeWidgetCompositor> widget_compositor_;
  bool is_callback_run_ = false;
  bool second_run_with_null_ = false;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(WidgetCompositorTest);

class WidgetCompositorWithNullWidgetBaseTest : public WidgetCompositorTest {
  void BeginTest() override {
    set_second_run_with_null();
    WidgetCompositorTest::BeginTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(WidgetCompositorWithNullWidgetBaseTest);

}  // namespace blink
