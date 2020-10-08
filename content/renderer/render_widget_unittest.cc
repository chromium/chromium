// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/test/fake_render_widget_host.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_widget_delegate.h"
#include "content/test/fake_compositor_dependencies.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"
#include "third_party/blink/public/mojom/page/widget.mojom-test-utils.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;

namespace cc {
class AnimationHost;
}

namespace content {

namespace {

std::unique_ptr<AgentSchedulingGroup> CreateAgentSchedulingGroup(
    RenderThread& render_thread) {
  mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
      agent_scheduling_group_host;
  ignore_result(
      agent_scheduling_group_host.InitWithNewEndpointAndPassReceiver());
  mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
      agent_scheduling_group_mojo;
  return std::make_unique<AgentSchedulingGroup>(
      render_thread, std::move(agent_scheduling_group_host),
      std::move(agent_scheduling_group_mojo),
      base::OnceCallback<void(const AgentSchedulingGroup*)>());
}

}  // namespace

class InteractiveRenderWidget : public RenderWidget {
 public:
  explicit InteractiveRenderWidget(AgentSchedulingGroup& agent_scheduling_group,
                                   CompositorDependencies* compositor_deps)
      : RenderWidget(agent_scheduling_group,
                     ++next_routing_id_,
                     compositor_deps) {}

  void Init(blink::WebWidget* widget, const blink::ScreenInfo& screen_info) {
    Initialize(base::NullCallback(), widget, screen_info);
  }

  using RenderWidget::Close;

  IPC::TestSink* sink() { return &sink_; }

  std::unique_ptr<cc::LayerTreeFrameSink> AllocateNewLayerTreeFrameSink()
      override {
    std::unique_ptr<cc::FakeLayerTreeFrameSink> sink =
        cc::FakeLayerTreeFrameSink::Create3d();
    last_created_frame_sink_ = sink.get();
    return sink;
  }

  // The returned pointer is valid after RequestNewLayerTreeFrameSink() occurs,
  // until another call to RequestNewLayerTreeFrameSink() happens. It's okay to
  // use this pointer on the main thread because this class causes the
  // compositor to run in single thread mode by returning a null from
  // GetCompositorImplThreadTaskRunner().
  cc::FakeLayerTreeFrameSink* last_created_frame_sink() {
    return last_created_frame_sink_;
  }

 protected:
  bool Send(IPC::Message* msg) override {
    sink_.OnMessageReceived(*msg);
    delete msg;
    return true;
  }

 private:
  IPC::TestSink sink_;
  static int next_routing_id_;
  cc::FakeLayerTreeFrameSink* last_created_frame_sink_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(InteractiveRenderWidget);
};

int InteractiveRenderWidget::next_routing_id_ = 0;

class RenderWidgetUnittest : public testing::Test {
 public:
  explicit RenderWidgetUnittest(bool is_for_nested_main_frame = false)
      : is_for_nested_main_frame_(is_for_nested_main_frame) {}

  void SetUp() override {
    mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
        frame_widget_receiver =
            frame_widget_remote.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> frame_widget_host;
    mojo::PendingAssociatedReceiver<blink::mojom::FrameWidgetHost>
        frame_widget_host_receiver =
            frame_widget_host.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::Widget> widget_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::Widget> widget_receiver =
        widget_remote.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::WidgetHost> widget_host;
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
        widget_host_receiver =
            widget_host.BindNewEndpointAndPassDedicatedReceiver();

    web_view_ = blink::WebView::Create(/*client=*/&web_view_client_,
                                       /*is_hidden=*/false,
                                       /*is_inside_portal=*/false,
                                       /*compositing_enabled=*/true, nullptr,
                                       mojo::NullAssociatedReceiver());
    agent_scheduling_group_ = CreateAgentSchedulingGroup(render_thread_);
    widget_ = std::make_unique<InteractiveRenderWidget>(
        *agent_scheduling_group_, &compositor_deps_);
    web_local_frame_ = blink::WebLocalFrame::CreateMainFrame(
        web_view_, &web_frame_client_, nullptr,
        base::UnguessableToken::Create(), nullptr);
    web_frame_widget_ = blink::WebFrameWidget::CreateForMainFrame(
        widget_.get(), web_local_frame_, frame_widget_host.Unbind(),
        std::move(frame_widget_receiver), widget_host.Unbind(),
        std::move(widget_receiver), is_for_nested_main_frame_);
    widget_->Init(web_frame_widget_, blink::ScreenInfo());
    web_view_->DidAttachLocalMainFrame();
  }

  void TearDown() override {
    widget_->Close(std::move(widget_));
    web_local_frame_ = nullptr;
    web_frame_widget_ = nullptr;
    web_view_ = nullptr;
    // RenderWidget::Close() posts some destruction. Don't leak them.
    base::RunLoop loop;
    compositor_deps_.GetCleanupTaskRunner()->PostTask(FROM_HERE,
                                                      loop.QuitClosure());
    loop.Run();
  }

  InteractiveRenderWidget* widget() const { return widget_.get(); }

  blink::WebFrameWidget* frame_widget() const { return web_frame_widget_; }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  cc::FakeLayerTreeFrameSink* GetFrameSink() {
    return widget_->last_created_frame_sink();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  RenderProcess render_process_;
  MockRenderThread render_thread_;
  blink::WebViewClient web_view_client_;
  blink::WebView* web_view_;
  blink::WebLocalFrame* web_local_frame_;
  blink::WebFrameWidget* web_frame_widget_;
  blink::WebLocalFrameClient web_frame_client_;
  FakeCompositorDependencies compositor_deps_;
  std::unique_ptr<AgentSchedulingGroup> agent_scheduling_group_;
  std::unique_ptr<InteractiveRenderWidget> widget_;
  base::HistogramTester histogram_tester_;
  const bool is_for_nested_main_frame_;
};

class RenderWidgetSubFrameUnittest : public RenderWidgetUnittest {
 public:
  RenderWidgetSubFrameUnittest()
      : RenderWidgetUnittest(/*is_for_nested_main_frame=*/true) {}
};

// Tests that the value of VisualProperties::is_pinch_gesture_active is
// propagated to the LayerTreeHost when properties are synced for subframes.
TEST_F(RenderWidgetSubFrameUnittest,
       ActivePinchGestureUpdatesLayerTreeHostSubFrame) {
  cc::LayerTreeHost* layer_tree_host = widget()->layer_tree_host();
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  blink::VisualProperties visual_properties;

  // Sync visual properties on a child RenderWidget.
  visual_properties.is_pinch_gesture_active = true;
  widget()->GetWebWidget()->ApplyVisualProperties(visual_properties);
  // We expect the |is_pinch_gesture_active| value to propagate to the
  // LayerTreeHost for sub-frames. Since GesturePinch events are handled
  // directly in the main-frame's layer tree (and only there), information about
  // whether or not we're in a pinch gesture must be communicated separately to
  // sub-frame layer trees, via OnUpdateVisualProperties. This information
  // is required to allow sub-frame compositors to throttle rastering while
  // pinch gestures are active.
  EXPECT_TRUE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
  visual_properties.is_pinch_gesture_active = false;
  widget()->GetWebWidget()->ApplyVisualProperties(visual_properties);
  EXPECT_FALSE(layer_tree_host->is_external_pinch_gesture_active_for_testing());
}

#if defined(OS_ANDROID)
TEST_F(RenderWidgetUnittest, ForceSendMetadataOnInput) {
  cc::LayerTreeHost* layer_tree_host = widget()->layer_tree_host();
  // We should not have any force send metadata requests at start.
  EXPECT_FALSE(layer_tree_host->TakeForceSendMetadataRequest());
  // ShowVirtualKeyboard will trigger a text input state update.
  widget()->GetWebWidget()->ShowVirtualKeyboard();
  // We should now have a force send metadata request.
  EXPECT_TRUE(layer_tree_host->TakeForceSendMetadataRequest());
}
#endif  // !defined(OS_ANDROID)

class NotifySwapTimesRenderWidgetUnittest : public RenderWidgetUnittest {
 public:
  void SetUp() override {
    RenderWidgetUnittest::SetUp();

    viz::ParentLocalSurfaceIdAllocator allocator;

    // TODO(danakj): This usually happens through
    // RenderWidget::UpdateVisualProperties() and we are cutting past that for
    // some reason.
    allocator.GenerateId();
    widget()->layer_tree_host()->SetViewportRectAndScale(
        gfx::Rect(200, 100), 1.f, allocator.GetCurrentLocalSurfaceId());

    auto root_layer = cc::SolidColorLayer::Create();
    root_layer->SetBounds(gfx::Size(200, 100));
    root_layer->SetBackgroundColor(SK_ColorGREEN);
    widget()->layer_tree_host()->SetRootLayer(root_layer);

    auto color_layer = cc::SolidColorLayer::Create();
    color_layer->SetBounds(gfx::Size(100, 100));
    root_layer->AddChild(color_layer);
    color_layer->SetBackgroundColor(SK_ColorRED);
  }

  // |swap_to_presentation| determines how long after swap should presentation
  // happen. This can be negative, positive, or zero. If zero, an invalid (null)
  // presentation time is used.
  void CompositeAndWaitForPresentation(base::TimeDelta swap_to_presentation) {
    base::RunLoop swap_run_loop;
    base::RunLoop presentation_run_loop;

    // Register callbacks for swap time and presentation time.
    base::TimeTicks swap_time;
    frame_widget()->NotifySwapAndPresentationTime(
        base::BindOnce(
            [](base::OnceClosure swap_quit_closure, base::TimeTicks* swap_time,
               blink::WebSwapResult result, base::TimeTicks timestamp) {
              DCHECK(!timestamp.is_null());
              *swap_time = timestamp;
              std::move(swap_quit_closure).Run();
            },
            swap_run_loop.QuitClosure(), &swap_time),
        base::BindOnce(
            [](base::OnceClosure presentation_quit_closure,
               blink::WebSwapResult result, base::TimeTicks timestamp) {
              DCHECK(!timestamp.is_null());
              std::move(presentation_quit_closure).Run();
            },
            presentation_run_loop.QuitClosure()));

    // Composite and wait for the swap to complete.
    widget()->layer_tree_host()->Composite(base::TimeTicks::Now(),
                                           /*raster=*/true);
    swap_run_loop.Run();

    // Present and wait for it to complete.
    viz::FrameTimingDetails timing_details;
    if (!swap_to_presentation.is_zero()) {
      timing_details.presentation_feedback = gfx::PresentationFeedback(
          /*presentation_time=*/swap_time + swap_to_presentation,
          base::TimeDelta::FromMilliseconds(16), 0);
    }
    GetFrameSink()->NotifyDidPresentCompositorFrame(1, timing_details);
    presentation_run_loop.Run();
  }
};

TEST_F(NotifySwapTimesRenderWidgetUnittest, PresentationTimestampValid) {
  base::HistogramTester histograms;

  CompositeAndWaitForPresentation(base::TimeDelta::FromMilliseconds(2));

  EXPECT_THAT(histograms.GetAllSamples(
                  "PageLoad.Internal.Renderer.PresentationTime.Valid"),
              testing::ElementsAre(base::Bucket(true, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples(
          "PageLoad.Internal.Renderer.PresentationTime.DeltaFromSwapTime"),
      testing::ElementsAre(base::Bucket(2, 1)));
}

TEST_F(NotifySwapTimesRenderWidgetUnittest, PresentationTimestampInvalid) {
  base::HistogramTester histograms;

  CompositeAndWaitForPresentation(base::TimeDelta());

  EXPECT_THAT(histograms.GetAllSamples(
                  "PageLoad.Internal.Renderer.PresentationTime.Valid"),
              testing::ElementsAre(base::Bucket(false, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples(
          "PageLoad.Internal.Renderer.PresentationTime.DeltaFromSwapTime"),
      testing::IsEmpty());
}

TEST_F(NotifySwapTimesRenderWidgetUnittest,
       PresentationTimestampEarlierThanSwaptime) {
  base::HistogramTester histograms;

  CompositeAndWaitForPresentation(base::TimeDelta::FromMilliseconds(-2));

  EXPECT_THAT(histograms.GetAllSamples(
                  "PageLoad.Internal.Renderer.PresentationTime.Valid"),
              testing::ElementsAre(base::Bucket(false, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples(
          "PageLoad.Internal.Renderer.PresentationTime.DeltaFromSwapTime"),
      testing::IsEmpty());
}

}  // namespace content
