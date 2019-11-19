// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/mock_render_widget_host.h"

#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"

namespace {
class TestFrameTokenMessageQueue : public content::FrameTokenMessageQueue {
 public:
  TestFrameTokenMessageQueue() = default;
  ~TestFrameTokenMessageQueue() override = default;

  uint32_t processed_frame_messages_count() {
    return processed_frame_messages_count_;
  }

 protected:
  void ProcessSwapMessages(std::vector<IPC::Message> messages) override {
    processed_frame_messages_count_++;
  }

 private:
  uint32_t processed_frame_messages_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestFrameTokenMessageQueue);
};
}  // namespace

namespace content {

MockRenderWidgetHost::~MockRenderWidgetHost() {}

void MockRenderWidgetHost::OnTouchEventAck(
    const TouchEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  // Sniff touch acks.
  acked_touch_event_type_ = event.event.GetType();
  RenderWidgetHostImpl::OnTouchEventAck(event, ack_source, ack_result);
}

void MockRenderWidgetHost::DisableGestureDebounce() {
  input_router_.reset(new InputRouterImpl(this, this, fling_scheduler_.get(),
                                          InputRouter::Config()));
}

void MockRenderWidgetHost::ExpectForceEnableZoom(bool enable) {
  EXPECT_EQ(enable, force_enable_zoom_);

  InputRouterImpl* input_router =
      static_cast<InputRouterImpl*>(input_router_.get());
  EXPECT_EQ(enable, input_router->touch_action_filter_.force_enable_zoom_);
}

// Mocks out |renderer_compositor_frame_sink_| with a
// CompositorFrameSinkClientPtr bound to
// |mock_renderer_compositor_frame_sink|.
void MockRenderWidgetHost::SetMockRendererCompositorFrameSink(
    viz::MockCompositorFrameSinkClient* mock_renderer_compositor_frame_sink) {
  renderer_compositor_frame_sink_.reset();
  renderer_compositor_frame_sink_.Bind(
      mock_renderer_compositor_frame_sink->BindInterfaceRemote());
}

void MockRenderWidgetHost::SetupForInputRouterTest() {
  input_router_.reset(new MockInputRouter(this));
}

uint32_t MockRenderWidgetHost::processed_frame_messages_count() {
  CHECK(frame_token_message_queue_);
  return static_cast<TestFrameTokenMessageQueue*>(
             frame_token_message_queue_.get())
      ->processed_frame_messages_count();
}

// static
MockRenderWidgetHost* MockRenderWidgetHost::Create(
    RenderWidgetHostDelegate* delegate,
    RenderProcessHost* process,
    int32_t routing_id) {
  mojo::PendingRemote<mojom::Widget> widget;
  std::unique_ptr<MockWidgetImpl> widget_impl =
      std::make_unique<MockWidgetImpl>(widget.InitWithNewPipeAndPassReceiver());

  return new MockRenderWidgetHost(delegate, process, routing_id,
                                  std::move(widget_impl), std::move(widget));
}

mojom::WidgetInputHandler* MockRenderWidgetHost::GetWidgetInputHandler() {
  return &mock_widget_input_handler_;
}

void MockRenderWidgetHost::NotifyNewContentRenderingTimeoutForTesting() {
  new_content_rendering_timeout_fired_ = true;
}

MockRenderWidgetHost::MockRenderWidgetHost(
    RenderWidgetHostDelegate* delegate,
    RenderProcessHost* process,
    int routing_id,
    std::unique_ptr<MockWidgetImpl> widget_impl,
    mojo::PendingRemote<mojom::Widget> widget)
    : RenderWidgetHostImpl(delegate,
                           process,
                           routing_id,
                           std::move(widget),
                           /*hidden=*/false,
                           std::make_unique<TestFrameTokenMessageQueue>()),
      new_content_rendering_timeout_fired_(false),
      widget_impl_(std::move(widget_impl)),
      fling_scheduler_(std::make_unique<FlingScheduler>(this)) {
  acked_touch_event_type_ = blink::WebInputEvent::kUndefined;
}

}  // namespace content
