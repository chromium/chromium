// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/mock_render_widget_host.h"

#include <memory>

#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/test/test_render_widget_host.h"

namespace content {

MockRenderWidgetHost::~MockRenderWidgetHost() {}

void MockRenderWidgetHost::ExpectForceEnableZoom(bool enable) {
  EXPECT_EQ(enable, render_input_router_->GetForceEnableZoom());

  input::InputRouterImpl* input_router_impl =
      static_cast<input::InputRouterImpl*>(input_router());
  EXPECT_EQ(enable, input_router_impl->touch_action_filter_.force_enable_zoom_);
}

void MockRenderWidgetHost::SetupForInputRouterTest() {
  mock_render_input_router()->SetupForInputRouterTest();
}

// static
std::unique_ptr<MockRenderWidgetHost> MockRenderWidgetHost::Create(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id) {
  return Create(frame_tree, delegate, std::move(site_instance_group),
                routing_id, TestRenderWidgetHost::CreateStubWidgetRemote());
}

// static
std::unique_ptr<MockRenderWidgetHost> MockRenderWidgetHost::Create(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> pending_blink_widget) {
  DCHECK(pending_blink_widget);
  return base::WrapUnique(new MockRenderWidgetHost(
      frame_tree, delegate, std::move(site_instance_group), routing_id,
      std::move(pending_blink_widget)));
}

input::RenderInputRouter* MockRenderWidgetHost::GetRenderInputRouter() {
  return render_input_router_.get();
}

void MockRenderWidgetHost::NotifyNewContentRenderingTimeoutForTesting() {
  new_content_rendering_timeout_fired_ = true;
}

void MockRenderWidgetHost::RejectPointerLockOrUnlockIfNecessary(
    blink::mojom::PointerLockResult result) {
  pointer_lock_rejected_ = true;
  RenderWidgetHostImpl::RejectPointerLockOrUnlockIfNecessary(result);
}

MockRenderWidgetHost::MockRenderWidgetHost(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int routing_id,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> pending_blink_widget)
    : RenderWidgetHostImpl(frame_tree,
                           /*self_owned=*/false,
                           DefaultFrameSinkId(*site_instance_group, routing_id),
                           delegate,
                           site_instance_group,
                           routing_id,
                           /*hidden=*/false,
                           /*renderer_initiated_creation=*/false,
                           std::make_unique<FrameTokenMessageQueue>()) {
  SetupMockRenderInputRouter();
  mojo::AssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
  BindWidgetInterfaces(
      blink_widget_host.BindNewEndpointAndPassDedicatedReceiver(),
      std::move(pending_blink_widget));
}

void MockRenderWidgetHost::SetupMockRenderInputRouter() {
  render_input_router_ = std::make_unique<MockRenderInputRouter>(
      this, MakeFlingScheduler(), this,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  SetupInputRouter();
}

}  // namespace content
