// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_

#include "ui/display/display.h"
#include "ui/display/mojom/screen_orientation.mojom-shared.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

class HeadlessScreen : public display::ScreenBase {
 public:
  // Creates a display::Screen of the specified size and scale factor.
  static HeadlessScreen* Create(const gfx::Size& size, float scale_factor);

  HeadlessScreen(const HeadlessScreen&) = delete;
  HeadlessScreen& operator=(const HeadlessScreen&) = delete;

  ~HeadlessScreen() override;

  // Updates screen size given the screen orientation.
  static void UpdateScreenSizeForScreenOrientation(
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

  bool IsNaturalPortrait() const { return natural_portrait_; }
  bool IsNaturalLandscape() const { return !natural_portrait_; }

 private:
  HeadlessScreen(const gfx::Rect& bounds, float scale_factor);

  void UpdateScreenSizeForScreenOrientationImpl(
      display::mojom::ScreenOrientation screen_orientation);

  bool natural_portrait_ = false;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_
