// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"

#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_group.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_frame_widget.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_image_transport_factory.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/mock_widget.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"

namespace content {
namespace {

const viz::LocalSurfaceId kArbitraryLocalSurfaceId(
    1,
    base::UnguessableToken::CreateForTesting(2, 3));

}  // namespace

class MockFrameConnector : public CrossProcessFrameConnector {
 public:
  explicit MockFrameConnector() : CrossProcessFrameConnector(nullptr) {}
  ~MockFrameConnector() override = default;

  void FirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {
    last_surface_info_ = surface_info;
  }

  void SetViewportIntersection(
      const gfx::Rect& viewport_intersection,
      const gfx::Rect& main_frame_intersection,
      const gfx::Rect& compositor_visible_rect,
      blink::mojom::FrameOcclusionState occlusion_state) {
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

class RenderWidgetHostViewChildFrameTest
    : public RenderViewHostImplTestHarness {
 public:
  RenderWidgetHostViewChildFrameTest() {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    process_host_ = std::make_unique<MockRenderProcessHost>(browser_context());
    site_instance_group_ =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            browser_context(), process_host_.get()));
    int32_t routing_id = process_host_->GetNextRoutingID();
    sink_ = &process_host_->sink();

    // Create a RenderWidgetHostImpl which will be associated with an
    // RenderWidgetHostViewChildFrame, to simulate what would be done for an
    // OOPIF.
    widget_host_ = RenderWidgetHostFactory::Create(
        /*frame_tree=*/&contents()->GetPrimaryFrameTree(), &delegate_,
        RenderWidgetHostImpl::DefaultFrameSinkId(*site_instance_group_,
                                                 routing_id),
        site_instance_group_->GetSafeRef(), routing_id,
        /*hidden=*/false, /*renderer_initiated_creation=*/false);

    widget_host_->BindWidgetInterfaces(
        mojo::AssociatedRemote<blink::mojom::WidgetHost>()
            .BindNewEndpointAndPassDedicatedReceiver(),
        widget_.GetNewRemote());
    widget_host_->BindFrameWidgetInterfaces(
        mojo::AssociatedRemote<blink::mojom::FrameWidgetHost>()
            .BindNewEndpointAndPassDedicatedReceiver(),
        TestRenderWidgetHost::CreateStubFrameWidgetRemote());

    display::ScreenInfo screen_info;
    screen_info.rect = gfx::Rect(1, 2, 3, 4);
    display::ScreenInfos screen_infos(screen_info);
    view_ = RenderWidgetHostViewChildFrame::Create(widget_host_.get(),
                                                   screen_infos);
    // Test we get the expected ScreenInfo before the FrameDelegate is set.
    EXPECT_EQ(screen_info, view_->GetScreenInfo());
    EXPECT_EQ(screen_infos, view_->GetScreenInfos());

    test_frame_connector_ = std::make_unique<MockFrameConnector>();
    test_frame_connector_->SetView(view_, false);
    view_->SetFrameConnector(test_frame_connector_.get());
  }

  void TearDown() override {
    sink_ = nullptr;
    if (view_) {
      RenderWidgetHostViewChildFrame* local_view = view_;
      view_ = nullptr;
      local_view->Destroy();
    }
    widget_host_.reset();
    site_instance_group_.reset();
    process_host_->Cleanup();
    test_frame_connector_.reset();

    process_host_.reset();

    RenderViewHostImplTestHarness::TearDown();
  }

  viz::SurfaceId GetSurfaceId() const {
    return view_->last_activated_surface_info_.id();
  }

  viz::LocalSurfaceId GetLocalSurfaceId() const {
    return GetSurfaceId().local_surface_id();
  }

 protected:
  std::unique_ptr<MockRenderProcessHost> process_host_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;
  raw_ptr<IPC::TestSink> sink_ = nullptr;
  MockRenderWidgetHostDelegate delegate_;
  MockWidget widget_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  std::unique_ptr<RenderWidgetHostImpl> widget_host_;
  raw_ptr<RenderWidgetHostViewChildFrame> view_ = nullptr;
  std::unique_ptr<MockFrameConnector> test_frame_connector_;
};

TEST_F(RenderWidgetHostViewChildFrameTest, VisibilityTest) {
  // Calling show and hide also needs to be propagated to child frame by the
  // |frame_connector_| which itself requires a |frame_proxy_in_parent_renderer|
  // (set to nullptr for MockFrameConnector). To avoid crashing the test
  // |frame_connector_| is to set to nullptr.
  view_->SetFrameConnector(nullptr);

  view_->Show();
  ASSERT_TRUE(view_->IsShowing());

  view_->Hide();
  ASSERT_FALSE(view_->IsShowing());

  // Restore the MockFrameConnector to avoid a crash during destruction.
  view_->SetFrameConnector(test_frame_connector_.get());
}

// Tests that the viewport intersection rect is dispatched to the RenderWidget
// whenever screen rects are updated.
TEST_F(RenderWidgetHostViewChildFrameTest, ViewportIntersectionUpdated) {
  gfx::Rect intersection_rect(5, 5, 100, 80);
  gfx::Rect main_frame_intersection(5, 10, 200, 200);
  blink::mojom::FrameOcclusionState occlusion_state =
      blink::mojom::FrameOcclusionState::kPossiblyOccluded;

  test_frame_connector_->SetViewportIntersection(
      intersection_rect, main_frame_intersection, intersection_rect,
      occlusion_state);

  MockRenderProcessHost* process =
      static_cast<MockRenderProcessHost*>(widget_host_->GetProcess());
  process->Init();

  mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> blink_frame_widget_host;
  auto blink_frame_widget_host_receiver =
      blink_frame_widget_host.BindNewEndpointAndPassDedicatedReceiver();
  mojo::AssociatedRemote<blink::mojom::FrameWidget> blink_frame_widget;
  auto blink_frame_widget_receiver =
      blink_frame_widget.BindNewEndpointAndPassDedicatedReceiver();
  widget_host_->BindFrameWidgetInterfaces(
      std::move(blink_frame_widget_host_receiver), blink_frame_widget.Unbind());
  FakeFrameWidget fake_frame_widget(std::move(blink_frame_widget_receiver));

  widget_host_->RendererWidgetCreated(/*for_frame_widget=*/true);
  base::RunLoop().RunUntilIdle();
  widget_.ClearScreenRects();
  base::RunLoop().RunUntilIdle();

  auto& intersection_state = fake_frame_widget.GetIntersectionState();
  EXPECT_EQ(gfx::Rect(intersection_rect),
            intersection_state->viewport_intersection);
  EXPECT_EQ(gfx::Rect(main_frame_intersection),
            intersection_state->main_frame_intersection);
  EXPECT_EQ(gfx::Rect(intersection_rect),
            intersection_state->compositor_visible_rect);
  EXPECT_EQ(static_cast<blink::mojom::FrameOcclusionState>(occlusion_state),
            intersection_state->occlusion_state);
}

// Tests that moving the child around does not affect the physical backing size.
TEST_F(RenderWidgetHostViewChildFrameTest, CompositorViewportPixelSize) {
  display::ScreenInfo screen_info;
  screen_info.device_scale_factor = 2.0f;

  blink::FrameVisualProperties visual_properties;
  visual_properties.screen_infos = display::ScreenInfos(screen_info);
  test_frame_connector_->SynchronizeVisualProperties(visual_properties, false);

  gfx::Size local_frame_size(1276, 410);
  test_frame_connector_->SetLocalFrameSize(local_frame_size);
  EXPECT_EQ(local_frame_size, view_->GetCompositorViewportPixelSize());

  gfx::Rect rect_in_parent_view(local_frame_size);
  rect_in_parent_view.set_origin(gfx::Point(230, 263));
  test_frame_connector_->SetRectInParentView(rect_in_parent_view);
  EXPECT_EQ(local_frame_size, view_->GetCompositorViewportPixelSize());
  EXPECT_EQ(gfx::Point(115, 131), view_->GetViewBounds().origin());
}

// Tests that SynchronizeVisualProperties is called only once and all the
// parameters change atomically.
TEST_F(RenderWidgetHostViewChildFrameTest,
       SynchronizeVisualPropertiesOncePerChange) {
  MockRenderProcessHost* process =
      static_cast<MockRenderProcessHost*>(widget_host_->GetProcess());
  process->Init();

  widget_host_->RendererWidgetCreated(/*for_frame_widget=*/true);

  constexpr gfx::Rect compositor_viewport_pixel_rect(100, 100);
  constexpr gfx::Rect rect_in_local_root(compositor_viewport_pixel_rect);
  viz::ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  viz::LocalSurfaceId local_surface_id = allocator.GetCurrentLocalSurfaceId();

  blink::FrameVisualProperties visual_properties;
  visual_properties.screen_infos = display::ScreenInfos(display::ScreenInfo());
  visual_properties.rect_in_local_root = rect_in_local_root;
  visual_properties.compositor_viewport = compositor_viewport_pixel_rect;
  visual_properties.local_frame_size = compositor_viewport_pixel_rect.size();
  visual_properties.capture_sequence_number = 123u;
  visual_properties.local_surface_id = local_surface_id;
  visual_properties.root_widget_viewport_segments.emplace_back(1, 2, 3, 4);

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
    EXPECT_EQ(rect_in_local_root.size(), sent_visual_properties.new_size);
    EXPECT_EQ(local_surface_id, sent_visual_properties.local_surface_id);
    EXPECT_EQ(123u, sent_visual_properties.capture_sequence_number);
    EXPECT_EQ(1u, sent_visual_properties.root_widget_viewport_segments.size());
    EXPECT_EQ(gfx::Rect(1, 2, 3, 4),
              sent_visual_properties.root_widget_viewport_segments[0]);
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
      scroll_begin, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultSource::kBrowser,
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

  view_->GestureEventAck(
      scroll_begin, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  // Scrolling in a child my reach its extent and no longer be consumed, however
  // scrolling is latched to the child so we do not bubble the update.
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultSource::kBrowser,
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
      scroll_begin, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  // The GSB was rejected, so the child view must not attempt to bubble the
  // remaining events of the scroll sequence.
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultSource::kBrowser,
                         blink::mojom::InputEventResultState::kIgnored);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            test_frame_connector_->GetAndResetLastBubbledEventType());

  test_frame_connector_->SetCanBubble(true);

  // When we have a new scroll gesture, the view may try bubbling again.
  view_->GestureEventAck(
      scroll_begin, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            test_frame_connector_->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultSource::kBrowser,
                         blink::mojom::InputEventResultState::kIgnored);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            test_frame_connector_->GetAndResetLastBubbledEventType());
}

}  // namespace content
