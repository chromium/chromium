// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_
#define CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_

#include "base/callback.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

// Renderer-side representation of AgentSchedulingGroup, used for communication
// with the (browser-side) AgentSchedulingGroupHost. AgentSchedulingGroup is
// Blink's unit of scheduling and performance isolation, which is the only way
// to obtain ordering guarantees between different Mojo (associated) interfaces
// and legacy IPC messages.
class CONTENT_EXPORT AgentSchedulingGroup : public mojom::AgentSchedulingGroup {
 public:
  // |mojo_disconnect_handler| will be called with |this| when |receiver| is
  // disconnected.
  AgentSchedulingGroup(
      mojo::PendingRemote<mojom::AgentSchedulingGroupHost> host_remote,
      mojo::PendingReceiver<mojom::AgentSchedulingGroup> receiver,
      base::OnceCallback<void(const AgentSchedulingGroup*)>
          mojo_disconnect_handler);
  AgentSchedulingGroup(
      mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
          host_remote,
      mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver,
      base::OnceCallback<void(const AgentSchedulingGroup*)>
          mojo_disconnect_handler);
  ~AgentSchedulingGroup() override;

  AgentSchedulingGroup(const AgentSchedulingGroup&) = delete;
  AgentSchedulingGroup(const AgentSchedulingGroup&&) = delete;
  AgentSchedulingGroup& operator=(const AgentSchedulingGroup&) = delete;
  AgentSchedulingGroup& operator=(const AgentSchedulingGroup&&) = delete;

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
        base::OnceClosure disconnect_handler);
    MaybeAssociatedReceiver(
        AgentSchedulingGroup& impl,
        mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver,
        base::OnceClosure disconnect_handler);
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

  // Implementation of `mojom::AgentSchedulingGroup`, used for responding to
  // calls from the (browser-side) `AgentSchedulingGroupHost`.
  MaybeAssociatedReceiver receiver_;

  // Remote stub of mojom::AgentSchedulingGroupHost, used for sending calls to
  // the (browser-side) AgentSchedulingGroupHost.
  MaybeAssociatedRemote host_remote_;
};

}  // namespace content

#endif
