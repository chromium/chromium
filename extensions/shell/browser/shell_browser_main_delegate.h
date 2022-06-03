// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_MAIN_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_MAIN_DELEGATE_H_

namespace content {
class BrowserContext;
}

namespace extensions {

class DesktopController;

class ShellBrowserMainDelegate {
 public:
  virtual ~ShellBrowserMainDelegate() {}

  // Called to start an application after all initialization processes that are
  // necessary to run apps are completed.
  virtual void Start(content::BrowserContext* context) = 0;

  // Called after the main message looop has stopped, but before
  // other services such as BrowserContext / extension system are shut down.
  virtual void Shutdown() = 0;

  // Creates the ShellDesktopControllerAura instance to initialize the root
  // window and window manager. Subclass may return its subclass to customize
  // the window manager.
  virtual DesktopController* CreateDesktopController(
      content::BrowserContext* context) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_MAIN_DELEGATE_H_
