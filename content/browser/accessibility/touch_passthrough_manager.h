// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_TOUCH_PASSTHROUGH_MANAGER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_TOUCH_PASSTHROUGH_MANAGER_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/point.h"

namespace base {

class TimeTicks;

}  // namespace base

namespace ui {

class AXPlatformTreeManager;

}  // namespace ui

namespace content {

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
// When an onTouchStart is received, we must first do an async hit test to
// determine if it's within a touch passthrough region. If it is, then all
// touch events are passed through until onTouchEnd.
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
      base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                              ui::AXNodeID hit_node_id)> callback);
  virtual void CancelTouchesAndDestroyTouchDriver();
  virtual void SimulatePress(const gfx::Point& point,
                             const base::TimeTicks& time);
  virtual void SimulateMove(const gfx::Point& point,
                            const base::TimeTicks& time);
  virtual void SimulateRelease(const base::TimeTicks& time);

 private:
  // The main frame where touch events should be sent. Touch events
  // that target an iframe will be automatically forwarded.
  raw_ptr<RenderFrameHostImpl> rfh_;

  // Classes needed to generate touch events.
  std::unique_ptr<SyntheticGestureController> gesture_controller_;
  raw_ptr<SyntheticGestureTarget> gesture_target_ = nullptr;
  std::unique_ptr<SyntheticTouchDriver> touch_driver_;

  // Keeps track of whether or not touch is down, regardless of whether or
  // not we're passing through.
  bool is_touch_down_ = false;

  // Whether or not we're passing through touch events until the next
  // touch up.
  bool is_passthrough_ = false;

  // An incrementing ID so that hit test callbacks that arrive late can
  // be ignored.
  int hit_test_id_ = 0;

  void CreateTouchDriverIfNeeded();
  void OnHitTestResult(int event_id,
                       base::TimeTicks event_time,
                       gfx::Point location,
                       ui::AXPlatformTreeManager* hit_manager,
                       ui::AXNodeID hit_node_id);
  bool IsTouchPassthroughNode(ui::AXPlatformTreeManager* hit_manager,
                              ui::AXNodeID hit_node_id);
  gfx::Point ToCSSPoint(gfx::Point point_in_frame_pixels);

  base::WeakPtrFactory<TouchPassthroughManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_TOUCH_PASSTHROUGH_MANAGER_H_
