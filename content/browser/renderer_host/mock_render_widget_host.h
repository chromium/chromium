// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MOCK_RENDER_WIDGET_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MOCK_RENDER_WIDGET_HOST_H_

#include "content/browser/renderer_host/render_view_host_impl.h"

#include "content/browser/renderer_host/input/mock_input_router.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/test/mock_widget_input_handler.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"

namespace content {

class MockRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  // Allow poking at a few private members.
  using RenderWidgetHostImpl::frame_token_message_queue_;
  using RenderWidgetHostImpl::GetInitialVisualProperties;
  using RenderWidgetHostImpl::GetVisualProperties;
  using RenderWidgetHostImpl::input_router_;
  using RenderWidgetHostImpl::is_hidden_;
  using RenderWidgetHostImpl::old_visual_properties_;
  using RenderWidgetHostImpl::RendererExited;
  using RenderWidgetHostImpl::visual_properties_ack_pending_;

  MockRenderWidgetHost(const MockRenderWidgetHost&) = delete;
  MockRenderWidgetHost& operator=(const MockRenderWidgetHost&) = delete;

  ~MockRenderWidgetHost() override;

  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;

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

  void SetupForInputRouterTest();

  MockInputRouter* mock_input_router() {
    return static_cast<MockInputRouter*>(input_router_.get());
  }

  InputRouter* input_router() { return input_router_.get(); }

  static std::unique_ptr<MockRenderWidgetHost> Create(
      FrameTree* frame_tree,
      RenderWidgetHostDelegate* delegate,
      base::SafeRef<SiteInstanceGroup> site_instance_group,
      int32_t routing_id);

  static std::unique_ptr<MockRenderWidgetHost> Create(
      FrameTree* frame_tree,
      RenderWidgetHostDelegate* delegate,
      base::SafeRef<SiteInstanceGroup> site_instance_group,
      int32_t routing_id,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> pending_blink_widget);

  blink::mojom::WidgetInputHandler* GetWidgetInputHandler() override;

  MockWidgetInputHandler mock_widget_input_handler_;

 protected:
  void NotifyNewContentRenderingTimeoutForTesting() override;

  bool new_content_rendering_timeout_fired_;
  blink::WebInputEvent::Type acked_touch_event_type_;

 private:
  MockRenderWidgetHost(
      FrameTree* frame_tree,
      RenderWidgetHostDelegate* delegate,
      base::SafeRef<SiteInstanceGroup> site_instance_group,
      int32_t routing_id,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> pending_blink_widget);

  std::unique_ptr<FlingScheduler> fling_scheduler_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_RENDERER_HOST_MOCK_RENDER_WIDGET_HOST_H_
