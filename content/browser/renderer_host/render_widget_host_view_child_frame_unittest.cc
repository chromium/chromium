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
#include "cc/trees/render_frame_metadata.h"
#include "components/input/child_frame_input_helper.h"
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
#include "ui/gfx/selection_bound.h"

#if BUILDFLAG(IS_WIN)
#include "components/stylus_handwriting/win/features.h"
#include "content/browser/renderer_host/input/mock_tfhandwriting.h"
#include "content/browser/renderer_host/input/stylus_handwriting_callback_sink_win.h"
#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"
#include "content/browser/renderer_host/input/stylus_handwriting_win_test_helper.h"

using testing::_;
using testing::Return;
#endif  // BUILDFLAG(IS_WIN)

namespace content {
namespace {

const viz::LocalSurfaceId kArbitraryLocalSurfaceId(
    1,
    base::UnguessableToken::CreateForTesting(2, 3));

}  // namespace

class MockChildFrameInputHelper : public input::ChildFrameInputHelper {
 public:
  explicit MockChildFrameInputHelper(input::RenderWidgetHostViewInput* view,
                                     Delegate* delegate)
      : ChildFrameInputHelper(view, delegate) {}
  ~MockChildFrameInputHelper() override = default;

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

 private:
  blink::WebInputEvent::Type last_bubbled_event_type_ =
      blink::WebInputEvent::Type::kUndefined;
  bool can_bubble_ = true;
};

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

  void SetRootRenderWidgetHostView(RenderWidgetHostViewBase* root) {
    root_host_view_ = root;
  }

  void SetParentRenderWidgetHostView(RenderWidgetHostViewBase* parent) {
    parent_host_view_ = parent;
  }

  RenderWidgetHostViewBase* GetRootRenderWidgetHostView() override {
    return root_host_view_;
  }

  RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override {
    return parent_host_view_;
  }

  viz::SurfaceInfo last_surface_info_;
  raw_ptr<RenderWidgetHostViewBase> root_host_view_ = nullptr;
  raw_ptr<RenderWidgetHostViewBase> parent_host_view_ = nullptr;
};

class MockParentRenderWidgetHostView : public TestRenderWidgetHostView {
 public:
  explicit MockParentRenderWidgetHostView(RenderWidgetHost* rwh)
      : TestRenderWidgetHostView(rwh) {}

  gfx::Rect GetViewBounds() override { return bounds_; }
  gfx::Rect GetViewBoundsWithoutTransform() override {
    return bounds_without_transform_;
  }

  void SetBoundsWithAndWithoutTransform(const gfx::Rect& bounds,
                                        const gfx::Rect& bounds_without) {
    bounds_ = bounds;
    bounds_without_transform_ = bounds_without;
  }

 private:
  gfx::Rect bounds_;
  gfx::Rect bounds_without_transform_;
};

class TestTouchSelectionControllerClientManager
    : public TouchSelectionControllerClientManager {
 public:
  TestTouchSelectionControllerClientManager() = default;
  ~TestTouchSelectionControllerClientManager() override = default;

  void DidStopFlinging() override {}
  void OnSwipeToMoveCursorBegin() override {}
  void OnSwipeToMoveCursorEnd() override {}
  void OnClientHitTestRegionUpdated(
      ui::TouchSelectionControllerClient* client) override {}
  void UpdateClientSelectionBounds(
      const gfx::SelectionBound& start,
      const gfx::SelectionBound& end,
      ui::TouchSelectionControllerClient* client,
      ui::TouchSelectionMenuClient* menu_client) override {
    last_selection_start_ = start;
    last_selection_end_ = end;
  }
  void InvalidateClient(ui::TouchSelectionControllerClient* client) override {}
  ui::TouchSelectionController* GetTouchSelectionController() override {
    return nullptr;
  }
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  const gfx::SelectionBound& last_selection_start() const {
    return last_selection_start_;
  }
  const gfx::SelectionBound& last_selection_end() const {
    return last_selection_end_;
  }

 private:
  gfx::SelectionBound last_selection_start_;
  gfx::SelectionBound last_selection_end_;
};

class MockRenderWidgetHostView : public TestRenderWidgetHostView {
 public:
  explicit MockRenderWidgetHostView(RenderWidgetHost* rwh)
      : TestRenderWidgetHostView(rwh) {}
  ~MockRenderWidgetHostView() override = default;

  TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() override {
    return &selection_manager_;
  }

  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      input::RenderWidgetHostViewInput* target_view,
      gfx::PointF* transformed_point) override {
    *transformed_point = point;
    return true;
  }

  bool TransformPointToLocalCoordSpace(
      const gfx::PointF& point,
      const viz::FrameSinkId& original_frame_sink_id,
      gfx::PointF* transformed_point) override {
    *transformed_point = point;
    return true;
  }

#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void,
              ReportScrollJankStats,
              (uint32_t total_frames, uint32_t janky_frames),
              (override));
#elif BUILDFLAG(IS_MAC)
  MOCK_METHOD(void,
              ShowSharePicker,
              (const std::string& title,
               const std::string& text,
               const GURL& url,
               const std::vector<std::string>& file_paths,
               blink::mojom::ShareService::ShareCallback callback),
              (override));
#endif

  TestTouchSelectionControllerClientManager* selection_manager() {
    return &selection_manager_;
  }

 private:
  TestTouchSelectionControllerClientManager selection_manager_;
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
    // Set MockChildFrameInputHelper as `input_helper_` member variable.
    view_->SetInputHelperForTesting(
        std::make_unique<MockChildFrameInputHelper>(view_, nullptr));

    // Test we get the expected ScreenInfo before the FrameDelegate is set.
    EXPECT_EQ(screen_info, view_->GetScreenInfo());
    EXPECT_EQ(screen_infos, view_->GetScreenInfos());

    test_frame_connector_ = std::make_unique<MockFrameConnector>();
    test_frame_connector_->SetView(view_, false);
    view_->SetFrameConnector(test_frame_connector_.get());
  }

  void TearDown() override {
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

  MockChildFrameInputHelper* GetMockInputHelper() {
    return static_cast<MockChildFrameInputHelper*>(view_->input_helper_.get());
  }

  void SetParentFrameSinkId(const viz::FrameSinkId& parent_frame_sink_id) {
    view_->SetParentFrameSinkId(parent_frame_sink_id);
  }

  bool ConnectionHasRegisteredHierarchy() const {
    return view_->has_frame_sink_hierarchy_registered_;
  }

  viz::FrameSinkId GetParentFrameSinkId() const {
    return view_->parent_frame_sink_id_;
  }

 protected:
  std::unique_ptr<MockRenderProcessHost> process_host_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;
  MockRenderWidgetHostDelegate delegate_;
  MockWidget widget_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  std::unique_ptr<RenderWidgetHostImpl> widget_host_;
  raw_ptr<RenderWidgetHostViewChildFrame> view_ = nullptr;
  std::unique_ptr<MockFrameConnector> test_frame_connector_;
};

#if BUILDFLAG(IS_MAC)
TEST_F(RenderWidgetHostViewChildFrameTest, ShowSharePickerFromChildFrame) {
  // Set up a mock root view for the sake of accepting the share picker call.
  auto root_view =
      std::make_unique<testing::NiceMock<MockRenderWidgetHostView>>(
          widget_host_.get());
  RenderWidgetHostViewChildFrame* child_view =
      RenderWidgetHostViewChildFrame::Create(widget_host_.get(),
                                             display::ScreenInfos());
  std::unique_ptr<MockFrameConnector> connector =
      std::make_unique<MockFrameConnector>();
  connector->SetView(child_view, false);
  connector->SetRootRenderWidgetHostView(root_view.get());
  child_view->SetFrameConnector(connector.get());

  EXPECT_CALL(*root_view, ShowSharePicker(testing::_, testing::_, testing::_,
                                          testing::_, testing::_))
      .WillOnce([&](const std::string&, const std::string&, const GURL&,
                    const std::vector<std::string>&,
                    blink::mojom::ShareService::ShareCallback cb) {
        std::move(cb).Run(blink::mojom::ShareError::OK);
      });

  base::RunLoop run_loop;
  child_view->ShowSharePicker(
      "title", "text", GURL("http://example.com"), {},
      base::BindOnce([](blink::mojom::ShareError error) {
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
  child_view->Destroy();
  connector->SetRootRenderWidgetHostView(nullptr);
}
#endif  // BUILDFLAG(IS_MAC)

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

TEST_F(RenderWidgetHostViewChildFrameTest, GetViewBoundsWithoutTransform) {
  testing::NiceMock<MockParentRenderWidgetHostView> parent_view(
      widget_host_.get());
  test_frame_connector_->SetParentRenderWidgetHostView(&parent_view);

  gfx::Size local_frame_size(800, 600);
  test_frame_connector_->SetLocalFrameSize(local_frame_size);

  gfx::Rect rect_in_parent_view(local_frame_size);
  rect_in_parent_view.set_origin(gfx::Point(0, 0));
  test_frame_connector_->SetRectInParentView(rect_in_parent_view);

  parent_view.SetBoundsWithAndWithoutTransform(gfx::Rect(50, 50, 300, 200),
                                               gfx::Rect(0, 0, 300, 200));

  EXPECT_EQ(gfx::Point(50, 50), view_->GetViewBounds().origin());
  EXPECT_EQ(gfx::Point(0, 0), view_->GetViewBoundsWithoutTransform().origin());

  test_frame_connector_->SetParentRenderWidgetHostView(nullptr);
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
    EXPECT_EQ(rect_in_local_root.size(),
              sent_visual_properties.new_size_device_px);
    EXPECT_EQ(local_surface_id, sent_visual_properties.local_surface_id);
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
            GetMockInputHelper()->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultSource::kBrowser,
                         blink::mojom::InputEventResultState::kIgnored);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());
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
            GetMockInputHelper()->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());

  // Scrolling in a child my reach its extent and no longer be consumed, however
  // scrolling is latched to the child so we do not bubble the update.
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());

  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultSource::kBrowser,
                         blink::mojom::InputEventResultState::kIgnored);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());
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

  GetMockInputHelper()->SetCanBubble(false);

  view_->GestureEventAck(
      scroll_begin, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());

  // The GSB was rejected, so the child view must not attempt to bubble the
  // remaining events of the scroll sequence.
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultSource::kBrowser,
                         blink::mojom::InputEventResultState::kIgnored);
  EXPECT_EQ(blink::WebInputEvent::Type::kUndefined,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());

  GetMockInputHelper()->SetCanBubble(true);

  // When we have a new scroll gesture, the view may try bubbling again.
  view_->GestureEventAck(
      scroll_begin, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollBegin,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(
      scroll_update, blink::mojom::InputEventResultSource::kCompositorThread,
      blink::mojom::InputEventResultState::kNoConsumerExists);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollUpdate,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());
  view_->GestureEventAck(scroll_end,
                         blink::mojom::InputEventResultSource::kBrowser,
                         blink::mojom::InputEventResultState::kIgnored);
  EXPECT_EQ(blink::WebInputEvent::Type::kGestureScrollEnd,
            GetMockInputHelper()->GetAndResetLastBubbledEventType());
}

TEST_F(RenderWidgetHostViewChildFrameTest,
       InvalidateLocalSurfaceIdAndAllocationGroup) {
  // Calling this method on a child frame should be a no-op and not crash.
  view_->InvalidateLocalSurfaceIdAndAllocationGroup();
}

TEST_F(RenderWidgetHostViewChildFrameTest,
       SetParentFrameSinkIdFailedRegistration) {
  // Create a parent FrameSinkId. We do NOT register it in HostFrameSinkManager,
  // so hierarchy registration should fail.
  viz::FrameSinkId parent_frame_sink_id(99, 99);

  // Initially, hierarchy should not be registered.
  EXPECT_FALSE(ConnectionHasRegisteredHierarchy());

  // Call SetParentFrameSinkId with the unregistered parent ID.
  // This should attempt to register but fail, leaving the flag false.
  SetParentFrameSinkId(parent_frame_sink_id);
  EXPECT_FALSE(ConnectionHasRegisteredHierarchy());
  EXPECT_EQ(parent_frame_sink_id, GetParentFrameSinkId());

  // Call SetParentFrameSinkId again with an invalid ID.
  // This should attempt to unregister the previous parent.
  // If we didn't track the registration state, this would call
  // UnregisterFrameSinkHierarchy and crash because the parent was never
  // registered.
  // With the fix, it should notice registration failed and NOT call
  // UnregisterFrameSinkHierarchy.
  SetParentFrameSinkId(viz::FrameSinkId());
  EXPECT_FALSE(ConnectionHasRegisteredHierarchy());
  EXPECT_FALSE(GetParentFrameSinkId().is_valid());
}

#if BUILDFLAG(IS_WIN)
// Test fixture for verifying OnFocusFailed behavior in
// RenderWidgetHostViewChildFrame for stylus handwriting scenarios.
class StylusHandwritingOnFocusFailedChildFrameTest
    : public RenderWidgetHostViewChildFrameTest {
 public:
  void SetUp() override {
    RenderWidgetHostViewChildFrameTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        stylus_handwriting::win::kStylusHandwritingWin);
    stylus_handwriting_win_test_helper_.SetUpDefaultMockInfrastructure();
    stylus_handwriting_win_test_helper_
        .DefaultMockRequestHandwritingForPointerMethod();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  StylusHandwritingWinTestHelper stylus_handwriting_win_test_helper_;
  Microsoft::WRL::ComPtr<MockTfFocusHandwritingTargetArgsImpl> mock_focus_args_;
};

// Verify that destroying the child frame view that initiated an in-flight
// handwriting session calls OnFocusFailed.
TEST_F(StylusHandwritingOnFocusFailedChildFrameTest,
       InitiatingViewDestructionFailsFocus) {
  mock_focus_args_ =
      stylus_handwriting_win_test_helper_.SetUpWaitingForFocusResult(
          view_->GetWeakPtr());

  EXPECT_CALL(*mock_focus_args_.Get(), SetResponse(::TF_NO_HANDWRITING_TARGET))
      .Times(1);

  RenderWidgetHostViewChildFrame* local_view = view_;
  view_ = nullptr;
  local_view->Destroy();

  EXPECT_FALSE(
      StylusHandwritingControllerWin::GetInstance()->IsWaitingForFocusResult());
}

// Verify that destroying a child frame view that did NOT initiate the in-flight
// handwriting session leaves the session untouched. Regression test for a
// non-initiating ~RenderWidgetHostViewChildFrame ending an unrelated session.
TEST_F(StylusHandwritingOnFocusFailedChildFrameTest,
       NonInitiatingViewDestructionPreservesFocus) {
  // Create a separate child frame view to initiate the handwriting session.
  auto root_view =
      std::make_unique<testing::NiceMock<MockRenderWidgetHostView>>(
          widget_host_.get());
  RenderWidgetHostViewChildFrame* initiating_view =
      RenderWidgetHostViewChildFrame::Create(widget_host_.get(),
                                             display::ScreenInfos());
  auto connector = std::make_unique<MockFrameConnector>();
  connector->SetView(initiating_view, false);
  connector->SetRootRenderWidgetHostView(root_view.get());
  initiating_view->SetFrameConnector(connector.get());

  mock_focus_args_ =
      stylus_handwriting_win_test_helper_.SetUpWaitingForFocusResult(
          initiating_view->GetWeakPtr());

  EXPECT_CALL(*mock_focus_args_.Get(), SetResponse(_)).Times(0);

  // Destroy the unrelated fixture view; it did not initiate the session.
  RenderWidgetHostViewChildFrame* local_view = view_;
  view_ = nullptr;
  local_view->Destroy();

  EXPECT_TRUE(
      StylusHandwritingControllerWin::GetInstance()->IsWaitingForFocusResult());

  // Verify now; the session is resolved when `initiating_view` is destroyed
  // below, which would otherwise trip the Times(0) expectation.
  testing::Mock::VerifyAndClearExpectations(mock_focus_args_.Get());

  initiating_view->Destroy();
  connector->SetRootRenderWidgetHostView(nullptr);
}

// Verify that destroying the child frame view that initiated an in-flight
// session after the session started but before TSF delivers
// FocusHandwritingTarget causes the subsequent FocusHandwritingTarget to be
// declined immediately rather than forwarded, so the Shell Handwriting API is
// not left awaiting a response that can never arrive.
TEST_F(StylusHandwritingOnFocusFailedChildFrameTest,
       InitiatingViewDestroyedBeforeTargetDeclinesFocus) {
  // Start the session but do not deliver FocusHandwritingTarget yet.
  mock_focus_args_ =
      stylus_handwriting_win_test_helper_.SetUpStartedStylusWriting(
          view_->GetWeakPtr());
  auto* controller = StylusHandwritingControllerWin::GetInstance();
  ASSERT_FALSE(controller->IsWaitingForFocusResult());

  // The target must be declined, not forwarded to the renderer.
  EXPECT_CALL(*mock_focus_args_.Get(), SetResponse(::TF_NO_HANDWRITING_TARGET))
      .Times(1);

  // Destroying the initiating view before the target arrives arms the decline.
  RenderWidgetHostViewChildFrame* local_view = view_;
  view_ = nullptr;
  local_view->Destroy();

  // When TSF finally delivers the target, it is declined synchronously (S_OK,
  // not TF_S_ASYNC) and no focus result remains pending.
  auto sink = controller->GetCallbackSinkForTesting();
  ASSERT_TRUE(sink);
  EXPECT_EQ(S_OK, sink->FocusHandwritingTarget(mock_focus_args_.Get()));
  EXPECT_FALSE(controller->IsWaitingForFocusResult());
}

// Verify that OnEditElementFocusedForStylusWriting calls OnFocusFailed when
// the child frame has no root view.
TEST_F(StylusHandwritingOnFocusFailedChildFrameTest, NoRootView) {
  mock_focus_args_ =
      stylus_handwriting_win_test_helper_.SetUpWaitingForFocusResult(
          view_->GetWeakPtr());

  // Remove the root view from the frame connector so GetRootView() returns
  // nullptr.
  test_frame_connector_->SetRootRenderWidgetHostView(nullptr);

  EXPECT_CALL(*mock_focus_args_.Get(), SetResponse(::TF_NO_HANDWRITING_TARGET))
      .Times(1);

  view_->OnEditElementFocusedForStylusWriting(nullptr);

  EXPECT_FALSE(
      StylusHandwritingControllerWin::GetInstance()->IsWaitingForFocusResult());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(RenderWidgetHostViewChildFrameTest, SelectionBoundsClampedToViewBounds) {
  auto root_view =
      std::make_unique<testing::NiceMock<MockRenderWidgetHostView>>(
          widget_host_.get());
  RenderWidgetHostViewChildFrame* child_view =
      RenderWidgetHostViewChildFrame::Create(widget_host_.get(),
                                             display::ScreenInfos());
  std::unique_ptr<MockFrameConnector> connector =
      std::make_unique<MockFrameConnector>();
  connector->SetRootRenderWidgetHostView(root_view.get());
  connector->SetView(child_view, false);

  // Set local frame bounds and size DIP to 100x100.
  connector->SetRectInParentView(gfx::Rect(0, 0, 100, 100));
  connector->SetLocalFrameSize(gfx::Size(100, 100));
  child_view->SetSize(gfx::Size(100, 100));

  // Report selection bounds with spoofed coordinates far outside [0, 0, 100,
  // 100].
  cc::RenderFrameMetadata metadata;
  metadata.selection.start.set_type(gfx::SelectionBound::LEFT);
  metadata.selection.start.set_visible(true);
  metadata.selection.start.SetEdge(gfx::PointF(-50.0f, -50.0f),
                                   gfx::PointF(-50.0f, -10.0f));
  metadata.selection.start.SetVisibleEdge(gfx::PointF(-50.0f, -50.0f),
                                          gfx::PointF(-50.0f, -10.0f));

  metadata.selection.end.set_type(gfx::SelectionBound::RIGHT);
  metadata.selection.end.set_visible(true);
  metadata.selection.end.SetEdge(gfx::PointF(200.0f, 200.0f),
                                 gfx::PointF(200.0f, 250.0f));
  metadata.selection.end.SetVisibleEdge(gfx::PointF(200.0f, 200.0f),
                                        gfx::PointF(200.0f, 250.0f));

  widget_host_->render_frame_metadata_provider()
      ->SetLastRenderFrameMetadataForTest(metadata);
  child_view->OnRenderFrameMetadataChangedAfterActivation(base::TimeTicks());

  // Verify that start and end bounds sent to the manager were clamped to [0,
  // 100].
  gfx::SelectionBound start =
      root_view->selection_manager()->last_selection_start();
  gfx::SelectionBound end =
      root_view->selection_manager()->last_selection_end();

  EXPECT_EQ(gfx::PointF(0.0f, 0.0f), start.edge_start());
  EXPECT_EQ(gfx::PointF(0.0f, 0.0f), start.edge_end());
  EXPECT_EQ(gfx::PointF(100.0f, 100.0f), end.edge_start());
  EXPECT_EQ(gfx::PointF(100.0f, 100.0f), end.edge_end());

  child_view->Destroy();
  connector->SetRootRenderWidgetHostView(nullptr);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(RenderWidgetHostViewChildFrameTest, ReportScrollJankStats) {
  auto root_view =
      std::make_unique<testing::NiceMock<MockRenderWidgetHostView>>(
          widget_host_.get());
  RenderWidgetHostViewChildFrame* child_view =
      RenderWidgetHostViewChildFrame::Create(widget_host_.get(),
                                             display::ScreenInfos());
  std::unique_ptr<MockFrameConnector> connector =
      std::make_unique<MockFrameConnector>();
  connector->SetView(child_view, false);
  connector->SetRootRenderWidgetHostView(root_view.get());
  child_view->SetFrameConnector(connector.get());

  {
    EXPECT_CALL(*root_view, ReportScrollJankStats(/*total_frames=*/100,
                                                  /*janky_frames=*/10))
        .Times(1);
    child_view->OnReportScrollJankStats(/*total_frames=*/100,
                                        /*janky_frames=*/10);
  }

  {
    EXPECT_CALL(*root_view,
                ReportScrollJankStats(/*total_frames=*/50, /*janky_frames=*/5))
        .Times(1);
    child_view->ReportScrollJankStats(/*total_frames=*/50, /*janky_frames=*/5);
  }

  child_view->Destroy();
  connector->SetRootRenderWidgetHostView(nullptr);
}
#endif

}  // namespace content
