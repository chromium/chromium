// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_AUTO_ATTACHER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_AUTO_ATTACHER_H_

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"

namespace content {

class DevToolsAgentHost;
class DevToolsAgentHostImpl;
class DevToolsRendererChannel;
class NavigationRequest;
class RenderFrameHostImpl;

namespace protocol {

class TargetAutoAttacher {
 public:
  class Delegate {
   public:
    virtual bool AutoAttach(DevToolsAgentHost* host,
                            bool waiting_for_debugger) = 0;
    virtual void AutoDetach(DevToolsAgentHost* host) = 0;
    virtual void SetAttachedTargetsOfType(
        const base::flat_set<scoped_refptr<DevToolsAgentHost>>& hosts,
        const std::string& type) = 0;

   protected:
    Delegate() = default;
    virtual ~Delegate() = default;
  };

  virtual ~TargetAutoAttacher();

  void SetDelegate(Delegate* delegate);
  virtual void SetRenderFrameHost(RenderFrameHostImpl* host);
  void SetAutoAttach(bool auto_attach,
                     bool wait_for_debugger_on_start,
                     base::OnceClosure callback);

  DevToolsAgentHost* AutoAttachToFrame(NavigationRequest* navigation_request);
  void ChildWorkerCreated(DevToolsAgentHostImpl* agent_host,
                          bool waiting_for_debugger);
  virtual void UpdatePortals();
  virtual void DidFinishNavigation(NavigationRequest* navigation_handle);
  bool auto_attach() const { return auto_attach_; }
  bool wait_for_debugger_on_start() const {
    return wait_for_debugger_on_start_;
  }

 protected:
  using Hosts = base::flat_set<scoped_refptr<DevToolsAgentHost>>;

  TargetAutoAttacher();

  Delegate* delegate() { return delegate_; }

  DevToolsAgentHost* AutoAttachToFrame(NavigationRequest* navigation_request,
                                       bool wait_for_debugger_on_start);
  virtual void UpdateAutoAttach(base::OnceClosure callback);

 private:
  Delegate* delegate_ = nullptr;

  bool auto_attach_ = false;
  bool wait_for_debugger_on_start_ = false;

  DISALLOW_COPY_AND_ASSIGN(TargetAutoAttacher);
};

class RendererAutoAttacherBase : public TargetAutoAttacher {
 public:
  explicit RendererAutoAttacherBase(DevToolsRendererChannel* renderer_channel);
  ~RendererAutoAttacherBase() override;

 protected:
  void UpdateAutoAttach(base::OnceClosure callback) override;

 private:
  DevToolsRendererChannel* const renderer_channel_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_AUTO_ATTACHER_H_
