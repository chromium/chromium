// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_MAC_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_MAC_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "ui/display/screen.h"

namespace extensions {

class AppWindow;
class AppWindowClient;

// A simple implementation of the app_shell DesktopController for Mac Cocoa.
// Only currently supports one app window (unlike Aura).
class ShellDesktopControllerMac : public DesktopController {
 public:
  ShellDesktopControllerMac();

  ShellDesktopControllerMac(const ShellDesktopControllerMac&) = delete;
  ShellDesktopControllerMac& operator=(const ShellDesktopControllerMac&) =
      delete;

  ~ShellDesktopControllerMac() override;

  // DesktopController:
  void AddAppWindow(AppWindow* app_window, gfx::NativeWindow window) override;
  void CloseAppWindows() override;

 private:
  std::unique_ptr<AppWindowClient> app_window_client_;

  // The desktop only supports a single app window.
  // TODO(yoz): Support multiple app windows, as we do in Aura.
  raw_ptr<AppWindow, DanglingUntriaged>
      app_window_;  // NativeAppWindow::Close() deletes this.

  display::ScopedNativeScreen screen_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_MAC_H_
