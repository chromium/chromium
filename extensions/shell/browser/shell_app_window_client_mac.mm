// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_app_window_client.h"

#include <memory>

#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/shell/browser/desktop_controller.h"
#import "extensions/shell/browser/shell_native_app_window_mac.h"

namespace extensions {

std::unique_ptr<NativeAppWindow> ShellAppWindowClient::CreateNativeAppWindow(
    AppWindow* window,
    AppWindow::CreateParams* params) {
  auto native_app_window =
      std::make_unique<ShellNativeAppWindowMac>(window, *params);
  DesktopController::instance()->AddAppWindow(
      window, native_app_window->GetNativeWindow());
  return native_app_window;
}

}  // namespace extensions
