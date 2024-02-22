// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_H_
#define CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "content/common/content_export.h"
#include "content/common/input/fling_scheduler_base.h"
#include "content/common/input/input_disposition_handler.h"
#include "content/common/input/input_injector.mojom-shared.h"
#include "content/common/input/input_router_impl.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/page/widget.mojom.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom.h"

namespace content {

class MockRenderInputRouter;

// RenderInputRouter is currently owned by RenderWidgetHostImpl and is being
// used for forwarding input events. It maintains mojo connections
// with renderers to do so. In future, this class will be used to handle acks
// from renderers and with Input on Viz project
// (https://docs.google.com/document/d/1mcydbkgFCO_TT9NuFE962L8PLJWT2XOfXUAPO88VuKE),
// this will also be used to handle input events on VizCompositorThread (GPU
// process).
class CONTENT_EXPORT RenderInputRouter : public InputRouterImplClient {
 public:
  RenderInputRouter(const RenderInputRouter&) = delete;
  RenderInputRouter& operator=(const RenderInputRouter&) = delete;

  ~RenderInputRouter() override;

  RenderInputRouter(InputRouterImplClient* host,
                    InputDispositionHandler* handler,
                    std::unique_ptr<FlingSchedulerBase> fling_scheduler,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void SetupInputRouter(float device_scale_factor);

  void BindRenderInputRouterInterfaces(
      mojo::PendingRemote<blink::mojom::RenderInputRouterClient> remote);

  void RendererWidgetCreated(bool for_frame_widget);

  InputRouter* input_router() { return input_router_.get(); }

  void SetForceEnableZoom(bool);
  void SetDeviceScaleFactor(float device_scale_factor);

  void ProgressFlingIfNeeded(base::TimeTicks current_time);
  blink::mojom::FrameWidgetInputHandler* GetFrameWidgetInputHandler();

  // InputRouterImplClient overrides.
  blink::mojom::WidgetInputHandler* GetWidgetInputHandler() override;
  void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::optional<std::vector<gfx::Rect>>& character_bounds,
      const std::optional<std::vector<gfx::Rect>>& line_bounds) override;
  void OnImeCancelComposition() override;
  StylusInterface* GetStylusInterface() override;
  void OnStartStylusWriting() override;
  bool IsWheelScrollInProgress() override;
  bool IsAutoscrollInProgress() override;
  void SetMouseCapture(bool capture) override;
  void SetAutoscrollSelectionActiveInMainFrame(
      bool autoscroll_selection) override;
  void RequestMouseLock(
      bool from_user_gesture,
      bool unadjusted_movement,
      InputRouterImpl::RequestMouseLockCallback response) override;
  gfx::Size GetRootWidgetViewportSize() override;

  // InputRouterClient overrides.
  blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& event,
      const ui::LatencyInfo& latency_info) override;
  void IncrementInFlightEventCount() override;
  void NotifyUISchedulerOfGestureEventUpdate(
      blink::WebInputEvent::Type gesture_event) override;
  void DecrementInFlightEventCount(
      blink::mojom::InputEventResultSource ack_source) override;
  void DidOverscroll(const ui::DidOverscrollParams& params) override;
  void DidStartScrollingViewport() override;
  void OnSetCompositorAllowedTouchAction(cc::TouchAction) override {}
  void OnInvalidInputEventSource() override;
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) override;
  void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& latency_info) override;

  void FlushForTesting() {
    if (widget_input_handler_) {
      return widget_input_handler_.FlushForTesting();
    }
  }

  bool GetForceEnableZoom() { return force_enable_zoom_; }
  void ResetFrameWidgetInputHandler() { frame_widget_input_handler_.reset(); }
  void ResetWidgetInputHandler() { widget_input_handler_.reset(); }

 private:
  friend MockRenderInputRouter;

  // Must be declared before `input_router_`. The latter is constructed by
  // borrowing a reference to this object, so it must be deleted first.
  std::unique_ptr<FlingSchedulerBase> fling_scheduler_;
  std::unique_ptr<InputRouter> input_router_;

  raw_ptr<InputRouterImplClient> input_router_impl_client_;
  raw_ptr<InputDispositionHandler> input_disposition_handler_;

  mojo::Remote<blink::mojom::RenderInputRouterClient> client_remote_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  mojo::Remote<blink::mojom::WidgetInputHandler> widget_input_handler_;
  mojo::AssociatedRemote<blink::mojom::FrameWidgetInputHandler>
      frame_widget_input_handler_;

  bool force_enable_zoom_ = false;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_H_
