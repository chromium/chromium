// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/renderer_host/agent_scheduling_group_host.h"

#include <memory>

#include "base/feature_list.h"
#include "base/supports_user_data.h"
#include "base/util/type_safety/pass_key.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"

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

using PassKey = ::util::PassKey<AgentSchedulingGroupHost>;

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
  // We don't want to bind the mojo endpoints yet, as the process may not be
  // fully initialized yet. They will be initialized the next time an API
  // requiring an IPC is called.
}

// DO NOT USE |process_| HERE! At this point it (or at least parts of it) is no
// longer valid.
AgentSchedulingGroupHost::~AgentSchedulingGroupHost() = default;

void AgentSchedulingGroupHost::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  DCHECK_EQ(host, &process_);
  ResetMojo();
}

void AgentSchedulingGroupHost::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK_EQ(host, &process_);
  process_.RemoveObserver(this);
}

RenderProcessHost* AgentSchedulingGroupHost::GetProcess() {
  return &process_;
}

bool AgentSchedulingGroupHost::InitProcessAndMojos() {
  if (!process_.Init())
    return false;

  SetUpMojoIfNeeded();
  return true;
}

ChannelProxy* AgentSchedulingGroupHost::GetChannel() {
  // TODO(crbug.com/1111231): If the process is not initialized, it also implies
  // that it is not Ready, meaning the channel we return here will not be valid.
  // In that case we should return |nullptr|, but that causes certain tests to
  // fail. This should be changed once those tests are fixed.
  if (process_.IsInitializedAndNotDead())
    SetUpMojoIfNeeded();

  return process_.GetChannel();
}

bool AgentSchedulingGroupHost::Send(IPC::Message* message) {
  // Send takes ownership of the IPC message. Since there are flows where we
  // don't call RenderProcessHost::Send, we have to make sure we delete the
  // message appropriately to avoid leaks.
  std::unique_ptr<IPC::Message> msg(message);

  if (!process_.IsInitializedAndNotDead())
    return false;

  SetUpMojoIfNeeded();
  return process_.Send(msg.release());
}

void AgentSchedulingGroupHost::AddRoute(int32_t routing_id,
                                        Listener* listener) {
  process_.AddRoute(routing_id, listener);
}

void AgentSchedulingGroupHost::RemoveRoute(int32_t routing_id) {
  process_.RemoveRoute(routing_id);
}

mojom::RouteProvider* AgentSchedulingGroupHost::GetRemoteRouteProvider() {
  // TODO(domfarolino): Remove `GetRemoteRouteProvider` from `RenderProcessHost`
  // and make `AgentSchedulingGroupHost` a fully-fledged RouteProvider.
  RenderProcessHostImpl& process =
      static_cast<RenderProcessHostImpl&>(process_);
  return process.GetRemoteRouteProvider(PassKey());
}

void AgentSchedulingGroupHost::CreateFrame(mojom::CreateFrameParamsPtr params) {
  SetUpMojoIfNeeded();
  mojo_remote_.get()->CreateFrame(std::move(params));
}

void AgentSchedulingGroupHost::CreateView(mojom::CreateViewParamsPtr params) {
  SetUpMojoIfNeeded();
  mojo_remote_.get()->CreateView(std::move(params));
}

void AgentSchedulingGroupHost::DestroyView(int32_t routing_id) {
  if (mojo_remote_.is_bound())
    mojo_remote_.get()->DestroyView(routing_id);
}

void AgentSchedulingGroupHost::CreateFrameProxy(
    int32_t routing_id,
    int32_t render_view_routing_id,
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int32_t parent_routing_id,
    const FrameReplicationState& replicated_state,
    const base::UnguessableToken& frame_token,
    const base::UnguessableToken& devtools_frame_token) {
  mojo_remote_.get()->CreateFrameProxy(
      routing_id, render_view_routing_id, opener_frame_token, parent_routing_id,
      replicated_state, frame_token, devtools_frame_token);
}

void AgentSchedulingGroupHost::GetRoute(
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  // TODO(crbug.com/1111231): Make AgentSchedulingGroupHost a fully-fledged
  // RouteProvider, so we can register routes directly with an
  // AgentSchedulingGroupHost rather than RenderProcessHostImpl.
  static_cast<RenderProcessHostImpl&>(process_).GetRoute(routing_id,
                                                         std::move(receiver));
}

void AgentSchedulingGroupHost::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  // TODO(crbug.com/1111231): Make AgentSchedulingGroupHost a fully-fledged
  // AssociatedInterfaceProvider, so we can start associating interfaces
  // directly with the AgentSchedulingGroupHost interface.
  static_cast<RenderProcessHostImpl&>(process_).GetAssociatedInterface(
      name, std::move(receiver));
}

void AgentSchedulingGroupHost::ResetMojo() {
  receiver_.reset();
  mojo_remote_.reset();
}

void AgentSchedulingGroupHost::SetUpMojoIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(process_.IsInitializedAndNotDead());

  DCHECK_EQ(receiver_.is_bound(), mojo_remote_.is_bound());
  if (receiver_.is_bound())
    return;

  if (should_associate_) {
    process_.GetRendererInterface()->CreateAssociatedAgentSchedulingGroup(
        receiver_.BindNewEndpointAndPassRemote(),
        mojo_remote_.BindNewEndpointAndPassReceiver());
  } else {
    process_.GetRendererInterface()->CreateAgentSchedulingGroup(
        receiver_.BindNewPipeAndPassRemote(),
        mojo_remote_.BindNewPipeAndPassReceiver());
  }
}

}  // namespace content
