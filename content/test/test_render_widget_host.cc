// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_widget_host.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/site_instance_group.h"
#include "content/public/common/content_features.h"

namespace content {

std::unique_ptr<RenderWidgetHostImpl> TestRenderWidgetHost::Create(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    viz::FrameSinkId frame_sink_id,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    bool hidden,
    bool renderer_initiated_creation) {
  return base::WrapUnique(new TestRenderWidgetHost(
      frame_tree, delegate, frame_sink_id, std::move(site_instance_group),
      routing_id, hidden, renderer_initiated_creation));
}

TestRenderWidgetHost::TestRenderWidgetHost(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    viz::FrameSinkId frame_sink_id,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    bool hidden,
    bool renderer_initiated_creation)
    : RenderWidgetHostImpl(frame_tree,
                           /*self_owned=*/false,
                           frame_sink_id,
                           delegate,
                           std::move(site_instance_group),
                           routing_id,
                           hidden,
                           renderer_initiated_creation,
                           std::make_unique<FrameTokenMessageQueue>()) {
  mojo::AssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
  mojo::AssociatedRemote<blink::mojom::Widget> blink_widget;
  auto blink_widget_receiver =
      blink_widget.BindNewEndpointAndPassDedicatedReceiver();
  BindWidgetInterfaces(
      blink_widget_host.BindNewEndpointAndPassDedicatedReceiver(),
      blink_widget.Unbind());
}

TestRenderWidgetHost::~TestRenderWidgetHost() {}
blink::mojom::WidgetInputHandler*
TestRenderWidgetHost::GetWidgetInputHandler() {
  return &input_handler_;
}

MockWidgetInputHandler* TestRenderWidgetHost::GetMockWidgetInputHandler() {
  return &input_handler_;
}

// static
mojo::PendingAssociatedRemote<blink::mojom::FrameWidget>
TestRenderWidgetHost::CreateStubFrameWidgetRemote() {
  // There's no renderer to pass the receiver to in these tests.
  mojo::AssociatedRemote<blink::mojom::FrameWidget> widget_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget> widget_receiver =
      widget_remote.BindNewEndpointAndPassDedicatedReceiver();
  return widget_remote.Unbind();
}

// static
mojo::PendingAssociatedRemote<blink::mojom::Widget>
TestRenderWidgetHost::CreateStubWidgetRemote() {
  // There's no renderer to pass the receiver to in these tests.
  mojo::AssociatedRemote<blink::mojom::Widget> widget_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::Widget> widget_receiver =
      widget_remote.BindNewEndpointAndPassDedicatedReceiver();
  return widget_remote.Unbind();
}

}  // namespace content
