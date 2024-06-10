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

#include "components/input/fling_scheduler_base.h"
#include "content/common/content_export.h"
#include "content/common/input/input_disposition_handler.h"
#include "content/common/input/input_injector.mojom-shared.h"
#include "content/common/input/input_router_impl.h"
#include "content/common/input/render_input_router_delegate.h"
#include "content/common/input/render_input_router_iterator.h"
#include "content/common/input/render_input_router_latency_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/hit_test/input_target_client.mojom.h"
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
class CONTENT_EXPORT RenderInputRouter : public InputRouterImplClient,
                                         public InputDispositionHandler {
 public:
  RenderInputRouter(const RenderInputRouter&) = delete;
  RenderInputRouter& operator=(const RenderInputRouter&) = delete;

  ~RenderInputRouter() override;

  RenderInputRouter(InputRouterImplClient* host,
                    std::unique_ptr<input::FlingSchedulerBase> fling_scheduler,
                    RenderInputRouterDelegate* delegate,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void SetupInputRouter(float device_scale_factor);

  void BindRenderInputRouterInterfaces(
      mojo::PendingRemote<blink::mojom::RenderInputRouterClient> remote);

  void RendererWidgetCreated(bool for_frame_widget);

  InputRouter* input_router() { return input_router_.get(); }
  RenderInputRouterDelegate* delegate() { return delegate_; }

  void SetForceEnableZoom(bool);
  void SetDeviceScaleFactor(float device_scale_factor);

  void ProgressFlingIfNeeded(base::TimeTicks current_time);
  void StopFling();

  blink::mojom::FrameWidgetInputHandler* GetFrameWidgetInputHandler();

  void SetView(RenderWidgetHostViewInput* view);

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

  // InputDispositionHandler
  void OnWheelEventAck(const input::MouseWheelEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  void OnTouchEventAck(const input::TouchEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  void OnGestureEventAck(
      const input::GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) override;

  // Dispatch input events with latency information
  void DispatchInputEventWithLatencyInfo(
      const blink::WebInputEvent& event,
      ui::LatencyInfo* latency,
      ui::EventLatencyMetadata* event_latency_metadata);

  virtual void ForwardTouchEventWithLatencyInfo(
      const blink::WebTouchEvent& touch_event,
      const ui::LatencyInfo& latency);  // Virtual for testing

  // Retrieve an iterator over any RenderInputRouters that are
  // immediately embedded within this one. This does not return
  // RenderInputRouters that are embedded indirectly (i.e. nested within
  // embedded hosts).
  std::unique_ptr<RenderInputRouterIterator> GetEmbeddedRenderInputRouters();

  // |point| specifies the location in RenderWidget's coordinates for invoking
  // the context menu.
  void ShowContextMenuAtPoint(const gfx::Point& point,
                              const ui::MenuSourceType source_type);

  void SendGestureEventWithLatencyInfo(
      const input::GestureEventWithLatencyInfo& gesture_with_latency);

  // Signals if this host has forwarded a GestureScrollBegin without yet having
  // forwarded a matching GestureScrollEnd/GestureFlingStart.
  bool is_in_touchscreen_gesture_scroll() const {
    return is_in_gesture_scroll_[static_cast<int>(
        blink::WebGestureDevice::kTouchscreen)];
  }

  void DidStopFlinging() { is_in_touchpad_gesture_fling_ = false; }

  content::RenderInputRouterLatencyTracker* GetLatencyTracker() {
    return latency_tracker_.get();
  }

  void FlushForTesting() {
    if (widget_input_handler_) {
      return widget_input_handler_.FlushForTesting();
    }
  }

  bool GetForceEnableZoom() { return force_enable_zoom_; }
  void ResetFrameWidgetInputInterfaces();
  void ResetWidgetInputInterfaces();

  mojo::Remote<viz::mojom::InputTargetClient>& input_target_client() {
    return input_target_client_;
  }

  void SetInputTargetClientForTesting(
      mojo::Remote<viz::mojom::InputTargetClient> input_target_client);

 private:
  friend MockRenderInputRouter;

  // Must be declared before `input_router_`. The latter is constructed by
  // borrowing a reference to this object, so it must be deleted first.
  std::unique_ptr<input::FlingSchedulerBase> fling_scheduler_;
  std::unique_ptr<InputRouter> input_router_;

  // TODO(wjmaclean) Remove the code for supporting resending gesture events
  // when WebView transitions to OOPIF and BrowserPlugin is removed.
  // http://crbug.com/533069
  std::array<bool,
             base::checked_cast<size_t>(blink::WebGestureDevice::kMaxValue) + 1>
      is_in_gesture_scroll_ = {{false}};
  bool is_in_touchpad_gesture_fling_ = false;
  std::unique_ptr<RenderInputRouterLatencyTracker> latency_tracker_;

  raw_ptr<InputRouterImplClient> input_router_impl_client_;
  raw_ptr<RenderInputRouterDelegate> delegate_;

  mojo::Remote<viz::mojom::InputTargetClient> input_target_client_;
  mojo::Remote<blink::mojom::RenderInputRouterClient> client_remote_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  mojo::Remote<blink::mojom::WidgetInputHandler> widget_input_handler_;
  mojo::AssociatedRemote<blink::mojom::FrameWidgetInputHandler>
      frame_widget_input_handler_;

  bool force_enable_zoom_ = false;

  base::WeakPtr<RenderWidgetHostViewInput> view_input_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_H_
