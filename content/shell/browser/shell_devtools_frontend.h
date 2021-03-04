// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_
#define CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/shell/browser/shell_devtools_bindings.h"

namespace content {

class Shell;
class WebContents;

class ShellDevToolsFrontend : public ShellDevToolsDelegate,
                              public WebContentsObserver {
 public:
  static ShellDevToolsFrontend* Show(WebContents* inspected_contents);

  void Activate();
  void InspectElementAt(int x, int y);
  void Close() override;

  Shell* frontend_shell() const { return frontend_shell_; }

 private:
  // WebContentsObserver overrides
  void DocumentAvailableInMainFrame() override;
  void WebContentsDestroyed() override;

  ShellDevToolsFrontend(Shell* frontend_shell, WebContents* inspected_contents);
  ~ShellDevToolsFrontend() override;
  Shell* frontend_shell_;
  std::unique_ptr<ShellDevToolsBindings> devtools_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ShellDevToolsFrontend);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_
