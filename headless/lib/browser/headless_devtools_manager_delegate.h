// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_MANAGER_DELEGATE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_MANAGER_DELEGATE_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace headless {
class HeadlessBrowserImpl;

namespace protocol {
class HeadlessDevToolsSession;
}

class HeadlessDevToolsManagerDelegate
    : public content::DevToolsManagerDelegate {
 public:
  explicit HeadlessDevToolsManagerDelegate(
      base::WeakPtr<HeadlessBrowserImpl> browser);
  ~HeadlessDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation:
  void HandleCommand(content::DevToolsAgentHostClientChannel* channel,
                     base::span<const uint8_t> message,
                     NotHandledCallback callback) override;
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(
      const GURL& url,
      TargetType target_type) override;
  bool HasBundledFrontendResources() override;
  void ClientAttached(
      content::DevToolsAgentHostClientChannel* channel) override;
  void ClientDetached(
      content::DevToolsAgentHostClientChannel* channel) override;

  std::vector<content::BrowserContext*> GetBrowserContexts() override;
  content::BrowserContext* GetDefaultBrowserContext() override;
  content::BrowserContext* CreateBrowserContext() override;
  void DisposeBrowserContext(content::BrowserContext* context,
                             DisposeCallback callback) override;

 private:
  base::WeakPtr<HeadlessBrowserImpl> browser_;
  std::map<content::DevToolsAgentHostClientChannel*,
           std::unique_ptr<protocol::HeadlessDevToolsSession>>
      sessions_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_MANAGER_DELEGATE_H_
