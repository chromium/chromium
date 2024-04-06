// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/input.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/pointer_id.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/mojom/page/widget.mojom.h"

namespace content {
class DevToolsAgentHostImpl;
class RenderFrameHostImpl;
class RenderWidgetHostImpl;
class SyntheticPointerDriver;
class WebContentsImpl;

namespace protocol {

template <class BackendCallback>
class FailSafe;

class InputHandler : public DevToolsDomainHandler, public Input::Backend {
 public:
  InputHandler(bool allow_file_access, bool allow_sending_input_to_browser);

  InputHandler(const InputHandler&) = delete;
  InputHandler& operator=(const InputHandler&) = delete;

  ~InputHandler() override;

  static std::vector<InputHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  // StartDragging is used to inform CDP's InputHandler to start dragging. This
  // gets called whenever the renderer tells the content layer to initiate
  // dragging (see RenderWidgetHostImpl::StartDragging)
  void StartDragging(const DropData& drop_data,
                     const blink::mojom::DragData& drag_data,
                     blink::DragOperationsMask drag_operations_mask,
                     bool* intercepted);
  // DragEnded is used to inform CDP's InputHandler when a drag has ended. This
  // gets called whenever the Drag n' Drop APIs that end dragging get called
  // (all of which exist in RenderWidgetHostImpl).
  //
  // This function ensures the drag state of Chromium is in-sync no matter the
  // source of the drag end.
  //
  // In theory, if OS drag gets initiated, then this function gets called when
  // the OS drag ends. This can happen if some starts and ends a drag with the
  // OS manually before starting to use CDP. In practice, this doesn't occur.
  void DragEnded();

  Response Disable() override;

  void DispatchKeyEvent(
      const std::string& type,
      Maybe<int> modifiers,
      Maybe<double> timestamp,
      Maybe<std::string> text,
      Maybe<std::string> unmodified_text,
      Maybe<std::string> key_identifier,
      Maybe<std::string> code,
      Maybe<std::string> key,
      Maybe<int> windows_virtual_key_code,
      Maybe<int> native_virtual_key_code,
      Maybe<bool> auto_repeat,
      Maybe<bool> is_keypad,
      Maybe<bool> is_system_key,
      Maybe<int> location,
      Maybe<Array<std::string>> commands,
      std::unique_ptr<DispatchKeyEventCallback> callback) override;

  void InsertText(const std::string& text,
                  std::unique_ptr<InsertTextCallback> callback) override;

  void ImeSetComposition(
      const std::string& text,
      int selection_start,
      int selection_end,
      Maybe<int> replacement_start,
      Maybe<int> replacement_end,
      std::unique_ptr<ImeSetCompositionCallback> callback) override;

  void DispatchMouseEvent(
      const std::string& event_type,
      double x,
      double y,
      Maybe<int> modifiers,
      Maybe<double> timestamp,
      Maybe<std::string> button,
      Maybe<int> buttons,
      Maybe<int> click_count,
      Maybe<double> force,
      Maybe<double> tangential_pressure,
      Maybe<double> tilt_x,
      Maybe<double> tilt_y,
      Maybe<int> twist,
      Maybe<double> delta_x,
      Maybe<double> delta_y,
      Maybe<std::string> pointer_type,
      std::unique_ptr<DispatchMouseEventCallback> callback) override;

  void DispatchDragEvent(
      const std::string& event_type,
      double x,
      double y,
      std::unique_ptr<Input::DragData> data,
      Maybe<int> modifiers,
      std::unique_ptr<DispatchDragEventCallback> callback) override;

  void DispatchTouchEvent(
      const std::string& type,
      std::unique_ptr<Array<Input::TouchPoint>> touch_points,
      protocol::Maybe<int> modifiers,
      protocol::Maybe<double> timestamp,
      std::unique_ptr<DispatchTouchEventCallback> callback) override;

  void CancelDragging(
      std::unique_ptr<CancelDraggingCallback> callback) override;

  Response EmulateTouchFromMouseEvent(const std::string& type,
                                      int x,
                                      int y,
                                      const std::string& button,
                                      Maybe<double> timestamp,
                                      Maybe<double> delta_x,
                                      Maybe<double> delta_y,
                                      Maybe<int> modifiers,
                                      Maybe<int> click_count) override;

  Response SetIgnoreInputEvents(bool ignore) override;
  Response SetInterceptDrags(bool enabled) override;

  void SynthesizePinchGesture(
      double x,
      double y,
      double scale_factor,
      Maybe<int> relative_speed,
      Maybe<std::string> gesture_source_type,
      std::unique_ptr<SynthesizePinchGestureCallback> callback) override;

  void SynthesizeScrollGesture(
      double x,
      double y,
      Maybe<double> x_distance,
      Maybe<double> y_distance,
      Maybe<double> x_overscroll,
      Maybe<double> y_overscroll,
      Maybe<bool> prevent_fling,
      Maybe<int> speed,
      Maybe<std::string> gesture_source_type,
      Maybe<int> repeat_count,
      Maybe<int> repeat_delay_ms,
      Maybe<std::string> interaction_marker_name,
      std::unique_ptr<SynthesizeScrollGestureCallback> callback) override;

  void SynthesizeTapGesture(
      double x,
      double y,
      Maybe<int> duration,
      Maybe<int> tap_count,
      Maybe<std::string> gesture_source_type,
      std::unique_ptr<SynthesizeTapGestureCallback> callback) override;

 private:
  class InputInjector;
  class DragController {
   public:
    // `handler` must live as long as the drag controller.
    explicit DragController(InputHandler& handler);
    ~DragController();

    DragController(const DragController&) = delete;
    DragController& operator=(const DragController&) = delete;

    // Returns `true` if the drag controller handles the mouse event. All
    // arguments will be moved in this case.
    bool HandleMouseEvent(
        RenderWidgetHostImpl& host,
        const blink::WebMouseEvent& event,
        std::unique_ptr<DispatchMouseEventCallback>& callback);

    // You should call this whenever dragging needs to be cancelled (perhaps an
    // invalid state or desired by the user).
    void CancelDragging(base::OnceClosure callback);

    // Returns `true` if we are currently dragging.
    bool IsDragging() { return !!drag_state_; }

   private:
    struct DragState;
    struct InitialState;

    friend void InputHandler::StartDragging(
        const DropData& drop_data,
        const blink::mojom::DragData& drag_data,
        blink::DragOperationsMask drag_operations_mask,
        bool* intercepted);
    friend void InputHandler::DragEnded();

    void EnsureDraggingEntered(RenderWidgetHostImpl& host,
                               const blink::WebMouseEvent& event);

    // Called by the input handler to start a dragging session.
    void StartDragging(const content::DropData& drop_data,
                       blink::DragOperationsMask drag_operations_mask);

    // Updates the drag with the given mouse event.
    //
    // `callback` may be null.
    void UpdateDragging(
        RenderWidgetHostImpl& host,
        std::unique_ptr<blink::WebMouseEvent> event,
        std::unique_ptr<FailSafe<DispatchMouseEventCallback>> callback);
    void DragUpdated(
        std::unique_ptr<blink::WebMouseEvent> event,
        std::unique_ptr<FailSafe<DispatchMouseEventCallback>> callback,
        ui::mojom::DragOperation operation,
        bool document_is_handling_drag);

    // Ends the drag with the given event and host.
    //
    // Note only the modifiers for the event are really used here, so it's
    // expected you've updated the drag with the latest info (e.g. position)
    // excluding modifiers. This ensures the drag event sequence is semantically
    // correct. The event itself is still needed in case dragging suddenly stops
    // for external reasons and we need to reschedule it.
    //
    // Also note the host is only used if there are no updates ongoing.
    // Otherwise, it's a nullptr. This is an optimization.
    void EndDragging(
        RenderWidgetHostImpl* host,
        std::unique_ptr<blink::WebMouseEvent> event,
        std::unique_ptr<FailSafe<DispatchMouseEventCallback>> callback);
    void EndDraggingWithRenderWidgetHostAtPoint(
        std::unique_ptr<blink::WebMouseEvent> event,
        std::unique_ptr<FailSafe<DispatchMouseEventCallback>> callback,
        base::WeakPtr<RenderWidgetHostViewBase> view,
        std::optional<gfx::PointF> maybe_point);

    const raw_ref<InputHandler> handler_;

    // These get used for starting a drag.
    std::unique_ptr<InitialState> initial_state_;

    std::unique_ptr<DragState> drag_state_;

    base::WeakPtrFactory<DragController> weak_factory_{this};
  };

  void DispatchWebTouchEvent(
      const std::string& type,
      std::unique_ptr<Array<Input::TouchPoint>> touch_points,
      protocol::Maybe<int> modifiers,
      protocol::Maybe<double> timestamp,
      std::unique_ptr<DispatchTouchEventCallback> callback);

  void DispatchSyntheticPointerActionTouch(
      const std::string& type,
      std::unique_ptr<Array<Input::TouchPoint>> touch_points,
      protocol::Maybe<int> modifiers,
      protocol::Maybe<double> timestamp,
      std::unique_ptr<DispatchTouchEventCallback> callback);

  void OnWidgetForDispatchMouseEvent(
      std::unique_ptr<DispatchMouseEventCallback> callback,
      std::unique_ptr<blink::WebMouseEvent> mouse_event,
      base::WeakPtr<RenderWidgetHostViewBase> target,
      std::optional<gfx::PointF> point);

  void OnWidgetForDispatchDragEvent(
      const std::string& event_type,
      double x,
      double y,
      std::unique_ptr<Input::DragData> data,
      Maybe<int> modifiers,
      std::unique_ptr<DispatchDragEventCallback> callback,
      base::WeakPtr<RenderWidgetHostViewBase> target,
      std::optional<gfx::PointF> point);

  void OnWidgetForDispatchWebTouchEvent(
      std::unique_ptr<DispatchTouchEventCallback> callback,
      std::vector<blink::WebTouchEvent> events,
      base::WeakPtr<RenderWidgetHostViewBase> target,
      std::optional<gfx::PointF> point);

  SyntheticPointerActionParams PrepareSyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType pointer_action_type,
      int id,
      double x,
      double y,
      int key_modifiers,
      float radius_x = 1.f,
      float radius_y = 1.f,
      float rotation_angle = 0.f,
      float force = 1.f);

  void SynthesizeRepeatingScroll(
      SyntheticSmoothScrollGestureParams gesture_params,
      int repeat_count,
      base::TimeDelta repeat_delay,
      std::string interaction_marker_name,
      int id,
      std::unique_ptr<SynthesizeScrollGestureCallback> callback);

  void OnScrollFinished(
      SyntheticSmoothScrollGestureParams gesture_params,
      int repeat_count,
      base::TimeDelta repeat_delay,
      std::string interaction_marker_name,
      int id,
      std::unique_ptr<SynthesizeScrollGestureCallback> callback,
      SyntheticGesture::Result result);

  void HandleMouseEvent(std::unique_ptr<blink::WebMouseEvent> event,
                        std::unique_ptr<DispatchMouseEventCallback> callback);

  void ClearInputState();
  bool PointIsWithinContents(gfx::PointF point) const;
  InputInjector* EnsureInjector(RenderWidgetHostImpl* widget_host);

  RenderWidgetHostViewBase* GetRootView();

  float ScaleFactor();

  raw_ptr<RenderFrameHostImpl> host_ = nullptr;
  // WebContents associated with the |host_|.
  raw_ptr<WebContentsImpl> web_contents_ = nullptr;
  std::unique_ptr<Input::Frontend> frontend_;
  base::flat_set<std::unique_ptr<InputInjector>, base::UniquePtrComparator>
      injectors_;
  int last_id_ = 0;
  bool ignore_input_events_ = false;
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;
  bool intercept_drags_ = false;
  DragController drag_controller_;
  const bool allow_file_access_;
  const bool allow_sending_input_to_browser_ = false;
  std::set<int> pointer_ids_;
  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver_;
  base::flat_map<blink::PointerId, blink::WebTouchPoint> touch_points_;
  base::WeakPtrFactory<InputHandler> weak_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
