// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_
#define CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_

#include <map>

#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/task/single_thread_task_runner.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/public/common/content_features.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom-forward.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"

namespace IPC {
class Message;
class SyncChannel;
}  // namespace IPC

namespace blink {
class WebURL;
class WebView;
}  // namespace blink

namespace content {
class RenderFrameImpl;
class RenderThread;

// Renderer-side representation of AgentSchedulingGroup, used for communication
// with the (browser-side) AgentSchedulingGroupHost. AgentSchedulingGroup is
// Blink's unit of scheduling and performance isolation, which is the only way
// to obtain ordering guarantees between different Mojo (associated) interfaces
// and legacy IPC messages.
class CONTENT_EXPORT AgentSchedulingGroup
    : public IPC::Listener,
      public mojom::AgentSchedulingGroup,
      public mojom::RouteProvider,
      public blink::mojom::AssociatedInterfaceProvider {
 public:
  AgentSchedulingGroup(
      RenderThread& render_thread,
      mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> bootstrap);
  AgentSchedulingGroup(
      RenderThread& render_thread,
      mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver);
  ~AgentSchedulingGroup() override;

  AgentSchedulingGroup(const AgentSchedulingGroup&) = delete;
  AgentSchedulingGroup& operator=(const AgentSchedulingGroup&) = delete;

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  bool Send(IPC::Message* message);
#endif
  void AddFrameRoute(const blink::LocalFrameToken& frame_token,
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
                     int routing_id,
#endif
                     RenderFrameImpl* render_frame,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  void RemoveFrameRoute(const blink::LocalFrameToken& frame_token
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
                        ,
                        int routing_id
#endif
  );
  void DidUnloadRenderFrame(const blink::LocalFrameToken& frame_token);

  mojom::RouteProvider* GetRemoteRouteProvider();

  blink::scheduler::WebAgentGroupScheduler& agent_group_scheduler() {
    return *agent_group_scheduler_;
  }

  // Create a new WebView in this AgentSchedulingGroup.
  blink::WebView* CreateWebView(mojom::CreateViewParamsPtr params,
                                bool was_created_by_renderer,
                                const blink::WebURL& base_url);

 protected:
  // mojom::AgentSchedulingGroup:
  void BindAssociatedInterfaces(
      mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
          remote_host,
      mojo::PendingAssociatedReceiver<mojom::RouteProvider>
          route_provider_receiever) override;

 private:
  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnBadMessageReceived(const IPC::Message& message) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // mojom::AgentSchedulingGroup:
  void CreateView(mojom::CreateViewParamsPtr params) override;
  void CreateFrame(mojom::CreateFrameParamsPtr params) override;
  void CreateSharedStorageWorkletService(
      mojo::PendingReceiver<blink::mojom::SharedStorageWorkletService> receiver,
      blink::mojom::WorkletGlobalScopeCreationParamsPtr
          global_scope_creation_params) override;

  // mojom::RouteProvider
  void GetRoute(
      const blink::LocalFrameToken& frame_token,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
          receiver) override;

  // blink::mojom::AssociatedInterfaceProvider
  void GetAssociatedInterface(
      const std::string& name,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
          receiver) override;

  RenderFrameImpl* GetListener(const blink::LocalFrameToken& frame_token);

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  RenderFrameImpl* GetListener(int32_t routing_id);
#endif

  // Map of registered RenderFrames.
  std::map<blink::LocalFrameToken, raw_ptr<RenderFrameImpl, CtnExperimental>>
      listener_map_;

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  std::map<int32_t, raw_ptr<RenderFrameImpl, CtnExperimental>> routing_id_map_;
#endif

  // A dedicated scheduler for this AgentSchedulingGroup.
  std::unique_ptr<blink::scheduler::WebAgentGroupScheduler>
      agent_group_scheduler_;

  // This AgentSchedulingGroup's legacy IPC channel. Will only be used in
  // `features::MBIMode::kEnabledPerRenderProcessHost` or
  // `features::MBIMode::kEnabledPerSiteInstance` mode.
  std::unique_ptr<IPC::SyncChannel> channel_;

  const raw_ref<RenderThread> render_thread_;

  // Implementation of `mojom::AgentSchedulingGroup`, used for responding to
  // calls from the (browser-side) `AgentSchedulingGroupHost`.
  mojo::AssociatedReceiver<mojom::AgentSchedulingGroup> receiver_;

  // Remote stub of mojom::AgentSchedulingGroupHost, used for sending calls to
  // the (browser-side) AgentSchedulingGroupHost.
  mojo::AssociatedRemote<mojom::AgentSchedulingGroupHost> host_remote_;

  // The `mojom::RouteProvider` mojo receiver that the browser uses to establish
  // a `blink::AssociatedInterfaceProvider` route between `this` and a
  // `RenderFrameHostImpl`. See documentation above
  // `associated_interface_provider_receivers_`.
  mojo::AssociatedReceiver<mojom::RouteProvider> route_provider_receiver_{this};

  // The `blink::mojom::AssociatedInterfaceProvider` receiver set that *all*
  // `RenderFrameHostImpl`s associated with this agent scheduling group own a
  // remote to. `this` handles each of their associated interface requests. If
  // we have an associated `RenderFrameImpl` that we can forward the request to,
  // we do. Otherwise, we "queue" these requests in `pending_receivers_`. This
  // is really bad though; see the documentation there.
  mojo::AssociatedReceiverSet<blink::mojom::AssociatedInterfaceProvider,
                              blink::LocalFrameToken>
      associated_interface_provider_receivers_;

  struct ReceiverData {
    ReceiverData(
        const std::string& name,
        mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
            receiver);
    ReceiverData(ReceiverData&& other);
    ~ReceiverData();

    std::string name;
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface> receiver;
  };

  // See warning in `GetAssociatedInterface`.
  // Map from `blink::LocalFrameToken` to pending receivers that have not had
  // their route added. Note this is unsafe and can lead to message drops.
  std::multimap<blink::LocalFrameToken, ReceiverData> pending_receivers_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_
