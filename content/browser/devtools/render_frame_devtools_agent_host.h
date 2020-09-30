// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

#if defined(OS_ANDROID)
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "ui/android/view_android.h"
#endif  // OS_ANDROID

namespace cc {
class RenderFrameMetadata;
}

namespace content {

class BrowserContext;
class DevToolsFrameTraceRecorder;
class FrameTreeNode;
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
  // Returns appropriate agent host for given render frame host, traversing
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
  static bool ShouldCreateDevToolsForHost(RenderFrameHost* rfh);

  // This method is called when new frame is created during cross process
  // navigation.
  static scoped_refptr<DevToolsAgentHost> CreateForCrossProcessNavigation(
      NavigationRequest* request);
  static scoped_refptr<DevToolsAgentHost> FindForDangling(
      FrameTreeNode* frame_tree_node);

  static void WebContentsCreated(WebContents* web_contents);

#if defined(OS_ANDROID)
  static void SignalSynchronousSwapCompositorFrame(
      RenderFrameHost* frame_host,
      const cc::RenderFrameMetadata& frame_metadata);
#endif

  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  void OnNavigationRequestWillBeSent(
      const NavigationRequest& navigation_request);

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

  RenderFrameHostImpl* GetFrameHostForTesting() { return frame_host_; }

 private:
  friend class DevToolsAgentHost;

  static void UpdateRawHeadersAccess(RenderFrameHostImpl* old_rfh,
                                     RenderFrameHostImpl* new_rfh);

  RenderFrameDevToolsAgentHost(FrameTreeNode*, RenderFrameHostImpl*);
  ~RenderFrameDevToolsAgentHost() override;

  // DevToolsAgentHostImpl overrides.
  bool AttachSession(DevToolsSession* session, bool acquire_wake_lock) override;
  void DetachSession(DevToolsSession* session) override;
  void InspectElement(RenderFrameHost* frame_host, int x, int y) override;
  void UpdateRendererChannel(bool force) override;

  // WebContentsObserver overrides.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(RenderFrameHost* rfh) override;
  void RenderFrameDeleted(RenderFrameHost* rfh) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void OnPageScaleFactorChanged(float page_scale_factor) override;

  // RenderProcessHostObserver overrides.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  bool IsChildFrame();

  void DestroyOnRenderFrameGone();
  void UpdateFrameHost(RenderFrameHostImpl* frame_host);
  void SetFrameTreeNode(FrameTreeNode* frame_tree_node);
  void ChangeFrameHostAndObservedProcess(RenderFrameHostImpl* frame_host);
  void UpdateFrameAlive();

  bool ShouldAllowSession(DevToolsSession* session);

#if defined(OS_ANDROID)
  device::mojom::WakeLock* GetWakeLock();
  void SynchronousSwapCompositorFrame(
      const cc::RenderFrameMetadata& frame_metadata);
#endif

  void UpdateResourceLoaderFactories();

#if defined(OS_ANDROID)
  std::unique_ptr<DevToolsFrameTraceRecorder> frame_trace_recorder_;
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
#endif

  // The active host we are talking to.
  RenderFrameHostImpl* frame_host_ = nullptr;
  base::flat_set<NavigationRequest*> navigation_requests_;
  bool render_frame_alive_ = false;
  bool render_frame_crashed_ = false;

  // The FrameTreeNode associated with this agent.
  FrameTreeNode* frame_tree_node_;

  double page_scale_factor_ = 1;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameDevToolsAgentHost);
};

// Returns the ancestor FrameTreeNode* for which a RenderFrameDevToolsAgentHost
// should be created (i.e. the next local root).
FrameTreeNode* GetFrameTreeNodeAncestor(FrameTreeNode* frame_tree_node);

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
