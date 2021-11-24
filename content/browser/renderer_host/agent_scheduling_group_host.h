// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_

#include <stdint.h>

#include "base/containers/id_map.h"
#include "base/supports_user_data.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_export.h"
#include "content/common/renderer.mojom-forward.h"
#include "content/common/state_transitions.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/content_features.h"
#include "content/services/shared_storage_worklet/public/mojom/shared_storage_worklet_service.mojom-forward.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

namespace content {

class AgentSchedulingGroupHostFactory;
class BrowserMessageFilter;
class RenderProcessHost;
class SiteInstanceGroup;

// Browser-side host of an AgentSchedulingGroup, used for
// AgentSchedulingGroup-bound messaging. AgentSchedulingGroup is Blink's unit of
// scheduling and performance isolation, which is the only way to obtain
// ordering guarantees between different Mojo (associated) interfaces and legacy
// IPC messages.
//
// AgentSchedulingGroups can be assigned at various granularities, as coarse as
// process-wide or as specific as SiteInstanceGroup. There cannot be more than
// one AgentSchedulingGroup per SiteInstanceGroup without breaking IPC ordering
// for RenderWidgetHost. (SiteInstanceGroups themselves can be tuned to contain
// one or more SiteInstances, depending on platform and policy.)
//
// An AgentSchedulingGroupHost is stored as (and owned by) UserData on the
// RenderProcessHost.
class CONTENT_EXPORT AgentSchedulingGroupHost
    : public base::SupportsUserData,
      public RenderProcessHostObserver,
      public IPC::Listener,
      public mojom::AgentSchedulingGroupHost,
      public mojom::RouteProvider,
      public blink::mojom::AssociatedInterfaceProvider {
 public:
  // Get the appropriate AgentSchedulingGroupHost for the given
  // `site_instance_group` and `process`. Depending on the value of
  // `features::kMBIModeParam`, there may be a single AgentSchedulingGroupHost
  // per RenderProcessHost, or a single one per SiteInstanceGroup, which may
  // lead to multiple AgentSchedulingGroupHosts per RenderProcessHost. This
  // method will never return null.
  static AgentSchedulingGroupHost* GetOrCreate(
      const SiteInstanceGroup& site_instance_group,
      RenderProcessHost& process);

  // Should not be called explicitly. Use `CreateIfNeeded()` instead.
  explicit AgentSchedulingGroupHost(RenderProcessHost& process);
  ~AgentSchedulingGroupHost() override;

  void AddFilter(BrowserMessageFilter* filter);

  RenderProcessHost* GetProcess();
  // Ensure that the process this AgentSchedulingGroupHost belongs to is alive.
  // Returns |false| if any part of the initialization failed.
  bool Init();

  int32_t id_for_debugging() const { return id_for_debugging_; }

  // IPC and mojo messages to be forwarded to the RenderProcessHost, for now. In
  // the future they will be handled directly by the AgentSchedulingGroupHost.
  // IPC:
  IPC::ChannelProxy* GetChannel();
  // This is marked virtual for use in tests by `MockAgentSchedulingGroupHost`.
  virtual bool Send(IPC::Message* message);
  void AddRoute(int32_t routing_id, IPC::Listener* listener);
  void RemoveRoute(int32_t routing_id);

  // Mojo:
  mojom::RouteProvider* GetRemoteRouteProvider();
  void CreateFrame(mojom::CreateFrameParamsPtr params);
  void CreateView(mojom::CreateViewParamsPtr params);
  void DestroyView(int32_t routing_id);
  void CreateFrameProxy(
      const blink::RemoteFrameToken& token,
      int32_t routing_id,
      const absl::optional<blink::FrameToken>& opener_frame_token,
      int32_t view_routing_id,
      int32_t parent_routing_id,
      blink::mojom::TreeScopeType tree_scope_type,
      blink::mojom::FrameReplicationStatePtr replicated_state,
      const base::UnguessableToken& devtools_frame_token,
      mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces);
  void CreateSharedStorageWorkletService(
      mojo::PendingReceiver<
          shared_storage_worklet::mojom::SharedStorageWorkletService> receiver);

  void ReportNoBinderForInterface(const std::string& error);

  static void set_agent_scheduling_group_host_factory_for_testing(
      AgentSchedulingGroupHostFactory* asgh_factory);
  static AgentSchedulingGroupHostFactory*
  get_agent_scheduling_group_host_factory_for_testing();

  // mojom::AgentSchedulingGroupHost overrides.
  void DidUnloadRenderFrame(const blink::LocalFrameToken& frame_token) override;

 private:
  enum class LifecycleState {
    // Just instantiated, no route assigned yet.
    kNewborn,

    // Bound mojo connection to the renderer.
    kBound,

    // Intermediate state between renderer process exit and rebinding mojo
    // connections.
    kRenderProcessExited,

    // RenderProcessHost is destroyed, and `this` is pending for deletion.
    // kRenderProcessHostDestroyed is the terminal state of the state machine.
    kRenderProcessHostDestroyed,
  };
  friend StateTransitions<LifecycleState>;
  friend std::ostream& operator<<(std::ostream& os, LifecycleState state);

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnBadMessageReceived(const IPC::Message& message) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // mojom::RouteProvider
  void GetRoute(
      int32_t routing_id,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
          receiver) override;

  // blink::mojom::AssociatedInterfaceProvider
  void GetAssociatedInterface(
      const std::string& name,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
          receiver) override;

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  void ResetIPC();
  void SetUpIPC();

  void SetState(LifecycleState state);

  IPC::Listener* GetListener(int32_t routing_id);

  static int32_t GetNextID();

  // The RenderProcessHost this AgentSchedulingGroup is assigned to.
  RenderProcessHost& process_;

  int32_t id_for_debugging_{GetNextID()};

  // This AgentSchedulingGroup's legacy IPC channel. Will only be used in
  // `features::MBIMode::kEnabledPerRenderProcessHost` or
  // `features::MBIMode::kEnabledPerSiteInstance` mode.
  std::unique_ptr<IPC::ChannelProxy> channel_;

  // Map of registered IPC listeners.
  base::IDMap<IPC::Listener*> listener_map_;

  // Remote stub of `mojom::AgentSchedulingGroup`, used for sending calls to the
  // (renderer-side) `AgentSchedulingGroup`.
  mojo::AssociatedRemote<mojom::AgentSchedulingGroup> mojo_remote_;

  // Implementation of `mojom::AgentSchedulingGroupHost`, used for responding to
  // calls from the (renderer-side) `AgentSchedulingGroup`.
  mojo::AssociatedReceiver<mojom::AgentSchedulingGroupHost> receiver_;

  // BrowserInterfaceBroker implementation through which this
  // AgentSchedulingGroupHost exposes ASG-scoped Mojo services to the
  // currently active document.
  //
  // The interfaces that can be requested from this broker are defined in the
  // content/browser/browser_interface_binders.cc file, in the functions which
  // take a `AgentSchedulingGroupHost*` parameter.
  //
  // TODO(crbug.com/1132752): Enable capability control for Prerender2 by
  // initializing BrowserInterfaceBrokerImpl with a non-null
  // MojoBinderPolicyApplier pointer.
  BrowserInterfaceBrokerImpl<AgentSchedulingGroupHost,
                             AgentSchedulingGroupHost*>
      broker_{this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  // The `mojom::RouteProvider` mojo pair to setup
  // `blink::AssociatedInterfaceProvider` routes between this and the
  // renderer-side `AgentSchedulingGroup`.
  mojo::AssociatedRemote<mojom::RouteProvider> remote_route_provider_;
  mojo::AssociatedReceiver<mojom::RouteProvider> route_provider_receiver_{this};

  // The `blink::mojom::AssociatedInterfaceProvider` receiver set that *all*
  // renderer-side `blink::AssociatedInterfaceProvider` objects own a remote to.
  // `AgentSchedulingGroupHost` will be responsible for routing each associated
  // interface request to the appropriate renderer host object.
  mojo::AssociatedReceiverSet<blink::mojom::AssociatedInterfaceProvider,
                              int32_t>
      associated_interface_provider_receivers_;

  LifecycleState state_{LifecycleState::kNewborn};
};

std::ostream& operator<<(std::ostream& os,
                         AgentSchedulingGroupHost::LifecycleState state);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_
