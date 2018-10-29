// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_TOPMOST_WINDOW_OBSERVER_H_
#define SERVICES_WS_TOPMOST_WINDOW_OBSERVER_H_

#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Env;
class Window;
}

namespace ws {
class WindowTree;

// TopmostWindowObserver is used to track the topmost window under the cursor or
// touch. It watches a type of located events (mouse or touch) and checks the
// windows under the mouse cursor or touch location.

// TODO(mukai): support multiple displays.
class COMPONENT_EXPORT(WINDOW_SERVICE) TopmostWindowObserver
    : public ui::EventObserver,
      public aura::WindowObserver {
 public:
  // |source| determines the type of the event, and |initial_target| is the
  // initial target of the event. This will report the topmost window under the
  // cursor to |window_tree|.
  TopmostWindowObserver(WindowTree* window_tree,
                        mojom::MoveLoopSource source,
                        aura::Window* initial_target);
  ~TopmostWindowObserver() override;

 private:
  friend class TopmostWindowObserverTest;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // Looks up the windows at |location_in_screen| and sends the windows to the
  // client if it's updated.
  void UpdateTopmostWindows();

  WindowTree* window_tree_;

  // The type of the events which should be obsered.
  mojom::MoveLoopSource source_;

  // The last target of the event. This is remembered since sometimes the client
  // wants to see the topmost window excluding the event target (i.e. dragging
  // window).
  aura::Window* last_target_;

  // The last location of the event.
  gfx::Point last_location_;

  // The topmost window excluding |last_target_| under the cursor/touch.
  aura::Window* topmost_ = nullptr;

  // The topmost window (including |last_target_|) under the cursor/touch.
  aura::Window* real_topmost_ = nullptr;

  aura::Env* env_;

  DISALLOW_COPY_AND_ASSIGN(TopmostWindowObserver);
};

}  // namespace ws

#endif  // SERVICES_WS_TOPMOST_WINDOW_OBSERVER_H_
