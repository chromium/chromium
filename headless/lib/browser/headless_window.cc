// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_window.h"

#include "ui/display/screen.h"

namespace headless {

HeadlessWindow::HeadlessWindow(HeadlessWindowDelegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate);
}

HeadlessWindow::~HeadlessWindow() = default;

void HeadlessWindow::SetVisible(bool visible) {
  UpdateVisible(visible);
}

void HeadlessWindow::SetBounds(const gfx::Rect& bounds) {
  UpdateBounds(bounds);
}

void HeadlessWindow::SetWindowState(HeadlessWindowState window_state) {
  if (window_state == window_state_) {
    return;
  }

  // Ignore transitions to states other than normal while in full screen state.
  // See http://crbug.com/429423225.
  if (window_state_ == HeadlessWindowState::kFullscreen &&
      window_state != HeadlessWindowState::kNormal) {
    return;
  }

  bool set_visible = false;

  switch (window_state) {
    case HeadlessWindowState::kNormal:
      RestoreWindowBounds();
      if (window_state_ == HeadlessWindowState::kMinimized) {
        set_visible = true;
      }
      break;
    case HeadlessWindowState::kMinimized:
      // Minimized windows are hidden but retain their normal size and position.
      if (visible_) {
        UpdateVisible(/*visible*/ false);
      }
      RestoreWindowBounds();
      break;
    case HeadlessWindowState::kMaximized:
      if (window_state_ != HeadlessWindowState::kFullscreen) {
        restored_bounds_ = bounds_;
      }
      ZoomWindowBounds();
      set_visible = true;
      break;
    case HeadlessWindowState::kFullscreen:
      if (window_state_ != HeadlessWindowState::kMaximized) {
        restored_bounds_ = bounds_;
      }
      ZoomWindowBounds();
      set_visible = true;
      break;
  }

  UpdateWindowState(window_state);

  // Make window visible after the window state has been changed.
  if (set_visible && !visible_) {
    UpdateVisible(/*visible*/ true);
  }
}

void HeadlessWindow::ZoomWindowBounds() {
  const gfx::Rect zoomed_bounds =
      display::Screen::Get()->GetDisplayMatching(bounds_).work_area();
  UpdateBounds(zoomed_bounds);
}

void HeadlessWindow::RestoreWindowBounds() {
  if (restored_bounds_) {
    const gfx::Rect restored_bounds = restored_bounds_.value();
    restored_bounds_.reset();
    UpdateBounds(restored_bounds);
  }
}

void HeadlessWindow::UpdateVisible(bool visible) {
  if (visible != visible_) {
    visible_ = visible;
    delegate_->OnVisibilityChanged();
  }
}

void HeadlessWindow::UpdateBounds(const gfx::Rect& bounds) {
  if (bounds != bounds_) {
    const gfx::Rect old_bounds = bounds_;
    bounds_ = bounds;
    delegate_->OnBoundsChanged({old_bounds});
  }
}

void HeadlessWindow::UpdateWindowState(HeadlessWindowState window_state) {
  if (window_state != window_state_) {
    const HeadlessWindowState old_window_state = window_state_;
    window_state_ = window_state;
    delegate_->OnWindowStateChanged(old_window_state);
  }
}

}  // namespace headless
