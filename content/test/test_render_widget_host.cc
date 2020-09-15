// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_widget_host.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/public/common/content_features.h"

namespace content {

std::unique_ptr<RenderWidgetHostImpl> TestRenderWidgetHost::Create(
    RenderWidgetHostDelegate* delegate,
    AgentSchedulingGroupHost& agent_scheduling_group,
    int32_t routing_id,
    bool hidden) {
  return base::WrapUnique(new TestRenderWidgetHost(
      delegate, agent_scheduling_group, routing_id, hidden));
}

TestRenderWidgetHost::TestRenderWidgetHost(
    RenderWidgetHostDelegate* delegate,
    AgentSchedulingGroupHost& agent_scheduling_group,
    int32_t routing_id,
    bool hidden)
    : RenderWidgetHostImpl(delegate,
                           agent_scheduling_group,
                           routing_id,
                           hidden,
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

}  // namespace content
