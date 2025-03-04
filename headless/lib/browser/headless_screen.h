// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_

#include <string_view>

#include "base/containers/flat_map.h"
#include "ui/display/display.h"
#include "ui/display/mojom/screen_orientation.mojom-shared.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

class HeadlessScreen : public display::ScreenBase {
 public:
  static HeadlessScreen* Create(const gfx::Size& window_size,
                                std::string_view screen_info_spec);

  HeadlessScreen(const HeadlessScreen&) = delete;
  HeadlessScreen& operator=(const HeadlessScreen&) = delete;

  ~HeadlessScreen() override;

  // Updates screen size given the screen orientation.
  static void UpdateScreenSizeForScreenOrientation(
      int64_t display_id,
      display::mojom::ScreenOrientation screen_orientation);

  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;

  bool IsNaturalPortrait(int64_t display_id) const;
  bool IsNaturalLandscape(int64_t display_id) const;

 protected:
  HeadlessScreen(const gfx::Size& window_size,
                 std::string_view screen_info_spec);

  void UpdateScreenSizeForScreenOrientationImpl(
      int64_t display_id,
      display::mojom::ScreenOrientation screen_orientation);

  display::Display GetDisplayById(int64_t display_id);

  base::flat_map<int64_t, bool> is_natural_landscape_map_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_
