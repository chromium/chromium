// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_screen.h"

#include <stdint.h>

#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"

namespace headless {

// static
HeadlessScreen* HeadlessScreen::Create(const gfx::Size& size) {
  return new HeadlessScreen(gfx::Rect(size));
}

HeadlessScreen::~HeadlessScreen() = default;

gfx::Point HeadlessScreen::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool HeadlessScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow HeadlessScreen::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  return nullptr;
}

gfx::NativeWindow HeadlessScreen::GetLocalProcessWindowAtPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) {
  return nullptr;
}

display::Display HeadlessScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return GetPrimaryDisplay();
}

HeadlessScreen::HeadlessScreen(const gfx::Rect& screen_bounds) {
  static int64_t synthesized_display_id = 2000;
  display::Display display(synthesized_display_id++);
  display.SetScaleAndBounds(1.0f, screen_bounds);
  ProcessDisplayChanged(display, true /* is_primary */);
}

}  // namespace headless
