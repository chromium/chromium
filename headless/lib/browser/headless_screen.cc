// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_screen.h"

#include <optional>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/notimplemented.h"
#include "components/headless/display_util/headless_display_util.h"
#include "components/headless/screen_info/headless_screen_info.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_list.h"
#include "ui/display/util/display_util.h"

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
  return gfx::NativeWindow();
}

gfx::NativeWindow HeadlessScreen::GetLocalProcessWindowAtPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) {
  return gfx::NativeWindow();
}

display::Display HeadlessScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  // On Windows and Linux native window is abstracted by aura::Window so we can
  // use its bounds to find the nearest display.
#if defined(USE_AURA)
  if (window) {
    const gfx::Rect bounds = window->GetBoundsInScreen();
    if (std::optional<display::Display> display =
            GetDisplayFromScreenRect(display_list().displays(), bounds)) {
      return display.value();
    }
  }
#else
  NOTIMPLEMENTED_LOG_ONCE();
#endif  // #if defined(USE_AURA)

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

    if (!it.work_area_insets.IsEmpty()) {
      display.UpdateWorkAreaFromInsets(it.work_area_insets);
    }

    if (it.rotation) {
      CHECK(display::Display::IsValidRotation(it.rotation));
      display.SetRotationAsDegree(it.rotation);
    }

    if (it.is_internal) {
      internal_display_ids.insert(display.id());
    }

    is_natural_landscape_map_.insert({display.id(), display.is_landscape()});
    ProcessDisplayChanged(display, is_primary);
    is_primary = false;
  }

  display::SetInternalDisplayIds(internal_display_ids);
}

bool HeadlessScreen::IsNaturalPortrait(int64_t display_id) const {
  return !IsNaturalLandscape(display_id);
}

bool HeadlessScreen::IsNaturalLandscape(int64_t display_id) const {
  auto it = is_natural_landscape_map_.find(display_id);
  return it != is_natural_landscape_map_.end() ? it->second : true;
}

// static
void HeadlessScreen::UpdateScreenSizeForScreenOrientation(
    int64_t display_id,
    display::mojom::ScreenOrientation screen_orientation) {
  auto& headless_screen =
      CHECK_DEREF(static_cast<HeadlessScreen*>(GetScreen()));
  headless_screen.UpdateScreenSizeForScreenOrientationImpl(display_id,
                                                           screen_orientation);
}

void HeadlessScreen::UpdateScreenSizeForScreenOrientationImpl(
    int64_t display_id,
    display::mojom::ScreenOrientation screen_orientation) {
  display::Display display = GetDisplayById(display_id);

  bool needs_swap = false;
  switch (screen_orientation) {
    case display::mojom::ScreenOrientation::kUndefined:
      break;
    case display::mojom::ScreenOrientation::kPortraitPrimary:
      needs_swap = display.is_landscape();
      display.set_panel_rotation(IsNaturalPortrait(display_id)
                                     ? display::Display::ROTATE_0
                                     : display::Display::ROTATE_90);
      break;
    case display::mojom::ScreenOrientation::kPortraitSecondary:
      needs_swap = display.is_landscape();
      display.set_panel_rotation(IsNaturalPortrait(display_id)
                                     ? display::Display::ROTATE_180
                                     : display::Display::ROTATE_270);
      break;
    case display::mojom::ScreenOrientation::kLandscapePrimary:
      needs_swap = !display.is_landscape();
      display.set_panel_rotation(IsNaturalLandscape(display_id)
                                     ? display::Display::ROTATE_0
                                     : display::Display::ROTATE_90);
      break;
    case display::mojom::ScreenOrientation::kLandscapeSecondary:
      needs_swap = !display.is_landscape();
      display.set_panel_rotation(IsNaturalLandscape(display_id)
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
  bool is_primary = display.id() == GetPrimaryDisplay().id();
  ProcessDisplayChanged(display, is_primary);
}

display::Display HeadlessScreen::GetDisplayById(int64_t display_id) {
  auto it = display_list().FindDisplayById(display_id);
  if (it != display_list().displays().end()) {
    return *it;
  }

  return GetPrimaryDisplay();
}

}  // namespace headless
