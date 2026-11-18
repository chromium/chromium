// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WEB_CONTENTS_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_WEB_CONTENTS_DEVTOOLS_AGENT_HOST_H_

#include <optional>

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class FrameTreeNode;

class CONTENT_EXPORT WebContentsDevToolsAgentHost
    : public DevToolsAgentHostImpl,
      public WebContentsObserver {
 public:
  // Returns appropriate agent host for given Web Contents
  static WebContentsDevToolsAgentHost* GetFor(WebContents* web_contents);
  // Similar to GetFor(), but creates a host if it doesn't exist yet.
  static WebContentsDevToolsAgentHost* GetOrCreateFor(
      WebContents* web_contents);

  static bool IsDebuggerAttached(WebContents* web_contents);

  WebContentsDevToolsAgentHost(const WebContentsDevToolsAgentHost&) = delete;
  WebContentsDevToolsAgentHost& operator=(const WebContentsDevToolsAgentHost&) =
      delete;

  static void AddAllAgentHosts(DevToolsAgentHost::List* result);

  // DevToolsAgentHostImpl overrides.
  protocol::TargetAutoAttacher* auto_attacher() override;

  // Instrumentation methods
  void WillInitiatePrerender(FrameTreeNode* ftn);
  // TODO(caseq): do we need more specific signals here?
  void UpdateChildFrameTrees(bool update_target_info);
  void InspectElement(RenderFrameHost* frame_host, int x, int y) override;

 private:
  class AutoAttacher;

  explicit WebContentsDevToolsAgentHost(WebContents* wc);
  ~WebContentsDevToolsAgentHost() override;

  void InnerAttach(WebContents* web_contents);
  void InnerDetach();

  // DevToolsAgentHost overrides.
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* web_contents) override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  std::string GetParentId() override;
  std::string GetOpenerId() override;
  std::string GetOpenerFrameId() override;
  bool CanAccessOpener() override;
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetDescription() override;
  GURL GetURL() override;
  GURL GetFaviconURL() override;
  bool Activate() override;
  void Reload() override;

  bool Close() override;
  base::TimeTicks GetLastActivityTime() override;

  std::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id) override;
  std::optional<network::CrossOriginOpenerPolicy> cross_origin_opener_policy(
      const std::string& id) override;

  // DevToolsAgentHostImpl overrides.
  std::string GetSubtype() override;
  DevToolsSession::Mode GetSessionMode() override;
  bool AttachSession(DevToolsSession* session) override;

  // WebContentsObserver overrides.
  void WebContentsDestroyed() override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void FrameDeleted(FrameTreeNodeId frame_tree_node_id) override;
  void WasDiscarded() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // Returns true if the tab has a history entry, but the primary main frame
  // has no document for it. This occurs after a discard, after session restore
  // without load, and after a renderer crash. The value stays true until a new
  // document commits, also if a navigation does not commit (for example, a 204
  // response). A new tab that has not navigated is not unloaded.
  bool ComputeIsUnloaded();

  // Sets `is_unloaded_` to ComputeIsUnloaded(), and notifies observers if the
  // value changes.
  void UpdateIsUnloaded();

  DevToolsAgentHostImpl* GetPrimaryFrameAgent();
  scoped_refptr<DevToolsAgentHost> GetOrCreatePrimaryFrameAgent();

  // The method returns a pointer retaining this. Once the pointer goes
  // out of scope, this may be destroyed.
  [[nodiscard]] scoped_refptr<WebContentsDevToolsAgentHost>
  RevalidateSessionAccess();

  // The last value of ComputeIsUnloaded() that was sent to observers.
  bool is_unloaded_ = false;
  std::unique_ptr<AutoAttacher> const auto_attacher_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WEB_CONTENTS_DEVTOOLS_AGENT_HOST_H_
