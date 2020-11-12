// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/renderer_host/agent_scheduling_group_host.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/supports_user_data.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/state_transitions.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "ipc/ipc_message.h"

namespace content {

namespace {

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

// MaybeAssociatedReceiver:
AgentSchedulingGroupHost::MaybeAssociatedReceiver::MaybeAssociatedReceiver(
    AgentSchedulingGroupHost& host,
    bool should_associate) {
  if (should_associate) {
    receiver_or_monostate_
        .emplace<AssociatedReceiver<mojom::AgentSchedulingGroupHost>>(&host);
    receiver_ = &absl::get<AssociatedReceiver<mojom::AgentSchedulingGroupHost>>(
        receiver_or_monostate_);
  } else {
    receiver_or_monostate_.emplace<Receiver<mojom::AgentSchedulingGroupHost>>(
        &host);
    receiver_ = &absl::get<Receiver<mojom::AgentSchedulingGroupHost>>(
        receiver_or_monostate_);
  }
}

AgentSchedulingGroupHost::MaybeAssociatedReceiver::~MaybeAssociatedReceiver() =
    default;

PendingRemote<mojom::AgentSchedulingGroupHost>
AgentSchedulingGroupHost::MaybeAssociatedReceiver::BindNewPipeAndPassRemote() {
  return absl::get<Receiver<mojom::AgentSchedulingGroupHost>*>(receiver_)
      ->BindNewPipeAndPassRemote();
}

PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
AgentSchedulingGroupHost::MaybeAssociatedReceiver::
    BindNewEndpointAndPassRemote() {
  return absl::get<AssociatedReceiver<mojom::AgentSchedulingGroupHost>*>(
             receiver_)
      ->BindNewEndpointAndPassRemote();
}

void AgentSchedulingGroupHost::MaybeAssociatedReceiver::reset() {
  absl::visit([](auto* r) { r->reset(); }, receiver_);
}

bool AgentSchedulingGroupHost::MaybeAssociatedReceiver::is_bound() {
  return absl::visit([](auto* r) { return r->is_bound(); }, receiver_);
}

// MaybeAssociatedRemote:
AgentSchedulingGroupHost::MaybeAssociatedRemote::MaybeAssociatedRemote(
    bool should_associate) {
  if (should_associate) {
    remote_ = AssociatedRemote<mojom::AgentSchedulingGroup>();
  } else {
    remote_ = Remote<mojom::AgentSchedulingGroup>();
  }
}

AgentSchedulingGroupHost::MaybeAssociatedRemote::~MaybeAssociatedRemote() =
    default;

PendingReceiver<mojom::AgentSchedulingGroup>
AgentSchedulingGroupHost::MaybeAssociatedRemote::BindNewPipeAndPassReceiver() {
  return absl::get<Remote<mojom::AgentSchedulingGroup>>(remote_)
      .BindNewPipeAndPassReceiver();
}

PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
AgentSchedulingGroupHost::MaybeAssociatedRemote::
    BindNewEndpointAndPassReceiver() {
  return absl::get<AssociatedRemote<mojom::AgentSchedulingGroup>>(remote_)
      .BindNewEndpointAndPassReceiver();
}

void AgentSchedulingGroupHost::MaybeAssociatedRemote::reset() {
  absl::visit([](auto& remote) { remote.reset(); }, remote_);
}

bool AgentSchedulingGroupHost::MaybeAssociatedRemote::is_bound() {
  return absl::visit([](auto& remote) { return remote.is_bound(); }, remote_);
}

mojom::AgentSchedulingGroup*
AgentSchedulingGroupHost::MaybeAssociatedRemote::get() {
  return absl::visit([](auto& r) { return r.get(); }, remote_);
}

// AgentSchedulingGroupHost:

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

AgentSchedulingGroupHost::AgentSchedulingGroupHost(RenderProcessHost& process)
    : AgentSchedulingGroupHost(
          process,
          !base::FeatureList::IsEnabled(
              features::kMbiDetachAgentSchedulingGroupFromChannel)) {}

AgentSchedulingGroupHost::AgentSchedulingGroupHost(RenderProcessHost& process,
                                                   bool should_associate)
    : process_(process),
      should_associate_(should_associate),
      receiver_(*this, should_associate),
      mojo_remote_(should_associate) {
  process_.AddObserver(this);

  // The RenderProcessHost's channel and other mojo interfaces are initialized
  // by the time this class is constructed, so we eagerly initialize this
  // class's mojos so they have the same bind lifetime as those of the
  // RenderProcessHost. Furthermore, when the RenderProcessHost's channel and
  // mojo interfaces get reset and reinitialized, we'll be notified so that we
  // can reset and reinitialize ours as well.
  SetUpMojoIfNeeded();
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
  ResetMojo();

  // RenderProcessHostImpl will attempt to call this method later if it has not
  // already been called. We call it now since `SetUpMojoIfNeeded()` relies on
  // it being called, thus setting up the IPC channel and mojom::Renderer
  // interface.
  process_.EnableSendQueue();

  // We call this so that we can immediately queue IPC and mojo messages on the
  // new channel/interfaces that are bound for the next renderer process, should
  // one eventually be spun up.
  SetUpMojoIfNeeded();
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
  return process_.GetChannel();
}

bool AgentSchedulingGroupHost::Send(IPC::Message* message) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  return process_.Send(message);
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
  if (mojo_remote_.is_bound())
    mojo_remote_.get()->DestroyView(routing_id, std::move(callback));
  else
    std::move(callback).Run();
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

void AgentSchedulingGroupHost::ResetMojo() {
  DCHECK_EQ(state_, LifecycleState::kRenderProcessExited);
  receiver_.reset();
  mojo_remote_.reset();
  remote_route_provider_.reset();
  route_provider_receiver_.reset();
  associated_interface_provider_receivers_.Clear();
}

void AgentSchedulingGroupHost::SetUpMojoIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We don't DCHECK |process_.IsInitializedAndNotDead()| here because we may
  // end up here after the render process has died but before the
  // RenderProcessHostImpl is re-initialized (and thus not considered dead
  // anymore).

  // The RenderProcessHostImpl's renderer interface must be initialized at this
  // at this point.
  DCHECK(process_.GetRendererInterface());

  // Make sure that the bind state of all of this class's mojos are equivalent.
  if (state_ == LifecycleState::kBound) {
    DCHECK(mojo_remote_.is_bound());
    DCHECK(receiver_.is_bound());
    DCHECK(remote_route_provider_.is_bound());
    DCHECK(route_provider_receiver_.is_bound());
    return;
  }

  DCHECK(!mojo_remote_.is_bound());
  DCHECK(!receiver_.is_bound());
  DCHECK(!remote_route_provider_.is_bound());
  DCHECK(!route_provider_receiver_.is_bound());

  DCHECK(state_ == LifecycleState::kNewborn ||
         state_ == LifecycleState::kRenderProcessExited);

  if (should_associate_) {
    process_.GetRendererInterface()->CreateAssociatedAgentSchedulingGroup(
        receiver_.BindNewEndpointAndPassRemote(),
        mojo_remote_.BindNewEndpointAndPassReceiver());
  } else {
    process_.GetRendererInterface()->CreateAgentSchedulingGroup(
        receiver_.BindNewPipeAndPassRemote(),
        mojo_remote_.BindNewPipeAndPassReceiver());
  }

  mojo_remote_.get()->BindAssociatedRouteProvider(
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
