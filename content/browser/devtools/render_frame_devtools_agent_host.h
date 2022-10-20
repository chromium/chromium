// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // This method is called when new frame is created for an emebedded page
  // (portal or fenced frame) or local root navigation.
  static scoped_refptr<RenderFrameDevToolsAgentHost>
  CreateForLocalRootOrEmbeddedPageNavigation(NavigationRequest* request);
  static scoped_refptr<RenderFrameDevToolsAgentHost> FindForDangling(
      FrameTreeNode* frame_tree_node);

  RenderFrameDevToolsAgentHost(const RenderFrameDevToolsAgentHost&) = delete;
  RenderFrameDevToolsAgentHost& operator=(const RenderFrameDevToolsAgentHost&) =
      delete;

  static void AttachToWebContents(WebContents* web_contents);

  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  void OnNavigationRequestWillBeSent(
      const NavigationRequest& navigation_request);
  void UpdatePortals();
  void DidCreateFencedFrame(FencedFrame* fenced_frame);

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

  absl::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id) override;
  absl::optional<network::CrossOriginOpenerPolicy> cross_origin_opener_policy(
      const std::string& id) override;

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

  // WebContentsObserver overrides.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(int frame_tree_node_id) override;
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

  bool ShouldAllowSession(DevToolsSession* session);

#if BUILDFLAG(IS_ANDROID)
  device::mojom::WakeLock* GetWakeLock();
#endif

  void UpdateResourceLoaderFactories();

#if BUILDFLAG(IS_ANDROID)
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
#endif

  std::unique_ptr<FrameAutoAttacher> auto_attacher_;
  // The active host we are talking to.
  RenderFrameHostImpl* frame_host_ = nullptr;
  base::flat_set<NavigationRequest*> navigation_requests_;
  bool render_frame_alive_ = false;
  bool render_frame_crashed_ = false;

  // The FrameTreeNode associated with this agent.
  FrameTreeNode* frame_tree_node_;
};

// Returns the ancestor FrameTreeNode* for which a RenderFrameDevToolsAgentHost
// should be created (i.e. the next local root).
FrameTreeNode* GetFrameTreeNodeAncestor(FrameTreeNode* frame_tree_node);

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
