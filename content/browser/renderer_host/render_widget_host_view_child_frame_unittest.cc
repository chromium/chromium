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
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "content/browser/compositor/test/test_image_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/frame_connector_delegate.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/mock_widget.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
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

  void SetViewportIntersection(const blink::WebRect& viewport_intersection,
                               const blink::WebRect& main_frame_intersection,
                               const blink::WebRect& compositor_visible_rect,
                               blink::FrameOcclusionState occlusion_state) {
    intersection_state_.viewport_intersection = viewport_intersection;
    intersection_state_.main_frame_intersection = main_frame_intersection;
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
    last_bubbled_event_type_ = blink::WebInputEvent::Type::kUndefined;
    return last;
  }

  void SetCanBubble(bool can_bubble) { can_bubble_ = can_bubble; }

  viz::SurfaceInfo last_surface_info_;

 private:
  blink::WebInputEvent::Type last_bubbled_event_type_ =
      blink::WebInputEvent::Type::kUndefined;
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

    auto* process_host = new MockRenderProcessHost(browser_context_.get());

    agent_scheduling_group_host_ =
        std::make_unique<AgentSchedulingGroupHost>(*process_host);
    int32_t routing_id = process_host->GetNextRoutingID();
    sink_ = &process_host->sink();

    widget_host_ = new RenderWidgetHostImpl(
        &delegate_, *agent_scheduling_group_host_, routing_id,
        /*hidden=*/false, std::make_unique<FrameTokenMessageQueue>());

    mojo::AssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
    widget_host_->BindWidgetInterfaces(
        blink_widget_host.BindNewEndpointAndPassDedicatedReceiver(),
        widget_.GetNewRemote());

    mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> frame_widget_host;
    mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget;
    auto frame_widget_receiver =
        frame_widget.BindNewEndpointAndPassDedicatedReceiver();
    widget_host_->BindFrameWidgetInterfaces(
        frame_widget_host.BindNewEndpointAndPassDedicatedReceiver(),
        frame_widget.Unbind());

    blink::ScreenInfo screen_info;
    screen_info.rect = gfx::Rect(1, 2, 3, 4);
    view_ = RenderWidgetHostViewChildFrame::Create(widget_host_, screen_info);
    // Test we get the expected ScreenInfo before the FrameDelegate is set.
    blink::ScreenInfo actual_screen_info;
    view_->GetScreenInfo(&actual_screen_info);
    EXPECT_EQ(screen_info, actual_screen_info);

    test_frame_connector_ =
        new MockFrameConnectorDelegate(use_zoom_for_device_scale_factor);
    test_frame_connector_->SetView(view_);
    view_->SetFrameConnectorDelegate(test_frame_connector_);
  }

  void TearDown() override {
    sink_ = nullptr;
    if (view_)
      view_->Destroy();
    delete widget_host_;
    agent_scheduling_group_host_ = nullptr;
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
  std::unique_ptr<AgentSchedulingGroupHost> agent_scheduling_group_host_;
  IPC::TestSink* sink_ = nullptr;
  MockRenderWidgetHostDelegate delegate_;
  MockWidget widget_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  RenderWidgetHostImpl* widget_host_;
  RenderWidgetHostViewChildFrame* view_;
  MockFrameConnectorDelegate* test_frame_connector_;
};

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
  blink::WebRect main_frame_intersection(5, 10, 200, 200);
  blink::FrameOcclusionState occlusion_state =
      blink::FrameOcclusionState::kPossiblyOccluded;
  test_frame_connector_->SetViewportIntersection(
      intersection_rect, main_frame_intersection, intersection_rect,
      occlusion_state);

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
  EXPECT_EQ(main_frame_intersection,
            std::get<0>(intersection_state).main_frame_intersection);
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
  blink::ScreenInfo screen_info;
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
  viz::LocalSurfaceId local_surface_id = allocator.GetCurrentLocalSurfaceId();

  blink::FrameVisualProperties visual_properties;
  visual_properties.screen_space_rect = screen_space_rect;
  visual_properties.compositor_viewport = compositor_viewport_pixel_rect;
  visual_properties.local_frame_size = compositor_viewport_pixel_rect.size();
  visual_properties.capture_sequence_number = 123u;
  visual_properties.local_surface_id = local_surface_id;
  visual_properties.root_widget_window_segments.emplace_back(1, 2, 3, 4);

  base::RunLoop().RunUntilIdle();
  widget_.ClearVisualProperties();
  test_frame_connector_->SynchronizeVisualProperties(visual_properties);

  // Update to the renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_.ReceivedVisualProperties().size());
  {
    blink::VisualProperties sent_visual_properties =
        widget_.ReceivedVisualProperties().at(0);

    EXPECT_EQ(compositor_viewport_pixel_rect,
              sent_visual_properties.compositor_viewport_pixel_rect);
    EXPECT_EQ(screen_space_rect.size(), sent_visual_properties.new_size);
    EXPECT_EQ(local_surface_id, sent_visual_properties.local_surface_id);
    EXPECT_EQ(123u, sent_visual_properties.capture_sequence_number);
    EXPECT_EQ(1u, sent_visual_properties.root_widget_window_segments.size());
    EXPECT_EQ(gfx::Rect(1, 2, 3, 4),
              sent_visual_properties.root_widget_window_segments[0]);
  }
}

// Test that when we have a gesture scroll sequence that is not consumed by the
// child, the events are bubbled so that the parent may consume them.
TEST_F(RenderWidgetHostViewChildFrameTest, UncomsumedGestureScrollBubbled) {
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          0.f, 10.f, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_update =
      blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          0.f, 10.f, 0, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_end =
      blink::SyntheticWebGestureEventBuilder::Build(
          blink::WebInputEvent::Type::kGestureScrollEnd,
          blink::WebGestureDevice::kTouchscreen);

  view_->GestureEventAck(
      scroll_begin, blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultState::kIgnored);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            test_frame_connector_->GetAndResetLastBubbledEventType());
}

// Test that when we have a gesture scroll sequence that is consumed by the
// child, the events are not bubbled to the parent.
TEST_F(RenderWidgetHostViewChildFrameTest, ConsumedGestureScrollNotBubbled) {
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          0.f, 10.f, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_update =
      blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          0.f, 10.f, 0, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_end =
      blink::SyntheticWebGestureEventBuilder::Build(
          blink::WebInputEvent::Type::kGestureScrollEnd,
          blink::WebGestureDevice::kTouchscreen);

  view_->GestureEventAck(scroll_begin,
                         blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_update,
                         blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  // Scrolling in a child my reach its extent and no longer be consumed, however
  // scrolling is latched to the child so we do not bubble the update.
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultState::kIgnored);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());
}

// Test that the child does not continue to attempt to bubble scroll events if
// bubbling has failed for the current scroll gesture.
TEST_F(RenderWidgetHostViewChildFrameTest,
       DoNotBubbleRemainingEventsOfRejectedScrollGesture) {
  blink::WebGestureEvent scroll_begin =
      blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
          0.f, 10.f, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_update =
      blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          0.f, 10.f, 0, blink::WebGestureDevice::kTouchscreen);
  blink::WebGestureEvent scroll_end =
      blink::SyntheticWebGestureEventBuilder::Build(
          blink::WebInputEvent::Type::kGestureScrollEnd,
          blink::WebGestureDevice::kTouchscreen);

  test_frame_connector_->SetCanBubble(false);

  view_->GestureEventAck(
      scroll_begin, blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  // The GSB was rejected, so the child view must not attempt to bubble the
  // remaining events of the scroll sequence.
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultState::kIgnored);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  test_frame_connector_->SetCanBubble(true);

  // When we have a new scroll gesture, the view may try bubbling again.
  view_->GestureEventAck(
      scroll_begin, blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultState::kIgnored);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            test_frame_connector_->GetAndResetLastBubbledEventType());
}

}  // namespace content
