// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MOCK_RENDER_WIDGET_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MOCK_RENDER_WIDGET_HOST_H_

#include "content/browser/renderer_host/render_view_host_impl.h"

#include "content/browser/renderer_host/input/mock_input_router.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/public/common/input_event_ack_source.h"
#include "content/public/common/input_event_ack_state.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/mock_widget_input_handler.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace viz {
class MockCompositorFrameSinkClient;
}  // namespace viz

namespace content {

class MockRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  // Allow poking at a few private members.
  using RenderWidgetHostImpl::frame_token_message_queue_;
  using RenderWidgetHostImpl::GetVisualProperties;
  using RenderWidgetHostImpl::input_router_;
  using RenderWidgetHostImpl::is_hidden_;
  using RenderWidgetHostImpl::old_visual_properties_;
  using RenderWidgetHostImpl::RendererExited;
  using RenderWidgetHostImpl::SetInitialVisualProperties;
  using RenderWidgetHostImpl::visual_properties_ack_pending_;

  ~MockRenderWidgetHost() override;

  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckSource ack_source,
                       InputEventAckState ack_result) override;

  void reset_new_content_rendering_timeout_fired() {
    new_content_rendering_timeout_fired_ = false;
  }

  bool new_content_rendering_timeout_fired() const {
    return new_content_rendering_timeout_fired_;
  }

  void DisableGestureDebounce();
  void ExpectForceEnableZoom(bool enable);

  blink::WebInputEvent::Type acked_touch_event_type() const {
    return acked_touch_event_type_;
  }

  // Mocks out |renderer_compositor_frame_sink_| with a
  // CompositorFrameSinkClientPtr bound to
  // |mock_renderer_compositor_frame_sink|.
  void SetMockRendererCompositorFrameSink(
      viz::MockCompositorFrameSinkClient* mock_renderer_compositor_frame_sink);

  void SetupForInputRouterTest();

  MockInputRouter* mock_input_router() {
    return static_cast<MockInputRouter*>(input_router_.get());
  }

  InputRouter* input_router() { return input_router_.get(); }

  uint32_t processed_frame_messages_count();

  static MockRenderWidgetHost* Create(RenderWidgetHostDelegate* delegate,
                                      RenderProcessHost* process,
                                      int32_t routing_id);

  mojom::WidgetInputHandler* GetWidgetInputHandler() override;

  MockWidgetInputHandler mock_widget_input_handler_;

 protected:
  void NotifyNewContentRenderingTimeoutForTesting() override;

  bool new_content_rendering_timeout_fired_;
  blink::WebInputEvent::Type acked_touch_event_type_;

 private:
  MockRenderWidgetHost(RenderWidgetHostDelegate* delegate,
                       RenderProcessHost* process,
                       int routing_id,
                       std::unique_ptr<MockWidgetImpl> widget_impl,
                       mojo::PendingRemote<mojom::Widget> widget);

  std::unique_ptr<MockWidgetImpl> widget_impl_;

  std::unique_ptr<FlingScheduler> fling_scheduler_;
  DISALLOW_COPY_AND_ASSIGN(MockRenderWidgetHost);
};

}  // namespace content
#endif  // CONTENT_BROWSER_RENDERER_HOST_MOCK_RENDER_WIDGET_HOST_H_
