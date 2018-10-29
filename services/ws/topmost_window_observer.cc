// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/topmost_window_observer.h"

#include "services/ws/window_service.h"
#include "services/ws/window_service_delegate.h"
#include "services/ws/window_tree.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ws {

TopmostWindowObserver::TopmostWindowObserver(WindowTree* window_tree,
                                             mojom::MoveLoopSource source,
                                             aura::Window* initial_target)
    : window_tree_(window_tree),
      source_(source),
      last_target_(initial_target),
      env_(initial_target->env()) {
  std::set<ui::EventType> types;
  if (source == mojom::MoveLoopSource::MOUSE) {
    types.insert(ui::ET_MOUSE_MOVED);
    types.insert(ui::ET_MOUSE_DRAGGED);
    last_location_ = env_->last_mouse_location();
  } else {
    types.insert(ui::ET_TOUCH_MOVED);
    gfx::PointF point;
    ui::GestureRecognizer* gesture_recognizer = env_->gesture_recognizer();
    if (gesture_recognizer->GetLastTouchPointForTarget(last_target_, &point))
      last_location_ = gfx::Point(point.x(), point.y());
    ::wm::ConvertPointToScreen(last_target_, &last_location_);
  }
  env_->AddEventObserver(this, env_, types);
  UpdateTopmostWindows();
}

TopmostWindowObserver::~TopmostWindowObserver() {
  env_->RemoveEventObserver(this);
  if (topmost_)
    topmost_->RemoveObserver(this);
  if (real_topmost_ && topmost_ != real_topmost_)
    real_topmost_->RemoveObserver(this);
}

void TopmostWindowObserver::OnEvent(const ui::Event& event) {
  CHECK(event.IsLocatedEvent());
  last_target_ = static_cast<aura::Window*>(event.target());
  last_location_ = event.AsLocatedEvent()->location();
  ::wm::ConvertPointToScreen(last_target_, &last_location_);
  UpdateTopmostWindows();
}

void TopmostWindowObserver::OnWindowVisibilityChanged(aura::Window* window,
                                                      bool visible) {
  if (visible)
    return;
  if (!window->Contains(topmost_) && !window->Contains(real_topmost_))
    return;
  UpdateTopmostWindows();
}

void TopmostWindowObserver::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  UpdateTopmostWindows();
}

void TopmostWindowObserver::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  gfx::Rect screen_bounds = new_bounds;
  ::wm::ConvertRectToScreen(window->parent(), &screen_bounds);
  if (!screen_bounds.Contains(last_location_))
    UpdateTopmostWindows();
}

void TopmostWindowObserver::UpdateTopmostWindows() {
  std::set<aura::Window*> ignore;
  ignore.insert(last_target_);
  aura::Window* real_topmost = nullptr;
  aura::Window* topmost =
      window_tree_->window_service()->delegate()->GetTopmostWindowAtPoint(
          last_location_, ignore, &real_topmost);

  if (topmost == topmost_ && real_topmost == real_topmost_)
    return;

  // Since |topmost_| and |real_topmost_| could be same, updating observation
  // for those windows is really complicated. To simplify the logic, here always
  // removes this from the old windows and then adds to the new windows. This
  // means removing and adding can happen on the same window when |topmost_| or
  // |real_topmost_| are same. See topmost_window_observer_unittest.cc for the
  // corner cases of the updates.
  if (topmost_)
    topmost_->RemoveObserver(this);
  if (real_topmost_ && real_topmost_ != topmost_)
    real_topmost_->RemoveObserver(this);
  topmost_ = topmost;
  real_topmost_ = real_topmost;
  if (topmost_)
    topmost_->AddObserver(this);
  if (real_topmost_ && real_topmost_ != topmost_)
    real_topmost_->AddObserver(this);

  std::vector<aura::Window*> windows;
  if (real_topmost_)
    windows.push_back(real_topmost_);
  if (topmost_ && topmost_ != real_topmost_)
    windows.push_back(topmost_);
  window_tree_->SendTopmostWindows(windows);
}

}  // namespace ws
