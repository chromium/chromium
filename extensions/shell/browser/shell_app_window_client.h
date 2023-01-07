// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_APP_WINDOW_CLIENT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_APP_WINDOW_CLIENT_H_

#include "base/compiler_specific.h"
#include "extensions/browser/app_window/app_window_client.h"

namespace extensions {

// app_shell's AppWindowClient implementation.
class ShellAppWindowClient : public AppWindowClient {
 public:
  ShellAppWindowClient();

  ShellAppWindowClient(const ShellAppWindowClient&) = delete;
  ShellAppWindowClient& operator=(const ShellAppWindowClient&) = delete;

  ~ShellAppWindowClient() override;

  // AppWindowClient overrides:
  AppWindow* CreateAppWindow(content::BrowserContext* context,
                             const Extension* extension) override;
  AppWindow* CreateAppWindowForLockScreenAction(
      content::BrowserContext* context,
      const Extension* extension,
      api::app_runtime::ActionType action) override;
  // Note that CreateNativeAppWindow is defined in separate (per-framework)
  // implementation files.
  std::unique_ptr<NativeAppWindow> CreateNativeAppWindow(
      AppWindow* window,
      AppWindow::CreateParams* params) override;
  void OpenDevToolsWindow(content::WebContents* web_contents,
                          base::OnceClosure callback) override;
  bool IsCurrentChannelOlderThanDev() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_APP_WINDOW_CLIENT_H_
