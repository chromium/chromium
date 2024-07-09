// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_DEVTOOLS_FRAME_AUTO_ATTACHER_H_
#define CONTENT_BROWSER_DEVTOOLS_FRAME_AUTO_ATTACHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/devtools/protocol/target_auto_attacher.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/shared_storage_worklet_devtools_manager.h"
#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"

namespace content {

class DevToolsRendererChannel;
class FrameTree;
class NavigationRequest;
class RenderFrameHostImpl;
class ServiceWorkerDevToolsAgentHost;

class FrameAutoAttacher : public protocol::RendererAutoAttacherBase,
                          public ServiceWorkerDevToolsManager::Observer,
                          public DebuggableAuctionWorkletTracker::Observer,
                          public SharedStorageWorkletDevToolsManager::Observer {
 public:
  explicit FrameAutoAttacher(DevToolsRendererChannel* renderer_channel);
  ~FrameAutoAttacher() override;

  void SetRenderFrameHost(RenderFrameHostImpl* render_frame_host);
  void DidFinishNavigation(NavigationRequest* navigation_request);
  void AutoAttachToPage(FrameTree* frame_tree, bool wait_for_debugger_on_start);

 protected:
  // Base overrides.
  void UpdateAutoAttach(base::OnceClosure callback) override;

  // ServiceWorkerDevToolsManager::Observer implementation.
  void WorkerCreated(ServiceWorkerDevToolsAgentHost* host,
                     bool* should_pause_on_start) override;
  void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) override;

  // DebuggableAuctionWorkletTracker::Observer implementation.
  void AuctionWorkletCreated(DebuggableAuctionWorklet* worklet,
                             bool& should_pause_on_start) override;

  // SharedStorageWorkletDevToolsManager::Observer implementation.
  void SharedStorageWorkletCreated(SharedStorageWorkletDevToolsAgentHost* host,
                                   bool& should_pause_on_start) override;
  void SharedStorageWorkletDestroyed(
      SharedStorageWorkletDevToolsAgentHost* host) override;

  void ReattachServiceWorkers();
  void UpdateFrames();

 private:
  raw_ptr<RenderFrameHostImpl> render_frame_host_ = nullptr;
  bool observing_service_workers_ = false;
  bool observing_auction_worklets_ = false;
  bool observing_shared_storage_worklets_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_FRAME_AUTO_ATTACHER_H_
