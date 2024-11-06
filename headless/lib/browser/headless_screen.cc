// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_screen.h"

#include "base/check_deref.h"

namespace headless {

// static
HeadlessScreen* HeadlessScreen::Create(const gfx::Size& size,
                                       float scale_factor) {
  return new HeadlessScreen(gfx::Rect(size), scale_factor);
}

HeadlessScreen::~HeadlessScreen() = default;

gfx::Point HeadlessScreen::GetCursorScreenPoint() {
  return gfx::Point();
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

HeadlessScreen::HeadlessScreen(const gfx::Rect& bounds, float scale_factor)
    : natural_portrait_(bounds.height() > bounds.width()) {
  static int64_t synthesized_display_id = 2000;
  display::Display display(synthesized_display_id++);
  display.SetScaleAndBounds(scale_factor, bounds);
  ProcessDisplayChanged(display, /*is_primary=*/true);
}

// static
void HeadlessScreen::UpdateScreenSizeForScreenOrientation(
    display::mojom::ScreenOrientation screen_orientation) {
  auto& headless_screen =
      CHECK_DEREF(static_cast<HeadlessScreen*>(GetScreen()));
  headless_screen.UpdateScreenSizeForScreenOrientationImpl(screen_orientation);
}

void HeadlessScreen::UpdateScreenSizeForScreenOrientationImpl(
    display::mojom::ScreenOrientation screen_orientation) {
  display::Display display = GetPrimaryDisplay();

  bool needs_swap = false;
  switch (screen_orientation) {
    case display::mojom::ScreenOrientation::kUndefined:
      break;
    case display::mojom::ScreenOrientation::kPortraitPrimary:
      needs_swap = display.is_landscape();
      display.set_panel_rotation(IsNaturalPortrait()
                                     ? display::Display::ROTATE_0
                                     : display::Display::ROTATE_90);
      break;
    case display::mojom::ScreenOrientation::kPortraitSecondary:
      needs_swap = display.is_landscape();
      display.set_panel_rotation(IsNaturalPortrait()
                                     ? display::Display::ROTATE_180
                                     : display::Display::ROTATE_270);
      break;
    case display::mojom::ScreenOrientation::kLandscapePrimary:
      needs_swap = !display.is_landscape();
      display.set_panel_rotation(IsNaturalLandscape()
                                     ? display::Display::ROTATE_0
                                     : display::Display::ROTATE_90);
      break;
    case display::mojom::ScreenOrientation::kLandscapeSecondary:
      needs_swap = !display.is_landscape();
      display.set_panel_rotation(IsNaturalLandscape()
                                     ? display::Display::ROTATE_180
                                     : display::Display::ROTATE_270);
      break;
  }

  // Swap display width and height to change its orientation.
  if (needs_swap) {
    gfx::Rect bounds = display.bounds();
    int old_width = bounds.width();
    bounds.set_width(bounds.height());
    bounds.set_height(old_width);
    display.set_bounds(bounds);
  }

  // Update display even if there was no swap.
  ProcessDisplayChanged(display, /*is_primary=*/true);
}

}  // namespace headless
