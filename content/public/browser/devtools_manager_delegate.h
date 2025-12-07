// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host.h"
#include "url/gurl.h"

namespace content {

class DevToolsAgentHostClientChannel;
class RenderFrameHost;
class WebContents;

class CONTENT_EXPORT DevToolsManagerDelegate {
 public:
  // Options for opening the DevTools window.
  struct CONTENT_EXPORT DevToolsOptions {
    // The panel to open when the DevTools window is opened.
    const std::optional<std::string> panel_id;

   public:
    DevToolsOptions();
    DevToolsOptions(const DevToolsOptions& other);
    explicit DevToolsOptions(std::optional<std::string> panel_id);
    ~DevToolsOptions();
  };

  // When the remote debugging server is started in the approval mode, the
  // AcceptDebugging() method is called for each connection. The result of
  // that call determines whether the connection is allowed or denied.
  enum class AcceptConnectionResult {
    // The connection was denied by the user.
    kDeny,
    // The connection was allowed.
    kAllow,
  };

  // Opens the inspector for |agent_host|.
  virtual void Inspect(DevToolsAgentHost* agent_host);

  // Gets the DevTools window for |agent_host| if exists.
  virtual scoped_refptr<DevToolsAgentHost> GetDevToolsAgentHost(
      DevToolsAgentHost* agent_host);

  // Opens the DevTools window for |agent_host|.
  virtual scoped_refptr<DevToolsAgentHost> OpenDevTools(
      DevToolsAgentHost* agent_host,
      const DevToolsManagerDelegate::DevToolsOptions& devtools_options);

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

  // Returns classification override for the `web_contents` about whether it
  // should be treated as a Tab target.
  // Returns:
  //   - std::nullopt if the delegate doesn't want to override the default
  //     behavior, which means a kTypeTab target for WebContents and a frame
  //     target for its main frame will be reported when enumerating all
  //     targets.
  //   - true if the target should be treated as a Tab with no parent even if
  //     it is an inner WebContents that might be treated as kTypeGuest by
  //     default. A kTypeTab target for WebContents and a frame target with
  //     type determined by GetTargetType() will be reported when enumerating
  //     all targets.
  //   - false if the target should not be treated as a Tab, which means no
  //     target is reported for the WebContents when enumerating all targets.
  //     Its main frame target will be reported as normal.
  virtual std::optional<bool> ShouldReportAsTabTarget(
      WebContents* web_contents);

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
  // |new_window| is currently only used on Android - Desktop platforms handle
  // window creation elsewhere. Note that there is also a limit to the number of
  // windows that may be opened on Android, and this parameter may be ignored if
  // new windows cannot be opened.
  virtual scoped_refptr<DevToolsAgentHost>
  CreateNewTarget(const GURL& url, TargetType target_type, bool new_window);

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

  using AcceptCallback =
      base::OnceCallback<void(DevToolsManagerDelegate::AcceptConnectionResult)>;
  // Called when a new debugging connection is received if the
  // remote debugging server is started in approval mode.
  virtual void AcceptDebugging(AcceptCallback);
  // Called when the number of active WebSocket connections changes
  // The embedder can use this information to update the UI.
  virtual void SetActiveWebSocketConnections(size_t count);

  virtual ~DevToolsManagerDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
