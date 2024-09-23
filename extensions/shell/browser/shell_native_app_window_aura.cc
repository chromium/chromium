// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_native_app_window_aura.h"

#include "content/public/browser/web_contents.h"
#include "extensions/shell/browser/shell_desktop_controller_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/window_util.h"

namespace extensions {

ShellNativeAppWindowAura::ShellNativeAppWindowAura(
    AppWindow* app_window,
    const AppWindow::CreateParams& params)
    : ShellNativeAppWindow(app_window, params) {
  // TODO(yoz): We might have to duplicate this for mac.
  gfx::Rect bounds =
      params.GetInitialWindowBounds(GetFrameInsets(), GetWindowRadii());
  bool position_specified =
      bounds.x() != AppWindow::BoundsSpecification::kUnspecifiedPosition &&
      bounds.y() != AppWindow::BoundsSpecification::kUnspecifiedPosition;
  if (!position_specified)
    bounds.set_origin(GetBounds().origin());
  SetBounds(bounds);
}

ShellNativeAppWindowAura::~ShellNativeAppWindowAura() {
}

bool ShellNativeAppWindowAura::IsActive() const {
  // Even though app_shell only supports a single app window, there might be
  // some sort of system-level dialog open and active.
  aura::Window* window = GetNativeWindow();
  return window && wm::IsActiveWindow(window);
}

gfx::NativeWindow ShellNativeAppWindowAura::GetNativeWindow() const {
  return app_window()->web_contents()->GetNativeView();
}

gfx::Rect ShellNativeAppWindowAura::GetBounds() const {
  return GetNativeWindow()->GetBoundsInScreen();
}

void ShellNativeAppWindowAura::Show() {
  GetNativeWindow()->Show();
}

void ShellNativeAppWindowAura::Hide() {
  GetNativeWindow()->Hide();
}

bool ShellNativeAppWindowAura::IsVisible() const {
  return GetNativeWindow()->IsVisible();
}

void ShellNativeAppWindowAura::Activate() {
  aura::Window* window = GetNativeWindow();
  if (window)
    wm::ActivateWindow(window);
}

void ShellNativeAppWindowAura::Deactivate() {
  aura::Window* window = GetNativeWindow();
  if (window)
    wm::DeactivateWindow(window);
}

void ShellNativeAppWindowAura::SetBounds(const gfx::Rect& bounds) {
  // When this is called during window creation, set the bounds naively.
  if (!GetNativeWindow()->GetRootWindow()) {
    GetNativeWindow()->SetBounds(bounds);
    return;
  }

  // ShellDesktopControllerAura will take care of moving the AppWindow to
  // another display if necessary.
  static_cast<ShellDesktopControllerAura*>(DesktopController::instance())
      ->SetWindowBoundsInScreen(app_window(), bounds);
}

gfx::Size ShellNativeAppWindowAura::GetContentMinimumSize() const {
  // Content fills the desktop and cannot be resized.
  return GetNativeWindow()->GetBoundsInRootWindow().size();
}

gfx::Size ShellNativeAppWindowAura::GetContentMaximumSize() const {
  return GetContentMinimumSize();
}

}  // namespace extensions
