// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_TOUCH_PASSTHROUGH_MANAGER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_TOUCH_PASSTHROUGH_MANAGER_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/point.h"

namespace content {

class BrowserAccessibility;
class BrowserAccessibilityManager;
class RenderFrameHostImpl;
class SyntheticGestureController;
class SyntheticGestureTarget;
class SyntheticTouchDriver;

// Class that implements support for aria-touchpassthrough. When a screen
// reader is running on a system with a touch screen, the default mode
// is usually "touch exploration", where tapping or dragging on the screen
// describes the item under the finger but does not activate it. Typically
// double-tapping activates an object.
//
// However, there are some types of interfaces where this mode is inefficient
// or just doesn't work at all, for example a signature pad where you're
// supposed to draw your signature, a musical instrument, a game, or a
// keyboard/keypad. In these scenarios, it's desirable for touch events
// to get passed directly through.
//
// This class implements support for passing through touch events sent to
// a region with aria-touchpassthrough. Its input is the raw touch events
// from the touch exploration system. It does hit tests to determine whether
// those events fall within passthrough regions, and if so, generates touch
// events within those regions.
//
// Note that touch exploration is still running - the first touch within a
// passthrough region would still bring accessibility focus to that region and
// the screen reader would still announce it - the difference is that the
// touch events would also get passed through.
//
// Implementation:
//
// Determining whether each event falls within a passthrough region or not
// requires doing an asynchronous hit test. Because of this, a queue is
// created of events in the order they were received, and then the events
// are processed in order when ready.
class CONTENT_EXPORT TouchPassthroughManager {
 public:
  explicit TouchPassthroughManager(RenderFrameHostImpl* rfh);
  TouchPassthroughManager(const TouchPassthroughManager&) = delete;
  virtual ~TouchPassthroughManager();

  // These are the touch events sent by the touch exploration system.  When
  // aria-touchpassthrough is not present, these events are only used to give
  // accessibility focus to objects being touched and this class will do
  // nothing. However, if aria-touchpassthrough is present on the HTML element
  // under the finger, then this class will forward touch events to
  // the renderer.
  void OnTouchStart(const gfx::Point& point_in_frame_pixels);
  void OnTouchMove(const gfx::Point& point_in_frame_pixels);
  void OnTouchEnd();

 protected:
  // These are virtual protected for unit-testing.
  virtual void SendHitTest(
      const gfx::Point& point_in_frame_pixels,
      base::OnceCallback<void(BrowserAccessibilityManager* hit_manager,
                              int hit_node_id)> callback);
  virtual void CancelTouchesAndDestroyTouchDriver();
  virtual void SimulatePress(const gfx::Point& point,
                             const base::TimeTicks& time);
  virtual void SimulateMove(const gfx::Point& point,
                            const base::TimeTicks& time);
  virtual void SimulateCancel(const base::TimeTicks& time);
  virtual void SimulateRelease(const base::TimeTicks& time);

 private:
  enum EventType {
    kPress,
    kMove,
    kRelease,
  };

  // The main frame where touch events should be sent. Touch events
  // that target an iframe will be automatically forwarded.
  RenderFrameHostImpl* rfh_;

  // Classes needed to generate touch events.
  std::unique_ptr<SyntheticGestureController> gesture_controller_;
  SyntheticGestureTarget* gesture_target_ = nullptr;
  std::unique_ptr<SyntheticTouchDriver> touch_driver_;

  // A struct containing the information about touch start, move, and end
  // events, stored in a queue in the order they were received. The |pending|
  // field is set to true for events that are waiting on the result of a
  // hit test. The hit test populates |tree_id| and |node_id|.
  struct TouchPassthroughEvent {
    bool pending = false;
    EventType type;
    base::TimeTicks time;
    gfx::Point location;
    ui::AXTreeID tree_id;
    ui::AXNodeID node_id = 0;
  };

  // The queue is implemented as a map from sequential IDs to an event
  // struct.
  int next_event_id_ = 0;
  int current_event_id_ = 0;
  std::map<int, std::unique_ptr<TouchPassthroughEvent>> id_to_event_;

  // The current state of events being passed through, as the queue is
  // being processed in order.
  bool is_touch_down_ = false;
  ui::AXTreeID current_tree_id_;
  ui::AXNodeID current_node_id_;

  // Returns the event ID.
  int EnqueueEventOfType(EventType type,
                         bool pending,
                         const gfx::Point& point_in_frame_pixels);
  void CreateTouchDriverIfNeeded();
  void HitTestAndEnqueueEventOfType(EventType type,
                                    const gfx::Point& point_in_frame_pixels);
  void OnHitTestResult(int event_id,
                       BrowserAccessibilityManager* hit_manager,
                       int hit_node_id);
  BrowserAccessibility* GetTouchPassthroughNode(
      BrowserAccessibilityManager* hit_manager,
      int hit_node_id);
  void ProcessPendingEvents();
  void ProcessPendingEvent(std::unique_ptr<TouchPassthroughEvent> event);
  gfx::Point ToCSSPoint(gfx::Point point_in_frame_pixels);

  base::WeakPtrFactory<TouchPassthroughManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_TOUCH_PASSTHROUGH_MANAGER_H_
