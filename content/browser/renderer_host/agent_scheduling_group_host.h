// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_H_

#include <stdint.h>

#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/associated_interfaces.mojom-forward.h"
#include "content/common/content_export.h"
#include "content/common/renderer.mojom-forward.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace IPC {
class ChannelProxy;
class Listener;
class Message;
}  // namespace IPC

namespace content {

class RenderProcessHost;
class SiteInstance;

// Browser-side host of an AgentSchedulingGroup, used for
// AgentSchedulingGroup-bound messaging. AgentSchedulingGroup is Blink's unit of
// scheduling and performance isolation, which is the only way to obtain
// ordering guarantees between different Mojo (associated) interfaces and legacy
// IPC messages.
//
// An AgentSchedulingGroupHost is stored as (and owned by) UserData on the
// RenderProcessHost.
class CONTENT_EXPORT AgentSchedulingGroupHost
    : public mojom::AgentSchedulingGroupHost {
 public:
  // Get the appropriate AgentSchedulingGroupHost for the given |instance| and
  // |process|. For now, each RenderProcessHost has a single
  // AgentSchedulingGroupHost, though future policies will allow multiple groups
  // in a process.
  static AgentSchedulingGroupHost* Get(const SiteInstance& instance,
                                       RenderProcessHost& process);

  // Should not be called explicitly. Use Get() instead.
  explicit AgentSchedulingGroupHost(RenderProcessHost& process);
  ~AgentSchedulingGroupHost() override;

  RenderProcessHost* GetProcess();

  // IPC and mojo messages to be forwarded to the RenderProcessHost, for now. In
  // the future they will be handled directly by the AgentSchedulingGroupHost.
  // IPC:
  IPC::ChannelProxy* GetChannel();
  bool Send(IPC::Message* message);
  void AddRoute(int32_t routing_id, IPC::Listener* listener);
  void RemoveRoute(int32_t routing_id);

  // Mojo:
  mojom::RouteProvider* GetRemoteRouteProvider();
  void CreateFrame(mojom::CreateFrameParamsPtr params);
  void CreateView(mojom::CreateViewParamsPtr params);
  void DestroyView(int32_t routing_id);

 private:
  // The RenderProcessHost this AgentSchedulingGroup is assigned to.
  RenderProcessHost& process_;

  // Implementation of `mojom::AgentSchedulingGroupHost`, used for responding to
  // calls from the (renderer-side) `AgentSchedulingGroup`.
  mojo::Receiver<mojom::AgentSchedulingGroupHost> receiver_{this};

  // Remote stub of `mojom::AgentSchedulingGroup`, used for sending calls to the
  // (renderer-side) `AgentSchedulingGroup`.
  mojo::Remote<mojom::AgentSchedulingGroup> mojo_remote_;
};

}  // namespace content

#endif
