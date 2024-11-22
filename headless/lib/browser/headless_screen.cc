// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_screen.h"

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "headless/lib/browser/headless_screen_info.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_list.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/rect.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace headless {

// static
HeadlessScreen* HeadlessScreen::Create(const gfx::Size& window_size,
                                       std::string_view screen_info_spec) {
  return new HeadlessScreen(window_size, screen_info_spec);
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
  // Mac always passes null gfx::NativeWindow, see https://crbug.com/380313546,
  // so this method currently only returns primary display on Macs.
#if defined(USE_AURA)
  if (window) {
    const gfx::Rect bounds = window->bounds();
    const display::Display* nearest_display =
        display::FindDisplayWithBiggestIntersection(display_list().displays(),
                                                    bounds);
    if (!nearest_display) {
      nearest_display = display::FindDisplayNearestPoint(
          display_list().displays(), bounds.CenterPoint());
    }
    if (nearest_display) {
      return *nearest_display;
    }
  }
#endif
  return GetPrimaryDisplay();
}

HeadlessScreen::HeadlessScreen(const gfx::Size& window_size,
                               std::string_view screen_info_spec) {
  std::vector<HeadlessScreenInfo> screen_info;
  if (screen_info_spec.empty()) {
    screen_info.push_back(
        HeadlessScreenInfo({.bounds = gfx::Rect(window_size)}));
  } else {
    screen_info = HeadlessScreenInfo::FromString(screen_info_spec).value();
    CHECK(!screen_info.empty());
  }

  bool is_primary = true;
  base::flat_set<int64_t> internal_display_ids;
  for (const auto& it : screen_info) {
    static int64_t synthesized_display_id = 2000;
    display::Display display(synthesized_display_id++);
    display.set_label(it.label);
    display.set_color_depth(it.color_depth);
    display.SetScaleAndBounds(it.device_pixel_ratio, it.bounds);
    if (it.is_internal) {
      internal_display_ids.insert(display.id());
    }
    ProcessDisplayChanged(display, is_primary);
    is_primary = false;
  }

  display::SetInternalDisplayIds(internal_display_ids);

  // Currently rotation always assumes the primary screen, however,
  // this needs to change when multiple screen support stabilizes.
  // See https://crbug.com/379076352.
  const HeadlessScreenInfo& primary_screen = screen_info[0];
  natural_portrait_ =
      primary_screen.bounds.height() > primary_screen.bounds.width();
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
