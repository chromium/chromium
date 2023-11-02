// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_

#include "base/compiler_specific.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"

namespace gfx {
class Rect;
}

namespace headless {

class HeadlessScreen : public display::ScreenBase {
 public:
  // Creates a display::Screen of the specified size (physical pixels).
  static HeadlessScreen* Create(const gfx::Size& size);

  HeadlessScreen(const HeadlessScreen&) = delete;
  HeadlessScreen& operator=(const HeadlessScreen&) = delete;

  ~HeadlessScreen() override;

 protected:
  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;

 private:
  explicit HeadlessScreen(const gfx::Rect& screen_bounds);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_H_
