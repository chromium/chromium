// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_MOUSE_CURSOR_OVERLAY_CONTROLLER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_MOUSE_CURSOR_OVERLAY_CONTROLLER_H_

#include <atomic>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class TickClock;
}  // namespace base

namespace content {

// MouseCursorOverlayController is used by FrameSinkVideoCaptureDevice to manage
// the mouse cursor overlay in the viz::FrameSinkVideoCapturer session based on
// the behavior of the mouse cursor reported by the windowing system.
//
// All parts of this class are meant to run on the UI BrowserThread, except for
// IsUserInteractingWithView(), which may be called from any thread. It is up to
// the client code to ensure the controller's lifetime while in use across
// multiple threads.
class CONTENT_EXPORT MouseCursorOverlayController {
 public:
  using Overlay = viz::mojom::FrameSinkVideoCaptureOverlay;

  MouseCursorOverlayController();

  MouseCursorOverlayController(const MouseCursorOverlayController&) = delete;
  MouseCursorOverlayController& operator=(const MouseCursorOverlayController&) =
      delete;

  ~MouseCursorOverlayController();

  // Sets a new target view to monitor for mouse cursor updates.
  void SetTargetView(gfx::NativeView view);

  // If the target view is not a gfx::NativeView (which is the case when
  // capturing a NSWindow on macOS), this function may be used to set the size
  // of the target. This function will only have an effect if SetTargetView has
  // not been called (it doesn't make sense to call both functions). The units
  // for |target_size| are different on different platforms (DIPs on macOS,
  // pixels on all other platforms).
  void SetTargetSize(const gfx::Size& target_size) {
    target_size_ = target_size;
  }

  // Takes ownership of and starts controlling the given |overlay|, invoking its
  // methods (and destruction) via the given |task_runner|.
  void Start(std::unique_ptr<Overlay> overlay,
             scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Stops controlling the Overlay (passed to Start()) and schedules its
  // destruction.
  void Stop();

  // Returns true if the user has recently interacted with the target (by
  // moving or clicking the mouse).
  bool IsUserInteractingWithView() const;

  // Called from platform-specific code to report on mouse events within the
  // captured view. The units for |location| depend on the platform (DIPs on
  // macOS and pixels on other platforms).
  void OnMouseMoved(const gfx::PointF& location);
  void OnMouseClicked(const gfx::PointF& location);

  // Returns a weak pointer.
  base::WeakPtr<MouseCursorOverlayController> GetWeakPtr();

  bool ShouldSendMouseEvents() const { return should_send_mouse_events_; }

 private:
  friend class MouseCursorOverlayControllerBrowserTest;

  // Observes mouse events from the windowing system and reports them via
  // OnMouseMoved(), OnMouseClicked(), and OnMouseHasGoneIdle().
  class Observer;

  enum MouseMoveBehavior {
    kNotMoving,               // Mouse has not moved recently.
    kStartingToMove,          // Mouse has moved, but not significantly.
    kRecentlyMovedOrClicked,  // Sufficient mouse activity present.
  };

  // Called by the |mouse_activity_ended_timer_| once no mouse events have
  // occurred for kIdleTimeout. Also, called by platform-specific code when
  // changing the target view.
  void OnMouseHasGoneIdle();

  // Accessors for |mouse_move_behavior_atomic_|. See comments below.
  MouseMoveBehavior mouse_move_behavior() const {
    return mouse_move_behavior_atomic_.load(std::memory_order_relaxed);
  }
  void set_mouse_move_behavior(MouseMoveBehavior behavior) {
    mouse_move_behavior_atomic_.store(behavior, std::memory_order_relaxed);
  }

  // Send the latest mouse event reported by the captured view to the associated
  // mouse cursor overlay.
  void SendMouseEvent();

  // Examines the current mouse movement behavior, view properties, and cursor
  // changes to determine whether to show or hide the overlay. |location| is the
  // current mouse cursor location.
  void UpdateOverlay(const gfx::PointF& location);

  // Returns the current mouse cursor. The default "arrow pointer" cursor will
  // be returned in lieu of a null cursor.
  gfx::NativeCursor GetCurrentCursorOrDefault() const;

  // Computes where the overlay should be shown, in terms of relative
  // coordinates. This takes the view size, coordinate systems of the view and
  // cursor bitmap, and cursor hotspot offset; all into account.
  gfx::RectF ComputeRelativeBoundsForOverlay(const gfx::NativeCursor& cursor,
                                             const gfx::PointF& location) const;

  // Called after SetTargetView() to ignore mouse events from the
  // platform/toolkit and set a default mouse cursor. This is used by the
  // browser tests to prevent actual mouse movement from interfering with the
  // testing of the control logic.
  void DisconnectFromToolkitForTesting();

  // Returns the image of the mouse cursor.
  static SkBitmap GetCursorImage(const gfx::NativeCursor&);

  // Called from platform-specific code to report on mouse events within the
  // captured view.
  void OnMouseCoordinatesUpdated(const gfx::Point& coordinates);

  // Overrides the tick clock used by |this| for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // Platform-specific mouse event observer. Updated by SetTargetView().
  std::unique_ptr<Observer> observer_;

  // Updated in the mouse event handlers and used to decide whether the user is
  // interacting with the view and whether to update the overlay.
  gfx::PointF mouse_move_start_location_;
  base::RetainingOneShotTimer mouse_activity_ended_timer_;

  // Updated in the mouse event handlers and read by IsUserInteractingWithView()
  // (on any thread). This is not protected by a mutex since strict memory
  // ordering semantics are not necessary, just atomicity between threads. All
  // code should use the accessors to read or set this value.
  std::atomic<MouseMoveBehavior> mouse_move_behavior_atomic_;

  // The overlay being controlled, and the task runner to use to invoke its
  // methods and destruction.
  std::unique_ptr<Overlay> overlay_;
  scoped_refptr<base::SequencedTaskRunner> overlay_task_runner_;

  // The last-shown mouse cursor. UpdateOverlay() uses this to determine whether
  // to update the cursor image, or just the overlay position.
  gfx::NativeCursor last_cursor_ = gfx::NativeCursor();

  // This is empty if the overlay should be hidden. Otherwise, it represents a
  // shown overlay with a relative position within the view in terms of the
  // range [0.0,1.0). It can sometimes be a little bit outside of that range,
  // depending on the cursor's hotspot.
  gfx::RectF bounds_;

  // The target's size, if set explicitly by SetTargetSize.
  gfx::Size target_size_;

  // Whether to transmit mouse events to the renderer process for dispatching
  // it via the Captured Mouse Events API.
  // See https://screen-share.github.io/captured-mouse-events/
  bool should_send_mouse_events_ = false;

  // Latest coordinates reported to OnMouseCoordinatesUpdated() and timer to
  // postpone forwarding them to SendMouseEvent(), if necessary. Note that the
  // initial value of last_observed_coordinates_ does not matter here, since
  // it is always set in OnMouseCoordinatesUpdated() before being read in
  // SendMouseEvent().
  gfx::Point last_observed_coordinates_;
  base::OneShotTimer last_observed_coordinates_timer_;

  // Latest coordinates sent to the the overlay and time when it happened.
  std::optional<gfx::Point> last_emitted_coordinates_;
  base::TimeTicks last_emitted_coordinates_time_;

  // Everything except the constructor and IsUserInteractingWithView() must be
  // called on the UI BrowserThread.
  SEQUENCE_CHECKER(ui_sequence_checker_);

  // Tick clock used for controlling event delays.
  raw_ptr<const base::TickClock> tick_clock_ = nullptr;

  base::WeakPtrFactory<MouseCursorOverlayController> weak_factory_{this};

  // Minium movement before the cursor has been considered intentionally moved
  // by the user.
  static constexpr int kMinMovementPixels = 15;

  // Amount of time to elapse with no mouse activity before the cursor should
  // stop showing.
  static constexpr base::TimeDelta kIdleTimeout = base::Seconds(2);

  // Special value from the Captured Mouse Events specification to indicate that
  // the mouse is outside the captured view.
  // See
  // https://screen-share.github.io/captured-mouse-events/#captured-mouse-change-event
  static constexpr gfx::Point kOutsideSurface = {-1, -1};

  // The specification contains some hints to limit the frequency with which
  // events are fired. This is implemented using a minimal time to wait between
  // two reports of mouse coordinates (corresponding to a max frequency of 30
  // dispatched events per seconds).
  // See
  // https://screen-share.github.io/captured-mouse-events/#captured-mouse-change-event
  static constexpr base::TimeDelta kMinWaitInterval =
      base::Milliseconds(1000 / (30 - 1));
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_MOUSE_CURSOR_OVERLAY_CONTROLLER_H_
