// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mock_screen.h"

namespace extensions {

MockScreen::MockScreen() {
  for (int i = 0; i < 4; i++) {
    gfx::Rect bounds(0, 0, 1280, 720);
    gfx::Rect work_area(0, 0, 960, 720);
    display::Display display(i, bounds);
    display.set_work_area(work_area);
    displays_.push_back(display);
  }
}

MockScreen::~MockScreen() = default;

gfx::Point MockScreen::GetCursorScreenPoint() {
  return gfx::Point();
}

bool MockScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  return false;
}

gfx::NativeWindow MockScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return nullptr;
}

gfx::NativeWindow MockScreen::GetLocalProcessWindowAtPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) {
  return nullptr;
}

int MockScreen::GetNumDisplays() const {
  return static_cast<int>(displays_.size());
}

const std::vector<display::Display>& MockScreen::GetAllDisplays() const {
  return displays_;
}

display::Display MockScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display::Display(0);
}

display::Display MockScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return display::Display(0);
}

display::Display MockScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return display::Display(0);
}

display::Display MockScreen::GetPrimaryDisplay() const {
  return displays_[0];
}

}  // namespace extensions
