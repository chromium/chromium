// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_AUTO_ATTACHER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_AUTO_ATTACHER_H_

#include "base/containers/flat_set.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

class DevToolsAgentHostImpl;
class DevToolsRendererChannel;
class NavigationRequest;
class RenderFrameHostImpl;

namespace protocol {

class TargetAutoAttacher : public ServiceWorkerDevToolsManager::Observer {
 public:
  // Second parameter is |waiting_for_debugger|, returns whether it succeeded.
  using AttachCallback =
      base::RepeatingCallback<void(DevToolsAgentHost*, bool)>;
  using DetachCallback = base::RepeatingCallback<void(DevToolsAgentHost*)>;

  TargetAutoAttacher(AttachCallback attach_callback,
                     DetachCallback detach_callback,
                     DevToolsRendererChannel* renderer_channel);
  ~TargetAutoAttacher() override;

  void SetRenderFrameHost(RenderFrameHostImpl* host);
  void SetAutoAttach(bool auto_attach,
                     bool wait_for_debugger_on_start,
                     base::OnceClosure callback);

  void UpdatePortals();
  void UpdateServiceWorkers();
  void AgentHostClosed(DevToolsAgentHost* host);

  bool ShouldThrottleFramesNavigation();
  DevToolsAgentHost* AutoAttachToFrame(NavigationRequest* navigation_request);
  void ChildWorkerCreated(DevToolsAgentHostImpl* agent_host,
                          bool waiting_for_debugger);

 private:
  using Hosts = base::flat_set<scoped_refptr<DevToolsAgentHost>>;

  void ReattachServiceWorkers(bool waiting_for_debugger);
  void ReattachTargetsOfType(const Hosts& new_hosts,
                             const std::string& type,
                             bool waiting_for_debugger);

  // ServiceWorkerDevToolsManager::Observer implementation.
  void WorkerCreated(ServiceWorkerDevToolsAgentHost* host,
                     bool* should_pause_on_start) override;
  void WorkerVersionInstalled(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerVersionDoomed(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) override;

  void UpdateFrames();

  AttachCallback attach_callback_;
  DetachCallback detach_callback_;
  DevToolsRendererChannel* renderer_channel_;
  RenderFrameHostImpl* render_frame_host_;

  bool auto_attach_;
  bool wait_for_debugger_on_start_;
  bool auto_attaching_service_workers_ = false;

  Hosts auto_attached_hosts_;

  DISALLOW_COPY_AND_ASSIGN(TargetAutoAttacher);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_AUTO_ATTACHER_H_
