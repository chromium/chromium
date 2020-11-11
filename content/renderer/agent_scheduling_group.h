// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_
#define CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_

#include "base/containers/id_map.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"

namespace IPC {
class Listener;
class Message;
}  // namespace IPC

namespace content {

class RenderThread;

// Renderer-side representation of AgentSchedulingGroup, used for communication
// with the (browser-side) AgentSchedulingGroupHost. AgentSchedulingGroup is
// Blink's unit of scheduling and performance isolation, which is the only way
// to obtain ordering guarantees between different Mojo (associated) interfaces
// and legacy IPC messages.
class CONTENT_EXPORT AgentSchedulingGroup
    : public mojom::AgentSchedulingGroup,
      public mojom::RouteProvider,
      public blink::mojom::AssociatedInterfaceProvider {
 public:
  AgentSchedulingGroup(
      RenderThread& render_thread,
      mojo::PendingRemote<mojom::AgentSchedulingGroupHost> host_remote,
      mojo::PendingReceiver<mojom::AgentSchedulingGroup> receiver);
  AgentSchedulingGroup(
      RenderThread& render_thread,
      mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
          host_remote,
      mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver);
  ~AgentSchedulingGroup() override;

  AgentSchedulingGroup(const AgentSchedulingGroup&) = delete;
  AgentSchedulingGroup(const AgentSchedulingGroup&&) = delete;
  AgentSchedulingGroup& operator=(const AgentSchedulingGroup&) = delete;
  AgentSchedulingGroup& operator=(const AgentSchedulingGroup&&) = delete;

  bool Send(IPC::Message* message);
  void AddRoute(int32_t routing_id, IPC::Listener* listener);
  void RemoveRoute(int32_t routing_id);

  // This is virtual only for unit tests.
  virtual mojom::RouteProvider* GetRemoteRouteProvider();

  blink::scheduler::WebAgentGroupScheduler& agent_group_scheduler() {
    return *agent_group_scheduler_;
  }

 private:
  // `MaybeAssociatedReceiver` and `MaybeAssociatedRemote` are temporary helper
  // classes that allow us to switch between using associated and non-associated
  // mojo interfaces. This behavior is controlled by the
  // `kMbiDetachAgentSchedulingGroupFromChannel` feature flag.
  // Associated interfaces are associated with the IPC channel (transitively,
  // via the `Renderer` interface), thus preserving cross-agent scheduling group
  // message order. Non-associated interfaces are independent from each other
  // and do not preserve message order between agent scheduling groups.
  // TODO(crbug.com/1111231): Remove these once we can remove the flag.
  class MaybeAssociatedReceiver {
   public:
    MaybeAssociatedReceiver(
        AgentSchedulingGroup& impl,
        mojo::PendingReceiver<mojom::AgentSchedulingGroup> receiver,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner);
    MaybeAssociatedReceiver(
        AgentSchedulingGroup& impl,
        mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner);
    ~MaybeAssociatedReceiver();

   private:
    absl::variant<mojo::Receiver<mojom::AgentSchedulingGroup>,
                  mojo::AssociatedReceiver<mojom::AgentSchedulingGroup>>
        receiver_;
  };

  class MaybeAssociatedRemote {
   public:
    explicit MaybeAssociatedRemote(
        mojo::PendingRemote<mojom::AgentSchedulingGroupHost> host_remote,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner);
    explicit MaybeAssociatedRemote(
        mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
            host_remote,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner);
    ~MaybeAssociatedRemote();
    mojom::AgentSchedulingGroupHost* get();

   private:
    absl::variant<mojo::Remote<mojom::AgentSchedulingGroupHost>,
                  mojo::AssociatedRemote<mojom::AgentSchedulingGroupHost>>
        remote_;
  };

  // mojom::AgentSchedulingGroup:
  void CreateView(mojom::CreateViewParamsPtr params) override;
  void DestroyView(int32_t view_id, DestroyViewCallback callback) override;
  void CreateFrame(mojom::CreateFrameParamsPtr params) override;
  void CreateFrameProxy(
      int32_t routing_id,
      int32_t render_view_routing_id,
      const base::Optional<base::UnguessableToken>& opener_frame_token,
      int32_t parent_routing_id,
      const FrameReplicationState& replicated_state,
      const base::UnguessableToken& frame_token,
      const base::UnguessableToken& devtools_frame_token) override;
  void BindAssociatedRouteProvider(
      mojo::PendingAssociatedRemote<mojom::RouteProvider> remote,
      mojo::PendingAssociatedReceiver<mojom::RouteProvider> receiever) override;

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

  IPC::Listener* GetListener(int32_t routing_id);

  // Map of registered IPC listeners.
  base::IDMap<IPC::Listener*> listener_map_;

  // A dedicated scheduler for this AgentSchedulingGroup.
  std::unique_ptr<blink::scheduler::WebAgentGroupScheduler>
      agent_group_scheduler_;

  RenderThread& render_thread_;

  // Implementation of `mojom::AgentSchedulingGroup`, used for responding to
  // calls from the (browser-side) `AgentSchedulingGroupHost`.
  MaybeAssociatedReceiver receiver_;

  // Remote stub of mojom::AgentSchedulingGroupHost, used for sending calls to
  // the (browser-side) AgentSchedulingGroupHost.
  MaybeAssociatedRemote host_remote_;

  // The |mojom::RouteProvider| mojo pair to setup
  // |blink::AssociatedInterfaceProvider| routes between us and the browser-side
  // |AgentSchedulingGroup|.
  mojo::AssociatedRemote<mojom::RouteProvider> remote_route_provider_;
  mojo::AssociatedReceiver<mojom::RouteProvider> route_provider_receiver_{this};

  // The `blink::mojom::AssociatedInterfaceProvider` receiver set that *all*
  // browser-side `blink::AssociatedInterfaceProvider` objects own a remote to.
  // `AgentSchedulingGroupHost` will be responsible for routing each associated
  // interface request to the appropriate renderer object.
  mojo::AssociatedReceiverSet<blink::mojom::AssociatedInterfaceProvider,
                              int32_t>
      associated_interface_provider_receivers_;
};

}  // namespace content

#endif
