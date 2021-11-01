// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/agent_scheduling_group.h"

#include <map>
#include <utility>

#include "base/feature_list.h"
#include "base/types/pass_key.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/public/common/content_features.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/services/shared_storage_worklet/public/mojom/shared_storage_worklet_service.mojom.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"

namespace content {

using ::IPC::ChannelMojo;
using ::IPC::Listener;
using ::IPC::SyncChannel;
using ::mojo::AssociatedReceiver;
using ::mojo::AssociatedRemote;
using ::mojo::PendingAssociatedReceiver;
using ::mojo::PendingAssociatedRemote;
using ::mojo::PendingReceiver;
using ::mojo::PendingRemote;
using ::mojo::Receiver;
using ::mojo::Remote;

using PassKey = ::base::PassKey<AgentSchedulingGroup>;

namespace {

RenderThreadImpl& ToImpl(RenderThread& render_thread) {
  DCHECK(RenderThreadImpl::current());
  return static_cast<RenderThreadImpl&>(render_thread);
}

static features::MBIMode GetMBIMode() {
  return base::FeatureList::IsEnabled(features::kMBIMode)
             ? features::kMBIModeParam.Get()
             : features::MBIMode::kLegacy;
}

}  // namespace

AgentSchedulingGroup::ReceiverData::ReceiverData(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface> receiver)
    : name(std::move(name)), receiver(std::move(receiver)) {}

AgentSchedulingGroup::ReceiverData::ReceiverData(ReceiverData&& other)
    : name(std::move(other.name)), receiver(std::move(other.receiver)) {}

AgentSchedulingGroup::ReceiverData::~ReceiverData() = default;

// AgentSchedulingGroup:
AgentSchedulingGroup::AgentSchedulingGroup(
    RenderThread& render_thread,
    mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> bootstrap,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker_remote)
    : agent_group_scheduler_(
          blink::scheduler::WebThreadScheduler::MainThreadScheduler()
              ->CreateAgentGroupScheduler()),
      render_thread_(render_thread),
      // `receiver_` will be bound by `OnAssociatedInterfaceRequest()`.
      receiver_(this) {
  DCHECK(agent_group_scheduler_);
  DCHECK_NE(GetMBIMode(), features::MBIMode::kLegacy);

  agent_group_scheduler_->BindInterfaceBroker(std::move(broker_remote));

  channel_ = SyncChannel::Create(
      /*listener=*/this, /*ipc_task_runner=*/render_thread_.GetIOTaskRunner(),
      /*listener_task_runner=*/agent_group_scheduler_->DefaultTaskRunner(),
      render_thread_.GetShutdownEvent());

  // TODO(crbug.com/1111231): Add necessary filters.
  // Currently, the renderer process has these filters:
  // 1. `UnfreezableMessageFilter` - in the process of being removed,
  // 2. `PnaclTranslationResourceHost` - NaCl is going away, and
  // 3. `AutomationMessageFilter` - needs to be handled somehow.

  channel_->Init(
      ChannelMojo::CreateClientFactory(
          bootstrap.PassPipe(),
          /*ipc_task_runner=*/render_thread_.GetIOTaskRunner(),
          /*proxy_task_runner=*/agent_group_scheduler_->DefaultTaskRunner()),
      /*create_pipe_now=*/true);
}

AgentSchedulingGroup::AgentSchedulingGroup(
    RenderThread& render_thread,
    PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker_remote)
    : agent_group_scheduler_(
          blink::scheduler::WebThreadScheduler::MainThreadScheduler()
              ->CreateAgentGroupScheduler()),
      render_thread_(render_thread),
      receiver_(this,
                std::move(receiver),
                agent_group_scheduler_->DefaultTaskRunner()) {
  DCHECK(agent_group_scheduler_);
  DCHECK_EQ(GetMBIMode(), features::MBIMode::kLegacy);
  agent_group_scheduler_->BindInterfaceBroker(std::move(broker_remote));
}

AgentSchedulingGroup::~AgentSchedulingGroup() = default;

bool AgentSchedulingGroup::OnMessageReceived(const IPC::Message& message) {
  DCHECK_NE(message.routing_id(), MSG_ROUTING_CONTROL);

  auto* listener = GetListener(message.routing_id());
  if (!listener)
    return false;

  return listener->OnMessageReceived(message);
}

void AgentSchedulingGroup::OnBadMessageReceived(const IPC::Message& message) {
  // Not strictly required, since we don't currently do anything with bad
  // messages in the renderer, but if we ever do then this will "just work".
  return ToImpl(render_thread_).OnBadMessageReceived(message);
}

void AgentSchedulingGroup::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  // The ASG's channel should only be used to bootstrap the ASG mojo interface.
  DCHECK_EQ(interface_name, mojom::AgentSchedulingGroup::Name_);
  DCHECK(!receiver_.is_bound());

  PendingAssociatedReceiver<mojom::AgentSchedulingGroup> pending_receiver(
      std::move(handle));
  receiver_.Bind(std::move(pending_receiver),
                 agent_group_scheduler_->DefaultTaskRunner());
}

bool AgentSchedulingGroup::Send(IPC::Message* message) {
  std::unique_ptr<IPC::Message> msg(message);

  if (GetMBIMode() == features::MBIMode::kLegacy)
    return render_thread_.Send(msg.release());

  // This DCHECK is too idealistic for now - messages that are handled by
  // filters are sent control messages since they are intercepted before
  // routing. It is put here as documentation for now, since this code would not
  // be reached until we activate
  // `features::MBIMode::kEnabledPerRenderProcessHost` or
  // `features::MBIMode::kEnabledPerSiteInstance`.
  DCHECK_NE(message->routing_id(), MSG_ROUTING_CONTROL);

  DCHECK(channel_);
  return channel_->Send(msg.release());
}

void AgentSchedulingGroup::AddRoute(int32_t routing_id, Listener* listener) {
  DCHECK(!listener_map_.Lookup(routing_id));
  listener_map_.AddWithID(listener, routing_id);
  render_thread_.AddRoute(routing_id, listener);

  // See warning in `GetAssociatedInterface`.
  // Replay any `GetAssociatedInterface` calls for this route.
  auto range = pending_receivers_.equal_range(routing_id);
  for (auto iter = range.first; iter != range.second; ++iter) {
    ReceiverData& data = iter->second;
    listener->OnAssociatedInterfaceRequest(data.name,
                                           data.receiver.PassHandle());
  }
  pending_receivers_.erase(range.first, range.second);
}

void AgentSchedulingGroup::AddFrameRoute(
    int32_t routing_id,
    IPC::Listener* listener,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  AddRoute(routing_id, listener);
  render_thread_.AttachTaskRunnerToRoute(routing_id, std::move(task_runner));
}

void AgentSchedulingGroup::RemoveRoute(int32_t routing_id) {
  DCHECK(listener_map_.Lookup(routing_id));
  listener_map_.Remove(routing_id);
  render_thread_.RemoveRoute(routing_id);
}

void AgentSchedulingGroup::DidUnloadRenderFrame(
    const blink::LocalFrameToken& frame_token) {
  host_remote_->DidUnloadRenderFrame(frame_token);
}

mojom::RouteProvider* AgentSchedulingGroup::GetRemoteRouteProvider() {
  DCHECK(remote_route_provider_);
  return remote_route_provider_.get();
}

void AgentSchedulingGroup::CreateView(mojom::CreateViewParamsPtr params) {
  RenderThreadImpl& renderer = ToImpl(render_thread_);
  renderer.SetScrollAnimatorEnabled(
      params->web_preferences.enable_scroll_animator, PassKey());

  RenderViewImpl::Create(*this, std::move(params),
                         /*was_created_by_renderer=*/false,
                         agent_group_scheduler_->DefaultTaskRunner());
}

void AgentSchedulingGroup::DestroyView(int32_t view_id) {
  RenderViewImpl* view = RenderViewImpl::FromRoutingID(view_id);
  DCHECK(view);

  // This IPC can be called from re-entrant contexts. We can't destroy a
  // RenderViewImpl while references still exist on the stack, so we dispatch a
  // non-nestable task. This method is called exactly once by the browser
  // process, and is used to release ownership of the corresponding
  // RenderViewImpl instance. https://crbug.com/1000035.
  agent_group_scheduler_->DefaultTaskRunner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&RenderViewImpl::Destroy, base::Unretained(view)));
}

void AgentSchedulingGroup::CreateFrame(mojom::CreateFrameParamsPtr params) {
  RenderFrameImpl::CreateFrame(
      *this, params->token, params->routing_id, std::move(params->frame),
      std::move(params->interface_broker), params->previous_routing_id,
      params->opener_frame_token, params->parent_routing_id,
      params->previous_sibling_routing_id, params->devtools_frame_token,
      params->tree_scope_type, std::move(params->replication_state),
      std::move(params->widget_params),
      std::move(params->frame_owner_properties),
      params->is_on_initial_empty_document,
      std::move(params->policy_container));
}

void AgentSchedulingGroup::CreateFrameProxy(
    const blink::RemoteFrameToken& token,
    int32_t routing_id,
    const absl::optional<blink::FrameToken>& opener_frame_token,
    int32_t view_routing_id,
    int32_t parent_routing_id,
    blink::mojom::TreeScopeType tree_scope_type,
    blink::mojom::FrameReplicationStatePtr replicated_state,
    const base::UnguessableToken& devtools_frame_token,
    mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces) {
  RenderFrameProxy::CreateFrameProxy(
      *this, token, routing_id, opener_frame_token, view_routing_id,
      parent_routing_id, tree_scope_type, std::move(replicated_state),
      devtools_frame_token, std::move(remote_main_frame_interfaces));
}

void AgentSchedulingGroup::CreateSharedStorageWorkletService(
    mojo::PendingReceiver<
        shared_storage_worklet::mojom::SharedStorageWorkletService> receiver) {
  RenderThreadImpl& renderer = ToImpl(render_thread_);
  renderer.CreateSharedStorageWorkletService(std::move(receiver));
}

void AgentSchedulingGroup::BindAssociatedInterfaces(
    mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost> remote_host,
    mojo::PendingAssociatedRemote<mojom::RouteProvider> remote_route_provider,
    mojo::PendingAssociatedReceiver<mojom::RouteProvider>
        route_provider_receiever) {
  host_remote_.Bind(std::move(remote_host),
                    agent_group_scheduler_->DefaultTaskRunner());
  remote_route_provider_.Bind(std::move(remote_route_provider),
                              agent_group_scheduler_->DefaultTaskRunner());
  route_provider_receiver_.Bind(std::move(route_provider_receiever),
                                agent_group_scheduler_->DefaultTaskRunner());
}

void AgentSchedulingGroup::GetRoute(
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  DCHECK(receiver.is_valid());
  associated_interface_provider_receivers_.Add(
      this, std::move(receiver), routing_id,
      agent_group_scheduler_->DefaultTaskRunner());
}

void AgentSchedulingGroup::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  int32_t routing_id =
      associated_interface_provider_receivers_.current_context();

  if (auto* listener = GetListener(routing_id)) {
    listener->OnAssociatedInterfaceRequest(name, receiver.PassHandle());
  } else {
    // THIS IS UNSAFE!
    // Associated receivers must be bound immediately or they could drop
    // messages. This is needed short term so the browser side Remote isn't
    // broken even after the corresponding `AddRoute` happens. Browser should
    // avoid calling this before the corresponding `AddRoute`, but this is a
    // short term workaround until that happens.
    pending_receivers_.emplace(routing_id,
                               ReceiverData(name, std::move(receiver)));
  }
}

Listener* AgentSchedulingGroup::GetListener(int32_t routing_id) {
  DCHECK_NE(routing_id, MSG_ROUTING_CONTROL);

  return listener_map_.Lookup(routing_id);
}

}  // namespace content
