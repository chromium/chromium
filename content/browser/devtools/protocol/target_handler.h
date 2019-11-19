// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_

#include <map>
#include <set>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/target.h"
#include "content/browser/devtools/protocol/target_auto_attacher.h"
#include "content/public/browser/devtools_agent_host_observer.h"

namespace content {

class DevToolsAgentHostImpl;
class DevToolsRendererChannel;
class DevToolsSession;
class NavigationHandle;
class NavigationThrottle;
class RenderFrameHostImpl;

namespace protocol {

class TargetHandler : public DevToolsDomainHandler,
                      public Target::Backend,
                      public DevToolsAgentHostObserver {
 public:
  enum class AccessMode {
    // Only setAutoAttach is supported. Any non-related target are not
    // accessible.
    kAutoAttachOnly,
    // Standard mode of operation: both auto-attach and discovery.
    kRegular,
    // This mode also allows advanced method like Target.exposeDevToolsProtocol,
    // which should not be exposed on a non-browser-wide connection.
    kBrowser
  };
  TargetHandler(AccessMode access_mode,
                const std::string& owner_target_id,
                DevToolsRendererChannel* renderer_channel,
                DevToolsSession* root_session);
  ~TargetHandler() override;

  static std::vector<TargetHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  void DidFinishNavigation();
  std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      NavigationHandle* navigation_handle);
  void UpdatePortals();

  // Domain implementation.
  Response SetDiscoverTargets(bool discover) override;
  void SetAutoAttach(bool auto_attach,
                     bool wait_for_debugger_on_start,
                     Maybe<bool> flatten,
                     Maybe<bool> window_open,
                     std::unique_ptr<SetAutoAttachCallback> callback) override;
  Response SetRemoteLocations(
      std::unique_ptr<protocol::Array<Target::RemoteLocation>>) override;
  Response AttachToTarget(const std::string& target_id,
                          Maybe<bool> flatten,
                          std::string* out_session_id) override;
  Response AttachToBrowserTarget(std::string* out_session_id) override;
  Response DetachFromTarget(Maybe<std::string> session_id,
                            Maybe<std::string> target_id) override;
  Response SendMessageToTarget(const std::string& message,
                               Maybe<std::string> session_id,
                               Maybe<std::string> target_id) override;
  Response GetTargetInfo(
      Maybe<std::string> target_id,
      std::unique_ptr<Target::TargetInfo>* target_info) override;
  Response ActivateTarget(const std::string& target_id) override;
  Response CloseTarget(const std::string& target_id,
                       bool* out_success) override;
  Response ExposeDevToolsProtocol(const std::string& target_id,
                                  Maybe<std::string> binding_name) override;
  Response CreateBrowserContext(std::string* out_context_id) override;
  void DisposeBrowserContext(
      const std::string& context_id,
      std::unique_ptr<DisposeBrowserContextCallback> callback) override;
  Response GetBrowserContexts(
      std::unique_ptr<protocol::Array<String>>* browser_context_ids) override;
  Response CreateTarget(const std::string& url,
                        Maybe<int> width,
                        Maybe<int> height,
                        Maybe<std::string> context_id,
                        Maybe<bool> enable_begin_frame_control,
                        Maybe<bool> new_window,
                        Maybe<bool> background,
                        std::string* out_target_id) override;
  Response GetTargets(
      std::unique_ptr<protocol::Array<Target::TargetInfo>>* target_infos)
      override;

 private:
  class Session;
  class Throttle;

  void AutoAttach(DevToolsAgentHost* host, bool waiting_for_debugger);
  void AutoDetach(DevToolsAgentHost* host);
  Response FindSession(Maybe<std::string> session_id,
                       Maybe<std::string> target_id,
                       Session** session);
  void ClearThrottles();
  void SetAutoAttachInternal(bool auto_attach,
                             bool wait_for_debugger_on_start,
                             bool flatten,
                             bool window_open,
                             base::OnceClosure callback);

  // DevToolsAgentHostObserver implementation.
  bool ShouldForceDevToolsAgentHostCreation() override;
  void DevToolsAgentHostCreated(DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostNavigated(DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDestroyed(DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostAttached(DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDetached(DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostCrashed(DevToolsAgentHost* agent_host,
                                base::TerminationStatus status) override;

  std::unique_ptr<Target::Frontend> frontend_;
  TargetAutoAttacher auto_attacher_;
  bool flatten_auto_attach_ = false;
  bool attach_to_window_open_ = false;
  bool discover_;
  std::map<std::string, std::unique_ptr<Session>> attached_sessions_;
  std::map<DevToolsAgentHost*, Session*> auto_attached_sessions_;
  std::set<DevToolsAgentHost*> reported_hosts_;
  AccessMode access_mode_;
  std::string owner_target_id_;
  DevToolsSession* root_session_;
  base::flat_set<Throttle*> throttles_;
  base::WeakPtrFactory<TargetHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TargetHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
