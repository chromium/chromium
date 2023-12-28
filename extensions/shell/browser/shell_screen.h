// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_SCREEN_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_SCREEN_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"

namespace aura {
class WindowTreeHost;
}

namespace gfx {
class Size;
}

namespace extensions {
class ShellDesktopControllerAura;

// A minimal Aura implementation of a screen. Scale factor is locked at 1.0.
// When running on a Linux desktop resizing the main window resizes the screen.
class ShellScreen : public display::ScreenBase,
                    public aura::WindowTreeHostObserver {
 public:
  // Creates a screen occupying |size| physical pixels. |desktop_controller|
  // can be null in tests.
  ShellScreen(ShellDesktopControllerAura* desktop_controller,
              const gfx::Size& size);

  ShellScreen(const ShellScreen&) = delete;
  ShellScreen& operator=(const ShellScreen&) = delete;

  ~ShellScreen() override;

  // aura::WindowTreeHostObserver overrides:
  void OnHostResized(aura::WindowTreeHost* host) override;

  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;

 private:
  const raw_ptr<ShellDesktopControllerAura> desktop_controller_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_SCREEN_H_
