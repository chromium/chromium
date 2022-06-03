// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_CLIENT_H_
#define EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_CLIENT_H_

#include "base/callback_forward.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/api/app_runtime.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

class Extension;
class NativeAppWindow;

// Sets up global state for the app window system. Should be Set() once in each
// process. This should be implemented by the client of the app window system.
// TODO(hashimoto): Move some functions to ExtensionsClient.
class AppWindowClient {
 public:
  virtual ~AppWindowClient() {}

  // Creates a new AppWindow for the app in |extension| for |context|.
  // Caller takes ownership.
  virtual AppWindow* CreateAppWindow(content::BrowserContext* context,
                                     const Extension* extension) = 0;

  // Creates a new AppWindow for |extension| as lock screen action handler for
  // |action|. This should be used to create windows that will be used to handle
  // |action| on lock screen. The method should return null window if the app
  // was not requested to handle the action.
  virtual AppWindow* CreateAppWindowForLockScreenAction(
      content::BrowserContext* context,
      const Extension* extension,
      api::app_runtime::ActionType action) = 0;

  // Creates a new extensions::NativeAppWindow for |window|.
  virtual NativeAppWindow* CreateNativeAppWindow(
      AppWindow* window,
      AppWindow::CreateParams* params) = 0;

  // Opens DevTools window and runs the callback.
  virtual void OpenDevToolsWindow(content::WebContents* web_contents,
                                  base::OnceClosure callback) = 0;

  // Returns true if the current channel is older than dev.
  virtual bool IsCurrentChannelOlderThanDev() = 0;

  // Return the app window client.
  static AppWindowClient* Get();

  // Initialize the app window system with this app window client.
  static void Set(AppWindowClient* client);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_CLIENT_H_
