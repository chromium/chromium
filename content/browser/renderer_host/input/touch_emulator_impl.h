// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_IMPL_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/input/touch_emulator.h"
#include "components/input/touch_emulator_client.h"
#include "content/common/content_export.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
}  // namespace blink

namespace input {
class RenderWidgetHostViewInput;
}  // namespace input

namespace content {

class CONTENT_EXPORT TouchEmulatorImpl : public input::TouchEmulator {
 public:
  TouchEmulatorImpl(input::TouchEmulatorClient* client,
                    float device_scale_factor);

  TouchEmulatorImpl(const TouchEmulatorImpl&) = delete;
  TouchEmulatorImpl& operator=(const TouchEmulatorImpl&) = delete;

  ~TouchEmulatorImpl() override;

  // TouchEmulator implementation.
  void SetDeviceScaleFactor(float device_scale_factor) override;
  void SetDoubleTapSupportForPageEnabled(bool enabled) override;
  bool IsEnabled() const override;
  bool HandleTouchEvent(const blink::WebTouchEvent& event) override;
  void OnGestureEventAck(
      const blink::WebGestureEvent& event,
      input::RenderWidgetHostViewInput* target_view) override;
  void OnViewDestroyed(
      input::RenderWidgetHostViewInput* destroyed_view) override;
  bool HandleTouchEventAck(
      const blink::WebTouchEvent& event,
      blink::mojom::InputEventResultState ack_result) override;

  void Enable(Mode mode, ui::GestureProviderConfigType config_type);
  void Disable();

  // Returns |true| if the event was consumed. Consumed event should not
  // propagate any further.
  // TODO(dgozman): maybe pass latency info together with events.
  bool HandleMouseEvent(const blink::WebMouseEvent& event,
                        input::RenderWidgetHostViewInput* target_view);
  bool HandleMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  bool HandleKeyboardEvent(const blink::WebKeyboardEvent& event);

  // Injects a touch event to be processed for gestures and optionally
  // forwarded to the client. Only works in kInjectingTouchEvents mode.
  void InjectTouchEvent(const blink::WebTouchEvent& event,
                        input::RenderWidgetHostViewInput* target_view,
                        base::OnceClosure completion_callback);

  // Cancel any touches, for example, when focus is lost.
  void CancelTouch();

  // This is needed because SyntheticGestureSmoothDrag doesn't support setting
  // key-modifiers on the drag sequence.
  // https://crbug.com/901374.
  void SetPinchGestureModeForTesting(bool pinch_gesture_mode);
  bool suppress_next_fling_cancel_for_testing() const {
    return suppress_next_fling_cancel_;
  }

 private:
  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;
  bool RequiresDoubleTapGestureEvents() const override;

  ui::Cursor InitCursorFromResource(int resource_id);
  void InitCursors();
  void ResetState();
  void UpdateCursor();
  bool UpdateShiftPressed(bool shift_pressed);

  // Whether we should convert scrolls into pinches.
  bool InPinchGestureMode() const;

  void FillTouchEventAndPoint(const blink::WebMouseEvent& mouse_event,
                              const gfx::PointF& pos_in_root);
  blink::WebGestureEvent GetPinchGestureEvent(
      blink::WebInputEvent::Type type,
      const blink::WebGestureEvent& original_event);

  // The following methods generate and pass gesture events to the renderer.
  void PinchBegin(const blink::WebGestureEvent& event);
  void PinchUpdate(const blink::WebGestureEvent& event);
  void PinchEnd(const blink::WebGestureEvent& event);
  void ScrollEnd(const blink::WebGestureEvent& event);

  // Offers the emulated event to |gesture_provider_|, conditionally forwarding
  // it to the client if appropriate. Returns whether event was handled
  // synchronously, and there will be no ack.
  bool HandleEmulatedTouchEvent(blink::WebTouchEvent event,
                                input::RenderWidgetHostViewInput* target_view);

  // Called when ack for injected touch has been received.
  void OnInjectedTouchCompleted();

  const raw_ptr<input::TouchEmulatorClient> client_;

  // Emulator is enabled iff gesture provider is created.
  // Disabled emulator does only process touch acks left from previous
  // emulation. It does not intercept any events.
  std::unique_ptr<ui::FilteredGestureProvider> gesture_provider_;
  ui::GestureProviderConfigType gesture_provider_config_type_;
  Mode mode_;
  bool double_tap_enabled_;

  // While emulation is on, default cursor is touch. Pressing shift changes
  // cursor to the pinch one.
  ui::Cursor touch_cursor_;
  ui::Cursor pinch_cursor_;
  gfx::SizeF cursor_size_;

  float cursor_scale_factor_ = 0;

  // These are used to drop extra mouse move events coming too quickly, so
  // we don't handle too much touches in gesture provider.
  bool last_mouse_event_was_move_;
  base::TimeTicks last_mouse_move_timestamp_;

  bool mouse_pressed_;
  bool shift_pressed_;
  bool pinch_gesture_mode_for_testing_;

  blink::WebTouchEvent touch_event_;
  int emulated_stream_active_sequence_count_;
  int native_stream_active_sequence_count_;
  raw_ptr<input::RenderWidgetHostViewInput> last_emulated_start_target_;
  // TODO(einbinder): this relies on synchronous tap gesture generation and does
  // not work for any other gestures. We should switch to callbacks which go
  // through touches and gestures once that's available.
  int pending_taps_count_;

  // Whether we should suppress next fling cancel. This may happen when we
  // did not send fling start in pinch mode.
  bool suppress_next_fling_cancel_;

  // Point which does not move while pinch-zooming.
  gfx::PointF pinch_anchor_;
  // The cumulative scale change from the start of pinch gesture.
  float pinch_scale_;
  bool pinch_gesture_active_;

  base::queue<base::OnceClosure> injected_touch_completion_callbacks_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_IMPL_H_
