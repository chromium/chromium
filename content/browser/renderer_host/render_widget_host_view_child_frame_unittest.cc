// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"

#include <stdint.h>

#include <tuple>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "content/browser/compositor/test/test_image_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/frame_connector_delegate.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/frame_visual_properties.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fake_renderer_compositor_frame_sink.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"

namespace content {
namespace {

const viz::LocalSurfaceId kArbitraryLocalSurfaceId(
    1,
    base::UnguessableToken::Deserialize(2, 3));

}  // namespace

class MockFrameConnectorDelegate : public FrameConnectorDelegate {
 public:
  MockFrameConnectorDelegate(bool use_zoom_for_device_scale_factor)
      : FrameConnectorDelegate(use_zoom_for_device_scale_factor) {}
  ~MockFrameConnectorDelegate() override {}

  void FirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {
    last_surface_info_ = surface_info;
  }

  void SetViewportIntersection(const blink::WebRect& intersection,
                               const blink::WebRect& compositor_visible_rect,
                               blink::FrameOcclusionState occlusion_state) {
    intersection_state_.viewport_intersection = intersection;
    intersection_state_.compositor_visible_rect = compositor_visible_rect;
    intersection_state_.occlusion_state = occlusion_state;
  }

  RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override {
    return nullptr;
  }

  bool BubbleScrollEvent(const blink::WebGestureEvent& event) override {
    last_bubbled_event_type_ = event.GetType();
    return can_bubble_;
  }

  blink::WebInputEvent::Type GetAndResetLastBubbledEventType() {
    blink::WebInputEvent::Type last = last_bubbled_event_type_;
    last_bubbled_event_type_ = blink::WebInputEvent::kUndefined;
    return last;
  }

  void SetCanBubble(bool can_bubble) { can_bubble_ = can_bubble; }

  viz::SurfaceInfo last_surface_info_;

 private:
  blink::WebInputEvent::Type last_bubbled_event_type_ =
      blink::WebInputEvent::kUndefined;
  bool can_bubble_ = true;
};

class RenderWidgetHostViewChildFrameTest : public testing::Test {
 public:
  RenderWidgetHostViewChildFrameTest() {}

  void SetUp() override {
    SetUpEnvironment(false /* use_zoom_for_device_scale_factor */);
  }

  void SetUpEnvironment(bool use_zoom_for_device_scale_factor) {
    browser_context_.reset(new TestBrowserContext);

// ImageTransportFactory doesn't exist on Android.
#if !defined(OS_ANDROID)
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
#endif

    MockRenderProcessHost* process_host =
        new MockRenderProcessHost(browser_context_.get());

    int32_t routing_id = process_host->GetNextRoutingID();
    sink_ = &process_host->sink();

    mojo::PendingRemote<mojom::Widget> widget;
    widget_impl_ = std::make_unique<MockWidgetImpl>(
        widget.InitWithNewPipeAndPassReceiver());
    widget_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host, routing_id, std::move(widget),
        /*hidden=*/false, std::make_unique<FrameTokenMessageQueue>());
    view_ = RenderWidgetHostViewChildFrame::Create(widget_host_);

    test_frame_connector_ =
        new MockFrameConnectorDelegate(use_zoom_for_device_scale_factor);
    test_frame_connector_->SetView(view_);
    view_->SetFrameConnectorDelegate(test_frame_connector_);

    mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink;
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink_receiver =
        sink.InitWithNewPipeAndPassReceiver();
    renderer_compositor_frame_sink_ =
        std::make_unique<FakeRendererCompositorFrameSink>(
            std::move(sink), renderer_compositor_frame_sink_remote_
                                 .BindNewPipeAndPassReceiver());
    view_->DidCreateNewRendererCompositorFrameSink(
        renderer_compositor_frame_sink_remote_.get());
  }

  void TearDown() override {
    sink_ = nullptr;
    if (view_)
      view_->Destroy();
    delete widget_host_;
    delete test_frame_connector_;

    browser_context_.reset();

    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    browser_context_.release());
    base::RunLoop().RunUntilIdle();
#if !defined(OS_ANDROID)
    ImageTransportFactory::Terminate();
#endif
  }

  viz::SurfaceId GetSurfaceId() const {
    return view_->last_activated_surface_info_.id();
  }

  viz::LocalSurfaceId GetLocalSurfaceId() const {
    return GetSurfaceId().local_surface_id();
  }

 protected:
  BrowserTaskEnvironment task_environment_;

  std::unique_ptr<BrowserContext> browser_context_;
  IPC::TestSink* sink_ = nullptr;
  MockRenderWidgetHostDelegate delegate_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  std::unique_ptr<MockWidgetImpl> widget_impl_;
  RenderWidgetHostImpl* widget_host_;
  RenderWidgetHostViewChildFrame* view_;
  MockFrameConnectorDelegate* test_frame_connector_;
  std::unique_ptr<FakeRendererCompositorFrameSink>
      renderer_compositor_frame_sink_;

 private:
  mojo::Remote<viz::mojom::CompositorFrameSinkClient>
      renderer_compositor_frame_sink_remote_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrameTest);
};

viz::CompositorFrame CreateDelegatedFrame(float scale_factor,
                                          gfx::Size size,
                                          const gfx::Rect& damage) {
  viz::CompositorFrame frame;
  frame.metadata.device_scale_factor = scale_factor;
  frame.metadata.begin_frame_ack = viz::BeginFrameAck(0, 1, true);

  std::unique_ptr<viz::RenderPass> pass = viz::RenderPass::Create();
  pass->SetNew(1, gfx::Rect(size), damage, gfx::Transform());
  frame.render_pass_list.push_back(std::move(pass));
  return frame;
}

TEST_F(RenderWidgetHostViewChildFrameTest, VisibilityTest) {
  // Calling show and hide also needs to be propagated to child frame by the
  // |frame_connector_| which itself requires a |frame_proxy_in_parent_renderer|
  // (set to nullptr for MockFrameConnectorDelegate). To avoid crashing the test
  // |frame_connector_| is to set to nullptr.
  view_->SetFrameConnectorDelegate(nullptr);

  view_->Show();
  ASSERT_TRUE(view_->IsShowing());

  view_->Hide();
  ASSERT_FALSE(view_->IsShowing());
}

// Tests that the viewport intersection rect is dispatched to the RenderWidget
// whenever screen rects are updated.
TEST_F(RenderWidgetHostViewChildFrameTest, ViewportIntersectionUpdated) {
  blink::WebRect intersection_rect(5, 5, 100, 80);
  blink::FrameOcclusionState occlusion_state =
      blink::FrameOcclusionState::kPossiblyOccluded;
  test_frame_connector_->SetViewportIntersection(
      intersection_rect, intersection_rect, occlusion_state);

  MockRenderProcessHost* process =
      static_cast<MockRenderProcessHost*>(widget_host_->GetProcess());
  process->Init();

  widget_host_->Init();

  const IPC::Message* intersection_update =
      process->sink().GetUniqueMessageMatching(
          WidgetMsg_SetViewportIntersection::ID);
  ASSERT_TRUE(intersection_update);
  std::tuple<blink::ViewportIntersectionState> intersection_state;

  WidgetMsg_SetViewportIntersection::Read(intersection_update,
                                          &intersection_state);
  EXPECT_EQ(intersection_rect,
            std::get<0>(intersection_state).viewport_intersection);
  EXPECT_EQ(intersection_rect,
            std::get<0>(intersection_state).compositor_visible_rect);
  EXPECT_EQ(occlusion_state, std::get<0>(intersection_state).occlusion_state);
}

class RenderWidgetHostViewChildFrameZoomForDSFTest
    : public RenderWidgetHostViewChildFrameTest {
 public:
  RenderWidgetHostViewChildFrameZoomForDSFTest() {}

  void SetUp() override {
    SetUpEnvironment(true /* use_zoom_for_device_scale_factor */);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrameZoomForDSFTest);
};

// Tests that moving the child around does not affect the physical backing size.
TEST_F(RenderWidgetHostViewChildFrameZoomForDSFTest,
       CompositorViewportPixelSize) {
  ScreenInfo screen_info;
  screen_info.device_scale_factor = 2.0f;
  test_frame_connector_->SetScreenInfoForTesting(screen_info);

  gfx::Size local_frame_size(1276, 410);
  test_frame_connector_->SetLocalFrameSize(local_frame_size);
  EXPECT_EQ(local_frame_size, view_->GetCompositorViewportPixelSize());

  gfx::Rect screen_space_rect(local_frame_size);
  screen_space_rect.set_origin(gfx::Point(230, 263));
  test_frame_connector_->SetScreenSpaceRect(screen_space_rect);
  EXPECT_EQ(local_frame_size, view_->GetCompositorViewportPixelSize());
  EXPECT_EQ(gfx::Point(115, 131), view_->GetViewBounds().origin());
  EXPECT_EQ(gfx::Point(230, 263),
            test_frame_connector_->screen_space_rect_in_pixels().origin());
}

// Tests that SynchronizeVisualProperties is called only once and all the
// parameters change atomically.
TEST_F(RenderWidgetHostViewChildFrameTest,
       SynchronizeVisualPropertiesOncePerChange) {
  MockRenderProcessHost* process =
      static_cast<MockRenderProcessHost*>(widget_host_->GetProcess());
  process->Init();

  widget_host_->Init();

  constexpr gfx::Rect compositor_viewport_pixel_rect(100, 100);
  constexpr gfx::Rect screen_space_rect(compositor_viewport_pixel_rect);
  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  viz::LocalSurfaceIdAllocation local_surface_id_allocation =
      allocator.GetCurrentLocalSurfaceIdAllocation();
  constexpr viz::FrameSinkId frame_sink_id(1, 1);

  FrameVisualProperties visual_properties;
  visual_properties.screen_space_rect = screen_space_rect;
  visual_properties.compositor_viewport = compositor_viewport_pixel_rect;
  visual_properties.local_frame_size = compositor_viewport_pixel_rect.size();
  visual_properties.capture_sequence_number = 123u;
  visual_properties.local_surface_id_allocation = local_surface_id_allocation;

  sink_->ClearMessages();
  test_frame_connector_->SynchronizeVisualProperties(frame_sink_id,
                                                     visual_properties);

  // Update to the renderer.
  ASSERT_EQ(1u, sink_->message_count());
  {
    const IPC::Message* msg = sink_->GetMessageAt(0);
    ASSERT_EQ(WidgetMsg_UpdateVisualProperties::ID, msg->type());
    WidgetMsg_UpdateVisualProperties::Param params;
    WidgetMsg_UpdateVisualProperties::Read(msg, &params);
    VisualProperties sent_visual_properties = std::get<0>(params);

    EXPECT_EQ(compositor_viewport_pixel_rect,
              sent_visual_properties.compositor_viewport_pixel_rect);
    EXPECT_EQ(screen_space_rect.size(), sent_visual_properties.new_size);
    EXPECT_EQ(local_surface_id_allocation,
              sent_visual_properties.local_surface_id_allocation);
    EXPECT_EQ(123u, sent_visual_properties.capture_sequence_number);
  }
}

// Test that when we have a gesture scroll sequence that is not consumed by the
// child, the events are bubbled so that the parent may consume them.
TEST_F(RenderWidgetHostViewChildFrameTest, UncomsumedGestureScrollBubbled) {
  blink::WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(
          0.f, 10.f, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          0.f, 10.f, 0, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      blink::WebInputEvent::kGestureScrollEnd,
      blink::WebGestureDevice::kTouchscreen);

  view_->GestureEventAck(scroll_begin,
                         INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_update,
                         INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollUpdate,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end, INPUT_EVENT_ACK_STATE_IGNORED);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd,
            test_frame_connector_->GetAndResetLastBubbledEventType());
}

// Test that when we have a gesture scroll sequence that is consumed by the
// child, the events are not bubbled to the parent.
TEST_F(RenderWidgetHostViewChildFrameTest, ConsumedGestureScrollNotBubbled) {
  blink::WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(
          0.f, 10.f, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          0.f, 10.f, 0, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      blink::WebInputEvent::kGestureScrollEnd,
      blink::WebGestureDevice::kTouchscreen);

  view_->GestureEventAck(scroll_begin, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(blink::WebInputEvent::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_update, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(blink::WebInputEvent::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  // Scrolling in a child my reach its extent and no longer be consumed, however
  // scrolling is latched to the child so we do not bubble the update.
  view_->GestureEventAck(scroll_update,
                         INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(blink::WebInputEvent::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  view_->GestureEventAck(scroll_end, INPUT_EVENT_ACK_STATE_IGNORED);
  EXPECT_EQ(blink::WebInputEvent::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());
}

// Test that the child does not continue to attempt to bubble scroll events if
// bubbling has failed for the current scroll gesture.
TEST_F(RenderWidgetHostViewChildFrameTest,
       DoNotBubbleRemainingEventsOfRejectedScrollGesture) {
  blink::WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(
          0.f, 10.f, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          0.f, 10.f, 0, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      blink::WebInputEvent::kGestureScrollEnd,
      blink::WebGestureDevice::kTouchscreen);

  test_frame_connector_->SetCanBubble(false);

  view_->GestureEventAck(scroll_begin,
                         INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  // The GSB was rejected, so the child view must not attempt to bubble the
  // remaining events of the scroll sequence.
  view_->GestureEventAck(scroll_update,
                         INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(blink::WebInputEvent::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end, INPUT_EVENT_ACK_STATE_IGNORED);
  EXPECT_EQ(blink::WebInputEvent::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  test_frame_connector_->SetCanBubble(true);

  // When we have a new scroll gesture, the view may try bubbling again.
  view_->GestureEventAck(scroll_begin,
                         INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_update,
                         INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollUpdate,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end, INPUT_EVENT_ACK_STATE_IGNORED);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd,
            test_frame_connector_->GetAndResetLastBubbledEventType());
}

}  // namespace content
