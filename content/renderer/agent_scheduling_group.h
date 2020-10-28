// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_
#define CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_

#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"

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

  mojom::RouteProvider* GetRemoteRouteProvider();

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
        mojo::PendingReceiver<mojom::AgentSchedulingGroup> receiver);
    MaybeAssociatedReceiver(
        AgentSchedulingGroup& impl,
        mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver);
    ~MaybeAssociatedReceiver();

   private:
    absl::variant<mojo::Receiver<mojom::AgentSchedulingGroup>,
                  mojo::AssociatedReceiver<mojom::AgentSchedulingGroup>>
        receiver_;
  };

  class MaybeAssociatedRemote {
   public:
    explicit MaybeAssociatedRemote(
        mojo::PendingRemote<mojom::AgentSchedulingGroupHost> host_remote);
    explicit MaybeAssociatedRemote(
        mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
            host_remote);
    ~MaybeAssociatedRemote();

   private:
    absl::variant<mojo::Remote<mojom::AgentSchedulingGroupHost>,
                  mojo::AssociatedRemote<mojom::AgentSchedulingGroupHost>>
        remote_;
  };

  // mojom::AgentSchedulingGroup:
  void CreateView(mojom::CreateViewParamsPtr params) override;
  void DestroyView(int32_t view_id) override;
  void CreateFrame(mojom::CreateFrameParamsPtr params) override;
  void CreateFrameProxy(
      int32_t routing_id,
      int32_t render_view_routing_id,
      const base::Optional<base::UnguessableToken>& opener_frame_token,
      int32_t parent_routing_id,
      const FrameReplicationState& replicated_state,
      const base::UnguessableToken& frame_token,
      const base::UnguessableToken& devtools_frame_token) override;

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

  RenderThread& render_thread_;

  // Implementation of `mojom::AgentSchedulingGroup`, used for responding to
  // calls from the (browser-side) `AgentSchedulingGroupHost`.
  MaybeAssociatedReceiver receiver_;

  // Remote stub of mojom::AgentSchedulingGroupHost, used for sending calls to
  // the (browser-side) AgentSchedulingGroupHost.
  MaybeAssociatedRemote host_remote_;
};

}  // namespace content

#endif
