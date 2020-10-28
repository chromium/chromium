// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/agent_scheduling_group.h"

#include "base/feature_list.h"
#include "base/util/type_safety/pass_key.h"
#include "content/public/common/content_features.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

using ::IPC::Listener;
using ::mojo::AssociatedReceiver;
using ::mojo::AssociatedRemote;
using ::mojo::PendingAssociatedReceiver;
using ::mojo::PendingAssociatedRemote;
using ::mojo::PendingReceiver;
using ::mojo::PendingRemote;
using ::mojo::Receiver;
using ::mojo::Remote;

using PassKey = ::util::PassKey<AgentSchedulingGroup>;

namespace {
RenderThreadImpl& ToImpl(RenderThread& render_thread) {
  DCHECK(RenderThreadImpl::current());
  return static_cast<RenderThreadImpl&>(render_thread);
}

}  // namespace

// MaybeAssociatedReceiver:
AgentSchedulingGroup::MaybeAssociatedReceiver::MaybeAssociatedReceiver(
    AgentSchedulingGroup& impl,
    PendingReceiver<mojom::AgentSchedulingGroup> receiver)
    : receiver_(absl::in_place_type<Receiver<mojom::AgentSchedulingGroup>>,
                &impl,
                std::move(receiver)) {}

AgentSchedulingGroup::MaybeAssociatedReceiver::MaybeAssociatedReceiver(
    AgentSchedulingGroup& impl,
    PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver)
    : receiver_(
          absl::in_place_type<AssociatedReceiver<mojom::AgentSchedulingGroup>>,
          &impl,
          std::move(receiver)) {}

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
    RenderThread& render_thread,
    PendingRemote<mojom::AgentSchedulingGroupHost> host_remote,
    PendingReceiver<mojom::AgentSchedulingGroup> receiver)
    // TODO(crbug.com/1111231): Mojo interfaces should be associated with
    // per-ASG task runners instead of default.
    : render_thread_(render_thread),
      receiver_(*this, std::move(receiver)),
      host_remote_(std::move(host_remote)) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kMbiDetachAgentSchedulingGroupFromChannel));
}

AgentSchedulingGroup::AgentSchedulingGroup(
    RenderThread& render_thread,
    PendingAssociatedRemote<mojom::AgentSchedulingGroupHost> host_remote,
    PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver)
    // TODO(crbug.com/1111231): Mojo interfaces should be associated with
    // per-ASG task runners instead of default.
    : render_thread_(render_thread),
      receiver_(*this, std::move(receiver)),
      host_remote_(std::move(host_remote)) {
  DCHECK(!base::FeatureList::IsEnabled(
      features::kMbiDetachAgentSchedulingGroupFromChannel));
}

AgentSchedulingGroup::~AgentSchedulingGroup() = default;

// IPC messages to be forwarded to the `RenderThread`, for now. In the future
// they will be handled directly by the `AgentSchedulingGroup`.
bool AgentSchedulingGroup::Send(IPC::Message* message) {
  // TODO(crbug.com/1111231): For some reason, changing this to use
  // render_thread_ causes trybots to time out (not specific tests).
  return RenderThread::Get()->Send(message);
}

// IPC messages to be forwarded to the `RenderThread`, for now. In the future
// they will be handled directly by the `AgentSchedulingGroup`.
void AgentSchedulingGroup::AddRoute(int32_t routing_id, Listener* listener) {
  // TODO(crbug.com/1111231): For some reason, changing this to use
  // render_thread_ causes trybots to time out (not specific tests).
  RenderThread::Get()->AddRoute(routing_id, listener);
}

// IPC messages to be forwarded to the `RenderThread`, for now. In the future
// they will be handled directly by the `AgentSchedulingGroup`.
void AgentSchedulingGroup::RemoveRoute(int32_t routing_id) {
  // TODO(crbug.com/1111231): For some reason, changing this to use
  // render_thread_ causes trybots to time out (not specific tests).
  RenderThread::Get()->RemoveRoute(routing_id);
}

mojom::RouteProvider* AgentSchedulingGroup::GetRemoteRouteProvider() {
  return render_thread_.GetRemoteRouteProvider(PassKey());
}

void AgentSchedulingGroup::CreateView(mojom::CreateViewParamsPtr params) {
  ToImpl(render_thread_).CreateView(std::move(params), PassKey());
}

void AgentSchedulingGroup::DestroyView(int32_t view_id) {
  ToImpl(render_thread_).DestroyView(view_id, PassKey());
}

void AgentSchedulingGroup::CreateFrame(mojom::CreateFrameParamsPtr params) {
  ToImpl(render_thread_).CreateFrame(std::move(params), PassKey());
}

void AgentSchedulingGroup::CreateFrameProxy(
    int32_t routing_id,
    int32_t render_view_routing_id,
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int32_t parent_routing_id,
    const FrameReplicationState& replicated_state,
    const base::UnguessableToken& frame_token,
    const base::UnguessableToken& devtools_frame_token) {
  ToImpl(render_thread_)
      .CreateFrameProxy(routing_id, render_view_routing_id, opener_frame_token,
                        parent_routing_id, replicated_state, frame_token,
                        devtools_frame_token, PassKey());
}

void AgentSchedulingGroup::GetRoute(
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  // TODO(crbug.com/1111231): Make AgentSchedulingGroup a fully-fledged
  // RouteProvider, so we can start registering routes directly with an
  // AgentSchedulingGroup rather than ChildThreadImpl.
  ToImpl(render_thread_).GetRoute(routing_id, std::move(receiver));
}

void AgentSchedulingGroup::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  // TODO(crbug.com/1111231): Make AgentSchedulingGroup a fully-fledged
  // AssociatedInterfaceProvider, so we can start associating interfaces
  // directly with the AgentSchedulingGroup interface.
  ToImpl(render_thread_).GetAssociatedInterface(name, std::move(receiver));
}

}  // namespace content
