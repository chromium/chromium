// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "ui/android/view_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

class BrowserContext;
class FencedFrame;
class FrameTreeNode;
class FrameAutoAttacher;
class NavigationRequest;
class RenderFrameHostImpl;

class CONTENT_EXPORT RenderFrameDevToolsAgentHost
    : public DevToolsAgentHostImpl,
      private WebContentsObserver,
      private RenderProcessHostObserver {
 public:
  // Returns true when DevTools was ever attached to any RenderFrameHost.
  // TODO(crbug.com/40264958): Remove this method after the experiment
  // associated with the bug entry.
  static bool WasEverAttachedToAnyFrame();

  static bool IsDebuggerAttached(WebContents* web_contents);

  static void AddAllAgentHosts(DevToolsAgentHost::List* result);

  // Returns appropriate agent host for given frame tree node, traversing
  // up to local root as needed.
  static DevToolsAgentHostImpl* GetFor(FrameTreeNode* frame_tree_node);
  // Returns appropriate agent host for given RenderFrameHost, traversing
  // up to local root as needed. This will have an effect different from
  // calling the above overload as GetFor(rfh->frame_tree_node()) when
  // given RFH is a pending local root.
  static DevToolsAgentHostImpl* GetFor(RenderFrameHostImpl* rfh);

  // Similar to GetFor(), but creates a host if it doesn't exist yet.
  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(
      FrameTreeNode* frame_tree_node);

  // Whether the RFH passed may have associated DevTools agent host
  // (i.e. the specified RFH is a local root). This does not indicate
  // whether DevToolsAgentHost has actually been created.
  static bool ShouldCreateDevToolsForHost(RenderFrameHostImpl* rfh);

  // This method is called when new frame is created for an embedded page
  // (fenced frame) or local root navigation.
  static scoped_refptr<RenderFrameDevToolsAgentHost>
  CreateForLocalRootOrEmbeddedPageNavigation(NavigationRequest* request);
  static scoped_refptr<RenderFrameDevToolsAgentHost> FindForDangling(
      FrameTreeNode* frame_tree_node);

  RenderFrameDevToolsAgentHost(const RenderFrameDevToolsAgentHost&) = delete;
  RenderFrameDevToolsAgentHost& operator=(const RenderFrameDevToolsAgentHost&) =
      delete;

  static void AttachToWebContents(WebContents* web_contents);
  static bool ShouldAllowSession(RenderFrameHost* frame_host,
                                 DevToolsSession* session);

  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  void OnNavigationRequestWillBeSent(
      const NavigationRequest& navigation_request);
  void DidCreateFencedFrame(FencedFrame* fenced_frame);

  // DevToolsAgentHost overrides.
  // TODO(caseq): remove (Dis)connectWebContents() on frame targets once
  // front-end uses tab target mode.
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
  std::optional<std::vector<network::mojom::ContentSecurityPolicyHeader>>
  content_security_policy(const std::string& id) override;

  // This is used to enable compatibility shims, including disabling some
  // features that are incompatible with older clients.
  bool HasSessionsWithoutTabTargetSupport() const;

  void SetFrameTreeNode(FrameTreeNode* frame_tree_node);

  RenderFrameHostImpl* GetFrameHostForTesting() { return frame_host_; }

 private:
  friend class DevToolsAgentHost;
  friend class RenderFrameDevToolsAgentHostFencedFrameBrowserTest;

  static void UpdateRawHeadersAccess(RenderFrameHostImpl* rfh);

  RenderFrameDevToolsAgentHost(FrameTreeNode*, RenderFrameHostImpl*);
  ~RenderFrameDevToolsAgentHost() override;

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;
  void DetachSession(DevToolsSession* session) override;
  void InspectElement(RenderFrameHost* frame_host, int x, int y) override;
  void UpdateRendererChannel(bool force) override;
  protocol::TargetAutoAttacher* auto_attacher() override;
  std::string GetSubtype() override;
  RenderProcessHost* GetProcessHost() override;
  void MainThreadDebuggerPaused() override;
  void MainThreadDebuggerResumed() override;

  // WebContentsObserver overrides.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(FrameTreeNodeId frame_tree_node_id) override;
  void RenderFrameDeleted(RenderFrameHost* rfh) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // RenderProcessHostObserver overrides.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  bool IsChildFrame();

  void DestroyOnRenderFrameGone();
  void UpdateFrameHost(RenderFrameHostImpl* frame_host);
  void ChangeFrameHostAndObservedProcess(RenderFrameHostImpl* frame_host);
  void UpdateFrameAlive();

#if BUILDFLAG(IS_ANDROID)
  device::mojom::WakeLock* GetWakeLock();
#endif

  void UpdateResourceLoaderFactories();

#if BUILDFLAG(IS_ANDROID)
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
#endif

  std::unique_ptr<FrameAutoAttacher> auto_attacher_;
  // The active host we are talking to.
  raw_ptr<RenderFrameHostImpl> frame_host_ = nullptr;
  base::flat_set<raw_ptr<NavigationRequest, CtnExperimental>>
      navigation_requests_;
  bool render_frame_alive_ = false;
  bool render_frame_crashed_ = false;

  // TODO(crbug.com/40269649): Remove these fields once we collect enough
  // data.
  bool is_debugger_paused_ = false;
  bool is_debugger_pause_situation_recorded_ = false;

  // The FrameTreeNode associated with this agent.
  raw_ptr<FrameTreeNode> frame_tree_node_;
};

// Returns the ancestor FrameTreeNode* for which a RenderFrameDevToolsAgentHost
// should be created (i.e. the next local root).
FrameTreeNode* GetFrameTreeNodeAncestor(FrameTreeNode* frame_tree_node);

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
