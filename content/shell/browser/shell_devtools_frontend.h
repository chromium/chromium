// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_
#define CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/shell/browser/shell_devtools_bindings.h"

namespace content {

class Shell;
class WebContents;

class ShellDevToolsFrontend : public ShellDevToolsDelegate,
                              public WebContentsObserver {
 public:
  static ShellDevToolsFrontend* Show(WebContents* inspected_contents);

  ShellDevToolsFrontend(const ShellDevToolsFrontend&) = delete;
  ShellDevToolsFrontend& operator=(const ShellDevToolsFrontend&) = delete;

  void Activate();
  void InspectElementAt(int x, int y);
  void Close() override;

  Shell* frontend_shell() const { return frontend_shell_; }

  base::WeakPtr<ShellDevToolsFrontend> GetWeakPtr();

 private:
  // WebContentsObserver overrides
  void PrimaryMainDocumentElementAvailable() override;
  void WebContentsDestroyed() override;

  ShellDevToolsFrontend(Shell* frontend_shell, WebContents* inspected_contents);
  ~ShellDevToolsFrontend() override;
  raw_ptr<Shell> frontend_shell_;
  std::unique_ptr<ShellDevToolsBindings> devtools_bindings_;

  base::WeakPtrFactory<ShellDevToolsFrontend> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_
