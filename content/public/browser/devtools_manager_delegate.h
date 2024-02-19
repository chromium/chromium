// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host.h"
#include "url/gurl.h"

namespace content {

class DevToolsAgentHostClientChannel;
class RenderFrameHost;
class WebContents;

class CONTENT_EXPORT DevToolsManagerDelegate {
 public:
  // Opens the inspector for |agent_host|.
  virtual void Inspect(DevToolsAgentHost* agent_host);

  // Activates the associated inspector for `agent_host` if there
  // is one.
  virtual void Activate(DevToolsAgentHost* agent_host);

  // Returns DevToolsAgentHost type to use for given |web_contents| target.
  virtual std::string GetTargetType(WebContents* web_contents);

  // Returns DevToolsAgentHost title to use for given |web_contents| target.
  virtual std::string GetTargetTitle(WebContents* web_contents);

  // Returns DevToolsAgentHost title to use for given |web_contents| target.
  virtual std::string GetTargetDescription(WebContents* web_contents);

  // Returns whether embedder allows to inspect given |rfh|.
  virtual bool AllowInspectingRenderFrameHost(RenderFrameHost* rfh);

  // Chrome Devtools Protocol Target type to use. Before MPArch frame targets
  // were used, which correspond to the primary outermost frame in the
  // WebContents. With prerender and other MPArch features, there could be
  // multiple outermost frames per WebContents. To make debugging them possible,
  // DevTools protocol introduced a tab target which is a parent of all
  // outermost frames in the WebContents (not that we refer to it as a tab even
  // though tabs only exist in //chrome because CDP calls it that way). For
  // details see
  // https://docs.google.com/document/d/14aeiC_zga2SS0OXJd6eIFj8N0o5LGwUpuqa4L8NKoR4/
  enum TargetType { kFrame, kTab };

  // Returns all targets embedder would like to report as debuggable remotely.
  virtual DevToolsAgentHost::List RemoteDebuggingTargets(TargetType target_type);

  // Creates new inspectable target given the |url|.
  virtual scoped_refptr<DevToolsAgentHost> CreateNewTarget(
      const GURL& url,
      TargetType target_type);

  // Get all live browser contexts created by CreateBrowserContext() method.
  virtual std::vector<BrowserContext*> GetBrowserContexts();

  // Get default browser context. May return null if not supported.
  virtual BrowserContext* GetDefaultBrowserContext();

  // Create new browser context. May return null if not supported or not
  // possible. Delegate must take ownership of the created browser context, and
  // may destroy it at will.
  virtual BrowserContext* CreateBrowserContext();

  // Dispose browser context that was created with |CreateBrowserContext|
  // method.
  using DisposeCallback = base::OnceCallback<void(bool, const std::string&)>;
  virtual void DisposeBrowserContext(BrowserContext* context,
                                     DisposeCallback callback);

  // Called when a new client is attached/detached.
  virtual void ClientAttached(DevToolsAgentHostClientChannel* channel);
  virtual void ClientDetached(DevToolsAgentHostClientChannel* channel);

  // Call callback if command was not handled.
  using NotHandledCallback =
      base::OnceCallback<void(base::span<const uint8_t>)>;
  virtual void HandleCommand(DevToolsAgentHostClientChannel* channel,
                             base::span<const uint8_t> message,
                             NotHandledCallback callback);

  // Should return discovery page HTML that should list available tabs
  // and provide attach links.
  virtual std::string GetDiscoveryPageHTML();

  // Returns whether frontend resources are bundled within the binary.
  virtual bool HasBundledFrontendResources();

  // Makes browser target easily discoverable for remote debugging.
  // This should only return true when remote debugging endpoint is not
  // accessible by the web (for example in Chrome for Android where it is
  // exposed via UNIX named socket) or when content/ embedder is built for
  // running in the controlled environment (for example a special build for
  // the Lab testing). If you want to return true here, please get security
  // clearance from the devtools owners.
  virtual bool IsBrowserTargetDiscoverable();

  virtual ~DevToolsManagerDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
