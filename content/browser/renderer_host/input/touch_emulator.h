// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "content/browser/renderer_host/input/touch_emulator_client.h"
#include "content/common/cursors/webcursor.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace content {

class RenderWidgetHostViewBase;

// Emulates touch input. See TouchEmulator::Mode for more details.
class CONTENT_EXPORT TouchEmulator : public ui::GestureProviderClient {
 public:
  enum class Mode {
    // Emulator will consume incoming mouse events and transform them
    // into touches and gestures.
    kEmulatingTouchFromMouse,
    // Emulator will not consume incoming mouse events and instead will
    // wait for manually injected touch events.
    kInjectingTouchEvents
  };

  TouchEmulator(TouchEmulatorClient* client, float device_scale_factor);
  ~TouchEmulator() override;

  void Enable(Mode mode, ui::GestureProviderConfigType config_type);
  void Disable();

  // Call when device scale factor changes.
  void SetDeviceScaleFactor(float device_scale_factor);

  // See GestureProvider::SetDoubleTapSupportForPageEnabled.
  void SetDoubleTapSupportForPageEnabled(bool enabled);

  // Note that TouchEmulator should always listen to touch events and their acks
  // (even in disabled state) to track native stream presence.
  bool enabled() const { return !!gesture_provider_; }

  // Returns |true| if the event was consumed. Consumed event should not
  // propagate any further.
  // TODO(dgozman): maybe pass latency info together with events.
  bool HandleMouseEvent(const blink::WebMouseEvent& event,
                        RenderWidgetHostViewBase* target_view);
  bool HandleMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  bool HandleKeyboardEvent(const blink::WebKeyboardEvent& event);
  bool HandleTouchEvent(const blink::WebTouchEvent& event);

  void OnGestureEventAck(const blink::WebGestureEvent& event,
                         RenderWidgetHostViewBase* target_view);

  // Called to notify the TouchEmulator when a view is destroyed.
  void OnViewDestroyed(RenderWidgetHostViewBase* destroyed_view);

  // Returns |true| if the event ack was consumed. Consumed ack should not
  // propagate any further.
  bool HandleTouchEventAck(const blink::WebTouchEvent& event,
                           blink::mojom::InputEventResultState ack_result);

  // Injects a touch event to be processed for gestures and optionally
  // forwarded to the client. Only works in kInjectingTouchEvents mode.
  void InjectTouchEvent(const blink::WebTouchEvent& event,
                        RenderWidgetHostViewBase* target_view,
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

  // Returns cursor size in DIP.
  gfx::SizeF InitCursorFromResource(
      WebCursor* cursor, float scale, int resource_id);
  bool InitCursors(float device_scale_factor, bool force);
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
                                RenderWidgetHostViewBase* target_view);

  // Called when ack for injected touch has been received.
  void OnInjectedTouchCompleted();

  TouchEmulatorClient* const client_;

  // Emulator is enabled iff gesture provider is created.
  // Disabled emulator does only process touch acks left from previous
  // emulation. It does not intercept any events.
  std::unique_ptr<ui::FilteredGestureProvider> gesture_provider_;
  ui::GestureProviderConfigType gesture_provider_config_type_;
  Mode mode_;
  bool double_tap_enabled_;

  bool use_2x_cursors_;
  // While emulation is on, default cursor is touch. Pressing shift changes
  // cursor to the pinch one.
  WebCursor pointer_cursor_;
  WebCursor touch_cursor_;
  WebCursor pinch_cursor_;
  gfx::SizeF cursor_size_;

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
  RenderWidgetHostViewBase* last_emulated_start_target_;
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

  DISALLOW_COPY_AND_ASSIGN(TouchEmulator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_H_
