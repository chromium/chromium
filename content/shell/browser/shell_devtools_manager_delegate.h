// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_MANAGER_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_MANAGER_DELEGATE_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

class BrowserContext;

class ShellDevToolsManagerDelegate : public DevToolsManagerDelegate {
 public:
  static void StartHttpHandler(BrowserContext* browser_context);
  static void StopHttpHandler();
  static int GetHttpHandlerPort();

  explicit ShellDevToolsManagerDelegate(BrowserContext* browser_context);

  ShellDevToolsManagerDelegate(const ShellDevToolsManagerDelegate&) = delete;
  ShellDevToolsManagerDelegate& operator=(const ShellDevToolsManagerDelegate&) =
      delete;

  ~ShellDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation.
  BrowserContext* GetDefaultBrowserContext() override;
  scoped_refptr<DevToolsAgentHost> CreateNewTarget(const GURL& url,
                                                   bool for_tab) override;
  std::string GetDiscoveryPageHTML() override;
  bool HasBundledFrontendResources() override;
  void ClientAttached(
      content::DevToolsAgentHostClientChannel* channel) override;
  void ClientDetached(
      content::DevToolsAgentHostClientChannel* channel) override;

 private:
  raw_ptr<BrowserContext, DanglingUntriaged> browser_context_;
  base::flat_set<content::DevToolsAgentHostClient*> clients_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_MANAGER_DELEGATE_H_
