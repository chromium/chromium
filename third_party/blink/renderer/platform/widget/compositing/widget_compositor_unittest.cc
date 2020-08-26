// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/widget_compositor.h"

#include "base/test/task_environment.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"

namespace blink {

class StubWidgetBaseClient : public WidgetBaseClient {
 public:
  void BeginMainFrame(base::TimeTicks) override {}
  void RecordTimeToFirstActivePaint(base::TimeDelta) override {}
  void UpdateLifecycle(WebLifecycleUpdate, DocumentUpdateReason) override {}
  void RequestNewLayerTreeFrameSink(LayerTreeFrameSinkCallback) override {}
  WebInputEventResult DispatchBufferedTouchEvents() override {
    return WebInputEventResult::kNotHandled;
  }
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override {
    return WebInputEventResult::kNotHandled;
  }
  bool SupportsBufferedTouchEvents() override { return false; }
  bool WillHandleGestureEvent(const WebGestureEvent&) override { return false; }
  bool WillHandleMouseEvent(const WebMouseEvent&) override { return false; }
  void ObserveGestureEventAndResult(const WebGestureEvent&,
                                    const gfx::Vector2dF&,
                                    const cc::OverscrollBehavior&,
                                    bool) override {}
  void FocusChanged(bool) override {}
  void UpdateVisualProperties(
      const VisualProperties& visual_properties) override {}
  const ScreenInfo& GetOriginalScreenInfo() override { return screen_info_; }
  gfx::Rect ViewportVisibleRect() override { return gfx::Rect(); }

 private:
  ScreenInfo screen_info_;
};

class FakeWidgetCompositor : public WidgetCompositor {
 public:
  FakeWidgetCompositor(
      cc::LayerTreeHost* layer_tree_host,
      base::WeakPtr<WidgetBase> widget_base,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      mojo::PendingReceiver<mojom::blink::WidgetCompositor> receiver)
      : WidgetCompositor(widget_base,
                         std::move(main_task_runner),
                         std::move(compositor_task_runner),
                         std::move(receiver)),
        layer_tree_host_(layer_tree_host) {}

  cc::LayerTreeHost* LayerTreeHost() const override { return layer_tree_host_; }

  cc::LayerTreeHost* layer_tree_host_;
};

class WidgetCompositorTest : public cc::LayerTreeTest {
 public:
  using CompositorMode = cc::CompositorMode;

  void BeginTest() override {
    widget_base_ = std::make_unique<WidgetBase>(
        &client_,
        blink::CrossVariantMojoAssociatedRemote<
            blink::mojom::WidgetHostInterfaceBase>(),
        blink::CrossVariantMojoAssociatedReceiver<
            blink::mojom::WidgetInterfaceBase>());

    widget_compositor_ = base::MakeRefCounted<FakeWidgetCompositor>(
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
