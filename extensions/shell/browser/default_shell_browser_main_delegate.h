// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_DEFAULT_SHELL_BROWSER_MAIN_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_DEFAULT_SHELL_BROWSER_MAIN_DELEGATE_H_

#include "base/compiler_specific.h"
#include "extensions/shell/browser/shell_browser_main_delegate.h"

namespace extensions {

// A ShellBrowserMainDelegate that starts an application specified
// by the "--app" command line. This is used only in the browser process.
class DefaultShellBrowserMainDelegate : public ShellBrowserMainDelegate {
 public:
  DefaultShellBrowserMainDelegate();

  DefaultShellBrowserMainDelegate(const DefaultShellBrowserMainDelegate&) =
      delete;
  DefaultShellBrowserMainDelegate& operator=(
      const DefaultShellBrowserMainDelegate&) = delete;

  ~DefaultShellBrowserMainDelegate() override;

  // ShellBrowserMainDelegate:
  void Start(content::BrowserContext* context) override;
  void Shutdown() override;
  DesktopController* CreateDesktopController(
      content::BrowserContext* context) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_DEFAULT_SHELL_BROWSER_MAIN_DELEGATE_H_
