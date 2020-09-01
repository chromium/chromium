// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_
#define CONTENT_RENDERER_AGENT_SCHEDULING_GROUP_H_

#include "base/callback.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

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
  ~AgentSchedulingGroup() override;

  AgentSchedulingGroup(const AgentSchedulingGroup&) = delete;
  AgentSchedulingGroup(const AgentSchedulingGroup&&) = delete;
  AgentSchedulingGroup& operator=(const AgentSchedulingGroup&) = delete;
  AgentSchedulingGroup& operator=(const AgentSchedulingGroup&&) = delete;

 private:
  // Implementation of `mojom::AgentSchedulingGroup`, used for responding to
  // calls from the (browser-side) `AgentSchedulingGroupHost`.
  mojo::Receiver<mojom::AgentSchedulingGroup> receiver_;

  // Remote stub of mojom::AgentSchedulingGroupHost, used for sending calls to
  // the (browser-side) AgentSchedulingGroupHost.
  mojo::Remote<mojom::AgentSchedulingGroupHost> host_remote_;
};

}  // namespace content

#endif
