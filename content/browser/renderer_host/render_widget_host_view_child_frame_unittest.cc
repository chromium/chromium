// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
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
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/frame_visual_properties.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/fake_renderer_compositor_frame_sink.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
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

  void SetViewportIntersection(const gfx::Rect& intersection,
                               const gfx::Rect& compositor_visible_rect) {
    viewport_intersection_rect_ = intersection;
    compositor_visible_rect_ = compositor_visible_rect;
  }

  RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override {
    return nullptr;
  }

  void BubbleScrollEvent(const blink::WebGestureEvent& event) override {
    if (event.GetType() == blink::WebInputEvent::kGestureScrollUpdate)
      seen_bubbled_gsu_ = true;
  }

  viz::SurfaceInfo last_surface_info_;
  bool seen_bubbled_gsu_ = false;
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
    mojom::WidgetPtr widget;
    widget_impl_ = std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));
    widget_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host, routing_id, std::move(widget), false);
    view_ = RenderWidgetHostViewChildFrame::Create(widget_host_);

    test_frame_connector_ =
        new MockFrameConnectorDelegate(use_zoom_for_device_scale_factor);
    test_frame_connector_->SetView(view_);
    view_->SetFrameConnectorDelegate(test_frame_connector_);

    viz::mojom::CompositorFrameSinkPtr sink;
    viz::mojom::CompositorFrameSinkRequest sink_request =
        mojo::MakeRequest(&sink);
    viz::mojom::CompositorFrameSinkClientRequest client_request =
        mojo::MakeRequest(&renderer_compositor_frame_sink_ptr_);
    renderer_compositor_frame_sink_ =
        std::make_unique<FakeRendererCompositorFrameSink>(
            std::move(sink), std::move(client_request));
    view_->DidCreateNewRendererCompositorFrameSink(
        renderer_compositor_frame_sink_ptr_.get());
  }

  void TearDown() override {
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
  TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<BrowserContext> browser_context_;
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
  viz::mojom::CompositorFrameSinkClientPtr renderer_compositor_frame_sink_ptr_;

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

// Verify that RenderWidgetHostViewChildFrame passes the child's SurfaceId to
// FrameConnectorDelegate to be sent to the embedding renderer.
TEST_F(RenderWidgetHostViewChildFrameTest, PassesSurfaceId) {
  gfx::Size view_size(100, 100);
  gfx::Rect view_rect(view_size);
  float scale_factor = 1.f;

  view_->SetSize(view_size);
  view_->Show();

  viz::SurfaceId surface_id(view_->GetFrameSinkId(),
                            view_->GetLocalSurfaceId());
  viz::SurfaceInfo surface_info(surface_id, scale_factor, view_size);
  view_->OnFirstSurfaceActivation(surface_info);

  EXPECT_EQ(surface_info, test_frame_connector_->last_surface_info_);
}

// Tests that the viewport intersection rect is dispatched to the RenderWidget
// whenever screen rects are updated.
TEST_F(RenderWidgetHostViewChildFrameTest, ViewportIntersectionUpdated) {
  gfx::Rect intersection_rect(5, 5, 100, 80);
  test_frame_connector_->SetViewportIntersection(intersection_rect,
                                                 intersection_rect);

  MockRenderProcessHost* process =
      static_cast<MockRenderProcessHost*>(widget_host_->GetProcess());
  process->Init();

  widget_host_->Init();

  const IPC::Message* intersection_update =
      process->sink().GetUniqueMessageMatching(
          WidgetMsg_SetViewportIntersection::ID);
  ASSERT_TRUE(intersection_update);
  std::tuple<gfx::Rect, gfx::Rect, bool> sent_rects;

  WidgetMsg_SetViewportIntersection::Read(intersection_update, &sent_rects);
  EXPECT_EQ(intersection_rect, std::get<0>(sent_rects));
  EXPECT_EQ(intersection_rect, std::get<1>(sent_rects));
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

// Tests that WasResized is called only once and all the parameters change
// atomically.
TEST_F(RenderWidgetHostViewChildFrameTest, WasResizedOncePerChange) {
  MockRenderProcessHost* process =
      static_cast<MockRenderProcessHost*>(widget_host_->GetProcess());
  process->Init();

  widget_host_->Init();

  constexpr gfx::Size compositor_viewport_pixel_size(100, 100);
  constexpr gfx::Rect screen_space_rect(compositor_viewport_pixel_size);
  viz::ParentLocalSurfaceIdAllocator allocator;
  viz::LocalSurfaceId local_surface_id = allocator.GetCurrentLocalSurfaceId();
  constexpr viz::FrameSinkId frame_sink_id(1, 1);
  const viz::SurfaceId surface_id(frame_sink_id, local_surface_id);

  process->sink().ClearMessages();

  FrameVisualProperties visual_properties;
  visual_properties.screen_space_rect = screen_space_rect;
  visual_properties.local_frame_size = compositor_viewport_pixel_size;
  visual_properties.capture_sequence_number = 123u;
  test_frame_connector_->SynchronizeVisualProperties(surface_id,
                                                     visual_properties);

  ASSERT_EQ(1u, process->sink().message_count());

  const IPC::Message* resize_msg = process->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID);
  ASSERT_NE(nullptr, resize_msg);
  WidgetMsg_SynchronizeVisualProperties::Param params;
  WidgetMsg_SynchronizeVisualProperties::Read(resize_msg, &params);
  EXPECT_EQ(compositor_viewport_pixel_size,
            std::get<0>(params).compositor_viewport_pixel_size);
  EXPECT_EQ(screen_space_rect.size(), std::get<0>(params).new_size);
  EXPECT_EQ(local_surface_id, std::get<0>(params).local_surface_id);
  EXPECT_EQ(123u, std::get<0>(params).capture_sequence_number);
}

}  // namespace content
