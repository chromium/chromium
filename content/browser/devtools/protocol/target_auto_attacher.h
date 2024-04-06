// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_AUTO_ATTACHER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_AUTO_ATTACHER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"

namespace content {

class DevToolsAgentHost;
class DevToolsAgentHostImpl;
class DevToolsRendererChannel;
class NavigationHandle;
class NavigationRequest;
class NavigationThrottle;
class RenderFrameDevToolsAgentHost;

namespace protocol {

class TargetAutoAttacher {
 public:
  class Client : public base::CheckedObserver {
   public:
    virtual bool AutoAttach(TargetAutoAttacher* source,
                            DevToolsAgentHost* host,
                            bool waiting_for_debugger) = 0;
    virtual void AutoDetach(TargetAutoAttacher* source,
                            DevToolsAgentHost* host) = 0;
    virtual void SetAttachedTargetsOfType(
        TargetAutoAttacher* source,
        const base::flat_set<scoped_refptr<DevToolsAgentHost>>& hosts,
        const std::string& type) = 0;
    virtual void AutoAttacherDestroyed(TargetAutoAttacher* auto_attacher) = 0;
    virtual std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
        TargetAutoAttacher* auto_attacher,
        NavigationHandle* navigation_handle) = 0;
    virtual void TargetInfoChanged(DevToolsAgentHost* host) = 0;

   protected:
    Client() = default;
    ~Client() override = default;
  };

  TargetAutoAttacher(const TargetAutoAttacher&) = delete;
  TargetAutoAttacher& operator=(const TargetAutoAttacher&) = delete;

  virtual ~TargetAutoAttacher();

  void AddClient(Client* client,
                 bool wait_for_debugger_on_start,
                 base::OnceClosure callback);
  void RemoveClient(Client* client);
  void UpdateWaitForDebuggerOnStart(Client* client,
                                    bool wait_for_debugger_on_start,
                                    base::OnceClosure callback);

  void AppendNavigationThrottles(
      NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<NavigationThrottle>>* throttles);

  scoped_refptr<RenderFrameDevToolsAgentHost> HandleNavigation(
      NavigationRequest* navigation_request,
      bool wait_for_debugger_on_start);

 protected:
  using Hosts = base::flat_set<scoped_refptr<DevToolsAgentHost>>;

  TargetAutoAttacher();

  bool auto_attach() const;
  bool wait_for_debugger_on_start() const;

  virtual void UpdateAutoAttach(base::OnceClosure callback);

  void DispatchAutoAttach(DevToolsAgentHost* host, bool waiting_for_debugger);
  void DispatchAutoDetach(DevToolsAgentHost* host);
  void DispatchSetAttachedTargetsOfType(
      const base::flat_set<scoped_refptr<DevToolsAgentHost>>& hosts,
      const std::string& type);
  void DispatchTargetInfoChanged(DevToolsAgentHost* host);

 private:
  base::ObserverList<Client, false, true> clients_;
  base::flat_set<raw_ptr<Client, CtnExperimental>>
      clients_requesting_wait_for_debugger_;
};

class RendererAutoAttacherBase : public TargetAutoAttacher {
 public:
  explicit RendererAutoAttacherBase(DevToolsRendererChannel* renderer_channel);
  ~RendererAutoAttacherBase() override;

 protected:
  void UpdateAutoAttach(base::OnceClosure callback) override;
  void ChildWorkerCreated(DevToolsAgentHostImpl* agent_host,
                          bool waiting_for_debugger);

 private:
  const raw_ptr<DevToolsRendererChannel> renderer_channel_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_AUTO_ATTACHER_H_
