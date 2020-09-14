// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/agent_scheduling_group.h"

#include "base/feature_list.h"
#include "content/public/common/content_features.h"

namespace content {

using ::mojo::AssociatedReceiver;
using ::mojo::AssociatedRemote;
using ::mojo::PendingAssociatedReceiver;
using ::mojo::PendingAssociatedRemote;
using ::mojo::PendingReceiver;
using ::mojo::PendingRemote;
using ::mojo::Receiver;
using ::mojo::Remote;

// MaybeAssociatedReceiver:
AgentSchedulingGroup::MaybeAssociatedReceiver::MaybeAssociatedReceiver(
    AgentSchedulingGroup& impl,
    PendingReceiver<mojom::AgentSchedulingGroup> receiver,
    base::OnceClosure disconnect_handler)
    : receiver_(absl::in_place_type<Receiver<mojom::AgentSchedulingGroup>>,
                &impl,
                std::move(receiver)) {
  if (disconnect_handler)
    absl::get<Receiver<mojom::AgentSchedulingGroup>>(receiver_)
        .set_disconnect_handler(std::move(disconnect_handler));
}

AgentSchedulingGroup::MaybeAssociatedReceiver::MaybeAssociatedReceiver(
    AgentSchedulingGroup& impl,
    PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver,
    base::OnceClosure disconnect_handler)
    : receiver_(
          absl::in_place_type<AssociatedReceiver<mojom::AgentSchedulingGroup>>,
          &impl,
          std::move(receiver)) {
  if (disconnect_handler)
    absl::get<AssociatedReceiver<mojom::AgentSchedulingGroup>>(receiver_)
        .set_disconnect_handler(std::move(disconnect_handler));
}

AgentSchedulingGroup::MaybeAssociatedReceiver::~MaybeAssociatedReceiver() =
    default;

// MaybeAssociatedRemote:
AgentSchedulingGroup::MaybeAssociatedRemote::MaybeAssociatedRemote(
    PendingRemote<mojom::AgentSchedulingGroupHost> host_remote)
    : remote_(absl::in_place_type<Remote<mojom::AgentSchedulingGroupHost>>,
              std::move(host_remote)) {}

AgentSchedulingGroup::MaybeAssociatedRemote::MaybeAssociatedRemote(
    PendingAssociatedRemote<mojom::AgentSchedulingGroupHost> host_remote)
    : remote_(absl::in_place_type<
                  AssociatedRemote<mojom::AgentSchedulingGroupHost>>,
              std::move(host_remote)) {}

AgentSchedulingGroup::MaybeAssociatedRemote::~MaybeAssociatedRemote() = default;

// AgentSchedulingGroup:
AgentSchedulingGroup::AgentSchedulingGroup(
    PendingRemote<mojom::AgentSchedulingGroupHost> host_remote,
    PendingReceiver<mojom::AgentSchedulingGroup> receiver,
    base::OnceCallback<void(const AgentSchedulingGroup*)>
        mojo_disconnect_handler)
    // TODO(crbug.com/1111231): Mojo interfaces should be associated with
    // per-ASG task runners instead of default.
    : receiver_(*this,
                std::move(receiver),
                base::BindOnce(std::move(mojo_disconnect_handler), this)),
      host_remote_(std::move(host_remote)) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kMbiDetachAgentSchedulingGroupFromChannel));
}

AgentSchedulingGroup::AgentSchedulingGroup(
    PendingAssociatedRemote<mojom::AgentSchedulingGroupHost> host_remote,
    PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver,
    base::OnceCallback<void(const AgentSchedulingGroup*)>
        mojo_disconnect_handler)
    // TODO(crbug.com/1111231): Mojo interfaces should be associated with
    // per-ASG task runners instead of default.
    : receiver_(*this,
                std::move(receiver),
                base::BindOnce(std::move(mojo_disconnect_handler), this)),
      host_remote_(std::move(host_remote)) {
  DCHECK(!base::FeatureList::IsEnabled(
      features::kMbiDetachAgentSchedulingGroupFromChannel));
}

AgentSchedulingGroup::~AgentSchedulingGroup() = default;

}  // namespace content
