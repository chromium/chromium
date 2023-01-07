// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_screen.h"

#include <stdint.h>
#include <vector>

#include "base/check.h"
#include "extensions/shell/browser/root_window_controller.h"
#include "extensions/shell/browser/shell_desktop_controller_aura.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace extensions {
namespace {

const int64_t kDisplayId = 0;

}  // namespace

ShellScreen::ShellScreen(ShellDesktopControllerAura* desktop_controller,
                         const gfx::Size& size)
    : desktop_controller_(desktop_controller) {
  DCHECK(!size.IsEmpty());

  // Screen is positioned at (0,0).
  display::Display display(kDisplayId);
  gfx::Rect bounds(size);
  display.SetScaleAndBounds(1.0f, bounds);
  ProcessDisplayChanged(display, true /* is_primary */);
}

ShellScreen::~ShellScreen() {
  DCHECK(!desktop_controller_ || !desktop_controller_->GetPrimaryHost())
      << "WindowTreeHost not closed before destroying ShellScreen";
}

void ShellScreen::OnHostResized(aura::WindowTreeHost* host) {
  // Based on ash::WindowTreeHostManager.
  display::Display display = GetDisplayNearestWindow(host->window());
  display.SetSize(host->GetBoundsInPixels().size());
  display_list().UpdateDisplay(display);
}

gfx::Point ShellScreen::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool ShellScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow ShellScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return desktop_controller_->GetPrimaryHost()
      ->window()
      ->GetEventHandlerForPoint(point);
}

display::Display ShellScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return GetPrimaryDisplay();
}

}  // namespace extensions
