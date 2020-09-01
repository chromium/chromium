// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/agent_scheduling_group.h"

namespace content {

AgentSchedulingGroup::AgentSchedulingGroup(
    mojo::PendingRemote<mojom::AgentSchedulingGroupHost> host_remote,
    mojo::PendingReceiver<mojom::AgentSchedulingGroup> receiver,
    base::OnceCallback<void(const AgentSchedulingGroup*)>
        mojo_disconnect_handler)
    // TODO(crbug.com/1111231): Mojo interfaces should be associated with
    // per-ASG task runners instead of default.
    : receiver_(this, std::move(receiver)),
      host_remote_(std::move(host_remote)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(std::move(mojo_disconnect_handler), this));
}

AgentSchedulingGroup::~AgentSchedulingGroup() = default;

}  // namespace content
