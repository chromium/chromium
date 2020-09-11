// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/renderer_host/agent_scheduling_group_host.h"

#include <memory>

#include "base/supports_user_data.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

using ::IPC::ChannelProxy;
using ::IPC::Listener;
using ::mojo::PendingRemote;
using ::mojo::Receiver;

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

AgentSchedulingGroupHost::AgentSchedulingGroupHost(RenderProcessHost& process)
    : process_(process) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  process_.GetRendererInterface()->CreateAgentSchedulingGroup(
      receiver_.BindNewPipeAndPassRemote(),
      mojo_remote_.BindNewPipeAndPassReceiver());
}

// DO NOT USE |process_| HERE! At this point it (or at least parts of it) is no
// longer valid.
AgentSchedulingGroupHost::~AgentSchedulingGroupHost() = default;

RenderProcessHost* AgentSchedulingGroupHost::GetProcess() {
  return &process_;
}

ChannelProxy* AgentSchedulingGroupHost::GetChannel() {
  return process_.GetChannel();
}

bool AgentSchedulingGroupHost::Send(IPC::Message* message) {
  return process_.Send(message);
}

void AgentSchedulingGroupHost::AddRoute(int32_t routing_id,
                                        Listener* listener) {
  process_.AddRoute(routing_id, listener);
}

void AgentSchedulingGroupHost::RemoveRoute(int32_t routing_id) {
  process_.RemoveRoute(routing_id);
}

mojom::RouteProvider* AgentSchedulingGroupHost::GetRemoteRouteProvider() {
  RenderProcessHostImpl& process =
      static_cast<RenderProcessHostImpl&>(process_);
  return process.GetRemoteRouteProvider();
}

void AgentSchedulingGroupHost::CreateFrame(mojom::CreateFrameParamsPtr params) {
  process_.GetRendererInterface()->CreateFrame(std::move(params));
}

void AgentSchedulingGroupHost::CreateView(mojom::CreateViewParamsPtr params) {
  // TODO(crbug.com/1111231): Ensure that the mojo endpoints are still connected
  // (once crrev.com/c/2397057 is submitted).
  process_.GetRendererInterface()->CreateView(std::move(params));
}

void AgentSchedulingGroupHost::DestroyView(int32_t routing_id) {
  process_.GetRendererInterface()->DestroyView(routing_id);
}

}  // namespace content
