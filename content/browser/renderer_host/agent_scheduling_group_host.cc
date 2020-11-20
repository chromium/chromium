// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/renderer_host/agent_scheduling_group_host.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/supports_user_data.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/state_transitions.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_message.h"

namespace content {

namespace {

using ::IPC::ChannelMojo;
using ::IPC::ChannelProxy;
using ::IPC::Listener;
using ::mojo::AssociatedReceiver;
using ::mojo::AssociatedRemote;
using ::mojo::PendingAssociatedReceiver;
using ::mojo::PendingAssociatedRemote;
using ::mojo::PendingReceiver;
using ::mojo::PendingRemote;
using ::mojo::Receiver;
using ::mojo::Remote;

static constexpr char kAgentGroupHostDataKey[] =
    "AgentSchedulingGroupHostUserDataKey";

class AgentGroupHostUserData : public base::SupportsUserData::Data {
 public:
  explicit AgentGroupHostUserData(
      std::unique_ptr<AgentSchedulingGroupHost> agent_group)
      : agent_group_(std::move(agent_group)) {
    DCHECK(agent_group_);
  }
  ~AgentGroupHostUserData() override = default;

  AgentSchedulingGroupHost* agent_group() { return agent_group_.get(); }

 private:
  std::unique_ptr<AgentSchedulingGroupHost> agent_group_;
};

}  // namespace

// static
AgentSchedulingGroupHost* AgentSchedulingGroupHost::Get(
    const SiteInstance& instance,
    RenderProcessHost& process) {
  AgentGroupHostUserData* data = static_cast<AgentGroupHostUserData*>(
      process.GetUserData(kAgentGroupHostDataKey));
  if (data != nullptr)
    return data->agent_group();

  auto agent_group_data = std::make_unique<AgentGroupHostUserData>(
      std::make_unique<AgentSchedulingGroupHost>(process));
  AgentSchedulingGroupHost* agent_group = agent_group_data->agent_group();
  process.SetUserData(kAgentGroupHostDataKey, std::move(agent_group_data));

  return agent_group;
}

int32_t AgentSchedulingGroupHost::GetNextID() {
  static int32_t next_id = 0;
  return next_id++;
}

AgentSchedulingGroupHost::AgentSchedulingGroupHost(RenderProcessHost& process)
    : process_(process),
      association_mode_(base::FeatureList::IsEnabled(
                            features::kMbiDetachAgentSchedulingGroupFromChannel)
                            ? IPCAssociationMode::kUnassociated
                            : IPCAssociationMode::kAssociatedWithProcess),
      receiver_(this) {
  process_.AddObserver(this);

  // The RenderProcessHost's channel and other mojo interfaces are bound by the
  // time this class is constructed, so we eagerly initialize this class's IPC
  // so they have the same bind lifetime as those of the RenderProcessHost.
  // Furthermore, when the RenderProcessHost's channel and mojo interfaces get
  // reset and reinitialized, we'll be notified so that we can reset and
  // reinitialize ours as well.
  SetUpIPC();
}

// DO NOT USE |process_| HERE! At this point it (or at least parts of it) is no
// longer valid.
AgentSchedulingGroupHost::~AgentSchedulingGroupHost() {
  DCHECK_EQ(state_, LifecycleState::kRenderProcessHostDestroyed);
}

void AgentSchedulingGroupHost::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  SetState(LifecycleState::kRenderProcessExited);
  DCHECK_EQ(host, &process_);

  // We mirror the RenderProcessHost flow here by resetting our mojos, and
  // reinitializing them once the process's IPC::ChannelProxy and renderer
  // interface are reinitialized.
  ResetIPC();

  // RenderProcessHostImpl will attempt to call this method later if it has not
  // already been called. We call it now since `SetUpIPC()` relies on it being
  // called, thus setting up the IPC channel and mojom::Renderer interface.
  process_.EnableSendQueue();

  // We call this so that we can immediately queue IPC and mojo messages on the
  // new channel/interfaces that are bound for the next renderer process, should
  // one eventually be spun up.
  SetUpIPC();
}

void AgentSchedulingGroupHost::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  if (RenderProcessHost::run_renderer_in_process()) {
    // In single process mode, RenderProcessExited call is sometimes omitted.
    if (state_ != LifecycleState::kBound) {
      RenderProcessExited(host, ChildProcessTerminationInfo());
    }
  }
  DCHECK_EQ(state_, LifecycleState::kBound);

  DCHECK_EQ(host, &process_);
  process_.RemoveObserver(this);
  SetState(LifecycleState::kRenderProcessHostDestroyed);
}

bool AgentSchedulingGroupHost::OnMessageReceived(const IPC::Message& message) {
  if (message.routing_id() == MSG_ROUTING_CONTROL) {
    bad_message::ReceivedBadMessage(&process_,
                                    bad_message::ASGH_RECEIVED_CONTROL_MESSAGE);
    return false;
  }

  auto* listener = GetListener(message.routing_id());
  if (!listener)
    return false;

  return listener->OnMessageReceived(message);
}

void AgentSchedulingGroupHost::OnBadMessageReceived(
    const IPC::Message& message) {
  // If a bad message is received, it should be treated the same as a bad
  // message on the renderer-wide channel (i.e., kill the renderer).
  return process_.OnBadMessageReceived(message);
}

void AgentSchedulingGroupHost::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  // There shouldn't be any interfaces requested this way - process-wide
  // interfaces should be requested via the process-wide channel, and
  // ASG-related interfaces should go through `RouteProvider`.
  bad_message::ReceivedBadMessage(
      &process_, bad_message::ASGH_ASSOCIATED_INTERFACE_REQUEST);
}

RenderProcessHost* AgentSchedulingGroupHost::GetProcess() {
  // TODO(crbug.com/1111231): Make the condition below hold.
  // Currently the DCHECK doesn't hold, since RenderViewHostImpl outlives
  // its associated AgentSchedulingGroupHost, and the dtor queries the
  // associated RenderProcessHost to remove itself from the
  // PerProcessRenderViewHostSet and RemoveObserver() itself.
  // DCHECK_NE(state_, LifecycleState::kRenderProcessHostDestroyed);
  return &process_;
}

bool AgentSchedulingGroupHost::Init() {
  // If we are about to initialize the RenderProcessHost, it is expected that
  // `RenderProcessHost::InitializeChannelProxy()` has already been called, and
  // thus its IPC::ChannelProxy and renderer interface are usable, as are our
  // own mojos. This is because the lifetime of our mojos should match the
  // lifetime of the RenderProcessHost's IPC::ChannelProxy and renderer
  // interfaces.
  DCHECK(process_.GetRendererInterface());
  DCHECK(mojo_remote_.is_bound());
  DCHECK_EQ(state_, LifecycleState::kBound);

  return process_.Init();
}

ChannelProxy* AgentSchedulingGroupHost::GetChannel() {
  DCHECK_EQ(state_, LifecycleState::kBound);

  if (association_mode_ == IPCAssociationMode::kAssociatedWithProcess)
    return process_.GetChannel();

  DCHECK(channel_);
  return channel_.get();
}

bool AgentSchedulingGroupHost::Send(IPC::Message* message) {
  DCHECK_EQ(state_, LifecycleState::kBound);

  std::unique_ptr<IPC::Message> msg(message);

  if (association_mode_ == IPCAssociationMode::kAssociatedWithProcess)
    return process_.Send(msg.release());

  // This DCHECK is too idealistic for now - messages that are handled by
  // filters are sent as control messages since they are intercepted before
  // routing. It is put here as documentation for now, since this code would not
  // be reached until we activate `kUnassociated`.
  DCHECK_NE(message->routing_id(), MSG_ROUTING_CONTROL);

  DCHECK(channel_);
  return channel_->Send(msg.release());
}

void AgentSchedulingGroupHost::AddRoute(int32_t routing_id,
                                        Listener* listener) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  DCHECK(!listener_map_.Lookup(routing_id));
  listener_map_.AddWithID(listener, routing_id);
  process_.AddRoute(routing_id, listener);
}

void AgentSchedulingGroupHost::RemoveRoute(int32_t routing_id) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  listener_map_.Remove(routing_id);
  process_.RemoveRoute(routing_id);
}
mojom::RouteProvider* AgentSchedulingGroupHost::GetRemoteRouteProvider() {
  DCHECK_EQ(state_, LifecycleState::kBound);
  return remote_route_provider_.get();
}

void AgentSchedulingGroupHost::CreateFrame(mojom::CreateFrameParamsPtr params) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  DCHECK(process_.IsInitializedAndNotDead());
  DCHECK(mojo_remote_.is_bound());
  mojo_remote_.get()->CreateFrame(std::move(params));
}

void AgentSchedulingGroupHost::CreateView(mojom::CreateViewParamsPtr params) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  DCHECK(process_.IsInitializedAndNotDead());
  DCHECK(mojo_remote_.is_bound());
  mojo_remote_.get()->CreateView(std::move(params));
}

void AgentSchedulingGroupHost::DestroyView(
    int32_t routing_id,
    mojom::AgentSchedulingGroup::DestroyViewCallback callback) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  if (mojo_remote_.is_bound()) {
    mojo_remote_.get()->DestroyView(routing_id, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AgentSchedulingGroupHost::CreateFrameProxy(
    int32_t routing_id,
    int32_t render_view_routing_id,
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int32_t parent_routing_id,
    const FrameReplicationState& replicated_state,
    const base::UnguessableToken& frame_token,
    const base::UnguessableToken& devtools_frame_token) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  mojo_remote_.get()->CreateFrameProxy(
      routing_id, render_view_routing_id, opener_frame_token, parent_routing_id,
      replicated_state, frame_token, devtools_frame_token);
}

void AgentSchedulingGroupHost::GetRoute(
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  DCHECK(receiver.is_valid());
  associated_interface_provider_receivers_.Add(this, std::move(receiver),
                                               routing_id);
}

void AgentSchedulingGroupHost::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  int32_t routing_id =
      associated_interface_provider_receivers_.current_context();

  if (auto* listener = GetListener(routing_id))
    listener->OnAssociatedInterfaceRequest(name, receiver.PassHandle());
}

void AgentSchedulingGroupHost::ResetIPC() {
  DCHECK_EQ(state_, LifecycleState::kRenderProcessExited);
  receiver_.reset();
  mojo_remote_.reset();
  remote_route_provider_.reset();
  route_provider_receiver_.reset();
  associated_interface_provider_receivers_.Clear();
  channel_ = nullptr;
}

void AgentSchedulingGroupHost::SetUpIPC() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(state_ == LifecycleState::kNewborn ||
         state_ == LifecycleState::kRenderProcessExited);

  // The RenderProcessHostImpl's renderer interface must be initialized at this
  // point. We don't DCHECK |process_.IsInitializedAndNotDead()| here because
  // we may end up here after the renderer process has died but before
  // RenderProcessHostImpl::Init() is called. Therefore, the process can accept
  // IPCs that will be queued for the next renderer process if one is spun up.
  DCHECK(process_.GetRendererInterface());

  DCHECK(!channel_);
  DCHECK(!mojo_remote_.is_bound());
  DCHECK(!receiver_.is_bound());
  DCHECK(!remote_route_provider_.is_bound());
  DCHECK(!route_provider_receiver_.is_bound());

  // After this function returns, all of `this`'s associated mojo interfaces
  // need to be bound, and associated "properly" - in `kUnassociated` mode that
  // means they are associated with the ASG's channel, and in
  // `kAssociatedWithProcess` mode with the process-global channel. This
  // initialization is done in a number of steps:
  // 1. If we're in `kUnassociated` mode, create an IPC Channel (i.e.,
  //    initialize `channel_`). After this, regardless of which mode we're in,
  //    the ASGH would have a channel.
  // 2. Initialize `mojo_remote_`. In `kAssociatedWithProcess` mode, this can be
  //    done via the `mojom::Renderer` interface, but in `kUnassociated` mode
  //    this *has* to be done via the just-created channel (so the interface is
  //    associated with the correct pipe).
  // 3. All the ASGH's other associated interfaces can now be initialized via
  //    `mojo_remote_`, and will be transitively associated with the appropriate
  //    IPC channel/pipe.
  if (association_mode_ == IPCAssociationMode::kAssociatedWithProcess) {
    process_.GetRendererInterface()->CreateAssociatedAgentSchedulingGroup(
        mojo_remote_.BindNewEndpointAndPassReceiver());
  } else {
    auto io_task_runner = GetIOThreadTaskRunner({});

    // Empty interface endpoint to pass pipes more easily.
    PendingRemote<IPC::mojom::ChannelBootstrap> bootstrap;

    process_.GetRendererInterface()->CreateAgentSchedulingGroup(
        bootstrap.InitWithNewPipeAndPassReceiver());

    auto channel_factory = ChannelMojo::CreateServerFactory(
        bootstrap.PassPipe(), /*ipc_task_runner=*/io_task_runner,
        /*proxy_task_runner=*/base::ThreadTaskRunnerHandle::Get());

    // TODO(crbug.com/1111231): Android WebViews (that support synchronous
    // compositing) send sync messages from the browser to the renderer, and
    // therefore need a `SyncChannel`. However, we don't plan to support
    // WebViews at this stage, so a plain `ChannelProxy` is fine for now.
    channel_ = ChannelProxy::Create(
        std::move(channel_factory), /*listener=*/this,
        /*ipc_task_runner=*/io_task_runner,
        /*listener_task_runner=*/base::ThreadTaskRunnerHandle::Get());

    // TODO(crbug.com/1111231): Add necessary filters.
    // Most of the filters currently installed on the process-wide channel are:
    // 1. "Process-bound", that is, they do not handle messages sent using ASG,
    // 2. Pepper/NaCl-related, that are going away, and are not supported, or
    // 3. Related to Android WebViews, which are not currently supported.

    channel_->GetRemoteAssociatedInterface(&mojo_remote_);
  }

  DCHECK(mojo_remote_.is_bound());

  mojo_remote_.get()->BindAssociatedInterfaces(
      receiver_.BindNewEndpointAndPassRemote(),
      route_provider_receiver_.BindNewEndpointAndPassRemote(),
      remote_route_provider_.BindNewEndpointAndPassReceiver());
  SetState(LifecycleState::kBound);
}

void AgentSchedulingGroupHost::SetState(
    AgentSchedulingGroupHost::LifecycleState state) {
  static const base::NoDestructor<StateTransitions<LifecycleState>> transitions(
      StateTransitions<LifecycleState>({
          {LifecycleState::kNewborn, {LifecycleState::kBound}},
          {LifecycleState::kBound,
           {LifecycleState::kRenderProcessExited,
            // Note: kRenderProcessHostDestroyed is only reached through kBound
            //       state. Upon kRenderProcessExited, we immediately setup a
            //       unclaimed mojo endpoint to be consumed by the next
            //       renderer process.
            LifecycleState::kRenderProcessHostDestroyed}},
          {LifecycleState::kRenderProcessExited, {LifecycleState::kBound}},
      }));

  DCHECK_STATE_TRANSITION(transitions, state_, state);
  state_ = state;
}

std::ostream& operator<<(std::ostream& os,
                         AgentSchedulingGroupHost::LifecycleState state) {
  switch (state) {
    case AgentSchedulingGroupHost::LifecycleState::kNewborn:
      os << "Newborn";
      break;
    case AgentSchedulingGroupHost::LifecycleState::kBound:
      os << "Bound";
      break;
    case AgentSchedulingGroupHost::LifecycleState::kRenderProcessExited:
      os << "RenderProcessExited";
      break;
    case AgentSchedulingGroupHost::LifecycleState::kRenderProcessHostDestroyed:
      os << "RenderProcessHostDestroyed";
      break;
    default:
      os << "<invalid value: " << static_cast<int>(state) << ">";
  }
  return os;
}

Listener* AgentSchedulingGroupHost::GetListener(int32_t routing_id) {
  DCHECK_NE(routing_id, MSG_ROUTING_CONTROL);

  return listener_map_.Lookup(routing_id);
}

}  // namespace content
