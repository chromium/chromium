// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_

#include <stdint.h>

#include "base/containers/id_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/safe_ref.h"
#include "base/state_transitions.h"
#include "base/supports_user_data.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/renderer.mojom-forward.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/content_features.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom-forward.h"

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
      public mojom::AgentSchedulingGroupHost {
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

  // Should not be called explicitly. Use `GetOrCreate()` instead.
  explicit AgentSchedulingGroupHost(RenderProcessHost& process);
  ~AgentSchedulingGroupHost() override;

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  void AddFilter(BrowserMessageFilter* filter);
#endif

  RenderProcessHost* GetProcess();
  // Ensure that the process this AgentSchedulingGroupHost belongs to is alive.
  // Returns |false| if any part of the initialization failed.
  bool Init();

  // Returns a SafeRef to `this`.
  base::SafeRef<AgentSchedulingGroupHost> GetSafeRef() const;

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
  void CreateSharedStorageWorkletService(
      mojo::PendingReceiver<blink::mojom::SharedStorageWorkletService> receiver,
      blink::mojom::WorkletGlobalScopeCreationParamsPtr
          global_scope_creation_params);

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
  friend base::StateTransitions<LifecycleState>;
  friend std::ostream& operator<<(std::ostream& os, LifecycleState state);

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnBadMessageReceived(const IPC::Message& message) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

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
  //
  // TODO(crbug.com/40061679): Change back to `raw_ref` after the ad-hoc
  // debugging is no longer needed to investigate the bug.
  const base::SafeRef<RenderProcessHost> process_;

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

  // The `mojom::RouteProvider` mojo pair to setup
  // `blink::AssociatedInterfaceProvider` routes between this and the
  // renderer-side `AgentSchedulingGroup`.
  mojo::AssociatedRemote<mojom::RouteProvider> remote_route_provider_;

  LifecycleState state_{LifecycleState::kNewborn};

  // This is used to create SafeRefs, and as a result, cannot be reset.
  base::WeakPtrFactory<AgentSchedulingGroupHost> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& os,
                         AgentSchedulingGroupHost::LifecycleState state);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_
