// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_DEVTOOLS_FRAME_AUTO_ATTACHER_H_
#define CONTENT_BROWSER_DEVTOOLS_FRAME_AUTO_ATTACHER_H_

#include "base/callback.h"
#include "content/browser/devtools/protocol/target_auto_attacher.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"

namespace content {

class DevToolsRendererChannel;
class NavigationRequest;
class RenderFrameHostImpl;
class ServiceWorkerDevToolsAgentHost;

class FrameAutoAttacher : public protocol::RendererAutoAttacherBase,
                          public ServiceWorkerDevToolsManager::Observer {
 public:
  explicit FrameAutoAttacher(DevToolsRendererChannel* renderer_channel);
  ~FrameAutoAttacher() override;

  void SetRenderFrameHost(RenderFrameHostImpl* render_frame_host);
  void DidFinishNavigation(NavigationRequest* navigation_request);
  void UpdatePortals();

 protected:
  // Base overrides.
  void UpdateAutoAttach(base::OnceClosure callback) override;

  // ServiceWorkerDevToolsManager::Observer implementation.
  void WorkerCreated(ServiceWorkerDevToolsAgentHost* host,
                     bool* should_pause_on_start) override;
  void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) override;

  void ReattachServiceWorkers();
  void UpdateFrames();

 private:
  RenderFrameHostImpl* render_frame_host_ = nullptr;
  bool observing_service_workers_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_FRAME_AUTO_ATTACHER_H_
