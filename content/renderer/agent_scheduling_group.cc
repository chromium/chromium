// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/agent_scheduling_group.h"

#include "base/feature_list.h"
#include "base/util/type_safety/pass_key.h"
#include "content/public/common/content_features.h"
#include "content/renderer/compositor/compositor_dependencies.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"

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

mojom::AgentSchedulingGroupHost*
AgentSchedulingGroup::MaybeAssociatedRemote::get() {
  return absl::visit([](auto& r) { return r.get(); }, remote_);
}

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
  DCHECK(remote_route_provider_);
  return remote_route_provider_.get();
}

void AgentSchedulingGroup::CreateView(mojom::CreateViewParamsPtr params) {
  RenderThreadImpl& renderer = ToImpl(render_thread_);
  renderer.SetScrollAnimatorEnabled(
      params->web_preferences.enable_scroll_animator, PassKey());

  RenderViewImpl::Create(
      *this, &renderer, std::move(params), RenderWidget::ShowCallback(),
      // TODO(crbug.com/1111231): Use proper per-ASG task-runner.
      renderer.GetWebMainThreadScheduler()->DefaultTaskRunner());
}

void AgentSchedulingGroup::DestroyView(int32_t view_id,
                                       DestroyViewCallback callback) {
  RenderViewImpl* view = RenderViewImpl::FromRoutingID(view_id);
  DCHECK(view);

  // This IPC can be called from re-entrant contexts. We can't destroy a
  // RenderViewImpl while references still exist on the stack, so we dispatch a
  // non-nestable task. This method is called exactly once by the browser
  // process, and is used to release ownership of the corresponding
  // RenderViewImpl instance. https://crbug.com/1000035.
  base::ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&RenderViewImpl::Destroy, base::Unretained(view))
          .Then(std::move(callback)));
}

void AgentSchedulingGroup::CreateFrame(mojom::CreateFrameParamsPtr params) {
  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      interface_provider(
          std::move(params->interface_bundle->interface_provider));
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker(
          std::move(params->interface_bundle->browser_interface_broker));

  RenderFrameImpl::CreateFrame(
      *this, params->routing_id, std::move(interface_provider),
      std::move(browser_interface_broker), params->previous_routing_id,
      params->opener_frame_token, params->parent_routing_id,
      params->previous_sibling_routing_id, params->frame_token,
      params->devtools_frame_token, params->replication_state,
      &ToImpl(render_thread_), std::move(params->widget_params),
      std::move(params->frame_owner_properties),
      params->has_committed_real_load);
}

void AgentSchedulingGroup::CreateFrameProxy(
    int32_t routing_id,
    int32_t render_view_routing_id,
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int32_t parent_routing_id,
    const FrameReplicationState& replicated_state,
    const base::UnguessableToken& frame_token,
    const base::UnguessableToken& devtools_frame_token) {
  RenderFrameProxy::CreateFrameProxy(
      *this, routing_id, render_view_routing_id, opener_frame_token,
      parent_routing_id, replicated_state, frame_token, devtools_frame_token);
}

void AgentSchedulingGroup::BindAssociatedRouteProvider(
    mojo::PendingAssociatedRemote<mojom::RouteProvider> remote,
    mojo::PendingAssociatedReceiver<mojom::RouteProvider> receiver) {
  remote_route_provider_.Bind(std::move(remote));
  route_provider_receiver_.Bind(std::move(receiver),
                                ToImpl(render_thread_)
                                    .GetWebMainThreadScheduler()
                                    ->DeprecatedDefaultTaskRunner());
}

void AgentSchedulingGroup::GetRoute(
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  DCHECK(receiver.is_valid());
  associated_interface_provider_receivers_.Add(this, std::move(receiver),
                                               routing_id);
}

void AgentSchedulingGroup::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  int32_t routing_id =
      associated_interface_provider_receivers_.current_context();
  IPC::Listener* listener = ToImpl(render_thread_).GetListener(routing_id);
  if (listener)
    listener->OnAssociatedInterfaceRequest(name, receiver.PassHandle());
}

}  // namespace content
