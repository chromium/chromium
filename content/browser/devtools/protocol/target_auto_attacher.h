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

class TargetAutoAttacher {
 public:
  class Delegate {
   public:
    virtual void AutoAttach(DevToolsAgentHost* host,
                            bool waiting_for_debugger) = 0;
    virtual void AutoDetach(DevToolsAgentHost* host) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  static std::unique_ptr<TargetAutoAttacher> CreateForBrowser();
  static std::unique_ptr<TargetAutoAttacher> CreateForServiceWorker(
      DevToolsRendererChannel* channel);
  static std::unique_ptr<TargetAutoAttacher> CreateForWorker(
      DevToolsRendererChannel* channel);
  static std::unique_ptr<TargetAutoAttacher> CreateForFrame(
      DevToolsRendererChannel* channel);

  virtual ~TargetAutoAttacher();

  void SetDelegate(Delegate* delegate);
  virtual void SetRenderFrameHost(RenderFrameHostImpl* host);
  void SetAutoAttach(bool auto_attach,
                     bool wait_for_debugger_on_start,
                     base::OnceClosure callback);

  void AgentHostClosed(DevToolsAgentHost* host);
  bool ShouldThrottleFramesNavigation() const;
  void AttachToAgentHost(DevToolsAgentHost* host);
  DevToolsAgentHost* AutoAttachToFrame(NavigationRequest* navigation_request);
  void ChildWorkerCreated(DevToolsAgentHostImpl* agent_host,
                          bool waiting_for_debugger);
  virtual void UpdatePortals();
  virtual void DidFinishNavigation(NavigationRequest* navigation_handle);

 protected:
  using Hosts = base::flat_set<scoped_refptr<DevToolsAgentHost>>;

  TargetAutoAttacher();

  bool auto_attach() const { return auto_attach_; }
  bool wait_for_debugger_on_start() const {
    return wait_for_debugger_on_start_;
  }
  Delegate* delegate() { return delegate_; }

  DevToolsAgentHost* AutoAttachToFrame(NavigationRequest* navigation_request,
                                       bool wait_for_debugger_on_start);
  void ReattachTargetsOfType(const Hosts& new_hosts,
                             const std::string& type,
                             bool waiting_for_debugger);

  virtual void UpdateAutoAttach(base::OnceClosure callback);

  Hosts auto_attached_hosts_;

 private:
  void AttachToAgentHost(DevToolsAgentHost* host,
                         bool wait_for_debugger_on_start);

  Delegate* delegate_ = nullptr;

  bool auto_attach_ = false;
  bool wait_for_debugger_on_start_ = false;

  DISALLOW_COPY_AND_ASSIGN(TargetAutoAttacher);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_AUTO_ATTACHER_H_
