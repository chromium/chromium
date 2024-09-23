// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/renderer_host/agent_scheduling_group_host.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/agent_scheduling_group_host_factory.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom.h"

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
#include "content/public/browser/browser_message_filter.h"
#endif

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

static constexpr char kAgentSchedulingGroupHostDataKey[] =
    "AgentSchedulingGroupHostUserDataKey";

AgentSchedulingGroupHostFactory* g_agent_scheduling_group_host_factory_ =
    nullptr;

// This is a struct that is owned by RenderProcessHost. It carries data
// structures that store the AgentSchedulingGroups associated with a
// RenderProcessHost.
struct AgentSchedulingGroupHostUserData : public base::SupportsUserData::Data {
 public:
  AgentSchedulingGroupHostUserData() = default;
  ~AgentSchedulingGroupHostUserData() override = default;

  std::set<std::unique_ptr<AgentSchedulingGroupHost>, base::UniquePtrComparator>
      owned_host_set;
  // This is used solely to DCHECK the invariant that a SiteInstanceGroup cannot
  // request an AgentSchedulingGroup twice from the same RenderProcessHost.
#if DCHECK_IS_ON()
  std::set<raw_ptr<const SiteInstanceGroup, SetExperimental>>
      site_instance_groups;
#endif
};

static features::MBIMode GetMBIMode() {
  return base::FeatureList::IsEnabled(features::kMBIMode)
             ? features::kMBIModeParam.Get()
             : features::MBIMode::kLegacy;
}

}  // namespace

// static
AgentSchedulingGroupHost* AgentSchedulingGroupHost::GetOrCreate(
    const SiteInstanceGroup& site_instance_group,
    RenderProcessHost& process) {
  AgentSchedulingGroupHostUserData* data =
      static_cast<AgentSchedulingGroupHostUserData*>(
          process.GetUserData(kAgentSchedulingGroupHostDataKey));

  if (!data) {
    process.SetUserData(kAgentSchedulingGroupHostDataKey,
                        std::make_unique<AgentSchedulingGroupHostUserData>());
    data = static_cast<AgentSchedulingGroupHostUserData*>(
        process.GetUserData(kAgentSchedulingGroupHostDataKey));
  }

  DCHECK(data);

  if (GetMBIMode() == features::MBIMode::kLegacy ||
      GetMBIMode() == features::MBIMode::kEnabledPerRenderProcessHost) {
    // We don't use |data->site_instance_groups| at all when
    // AgentSchedulingGroupHost is 1:1 with RenderProcessHost.
#if DCHECK_IS_ON()
    DCHECK(data->site_instance_groups.empty());
#endif

    if (data->owned_host_set.empty()) {
      std::unique_ptr<AgentSchedulingGroupHost> host =
          g_agent_scheduling_group_host_factory_
              ? g_agent_scheduling_group_host_factory_
                    ->CreateAgentSchedulingGroupHost(process)
              : std::make_unique<AgentSchedulingGroupHost>(process);
      data->owned_host_set.insert(std::move(host));
    }

    // When we are in an MBI mode that creates AgentSchedulingGroups 1:1 with
    // RenderProcessHosts, we expect to know about at most one
    // AgentSchedulingGroupHost, since it should be the only one associated
    // with the RenderProcessHost.
    DCHECK_EQ(data->owned_host_set.size(), 1ul);
    return data->owned_host_set.begin()->get();
  }

  DCHECK_EQ(GetMBIMode(), features::MBIMode::kEnabledPerSiteInstance);

  // If we're in an MBI mode that creates multiple AgentSchedulingGroupHosts
  // per RenderProcessHost, then this will be called whenever SiteInstance needs
  // a newly-created AgentSchedulingGroupHost, so we create it here.
  std::unique_ptr<AgentSchedulingGroupHost> host =
      std::make_unique<AgentSchedulingGroupHost>(process);
  AgentSchedulingGroupHost* return_host = host.get();

  // In the MBI mode where AgentSchedulingGroupHosts are 1:1 with
  // SiteInstanceGroups, a SiteInstanceGroup may see different
  // RenderProcessHosts throughout its lifetime, but it should only ever see a
  // single AgentSchedulingGroupHost for a given RenderProcessHost.
#if DCHECK_IS_ON()
  DCHECK(!base::Contains(data->site_instance_groups, &site_instance_group));
  data->site_instance_groups.insert(&site_instance_group);
#endif

  data->owned_host_set.insert(std::move(host));
  return return_host;
}

int32_t AgentSchedulingGroupHost::GetNextID() {
  static int32_t next_id = 0;
  return next_id++;
}

AgentSchedulingGroupHost::AgentSchedulingGroupHost(RenderProcessHost& process)
    : process_(process.GetSafeRef()), receiver_(this) {
  process_->AddObserver(this);

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
  DCHECK_EQ(host, &*process_);

  // We mirror the RenderProcessHost flow here by resetting our mojos, and
  // reinitializing them once the process's IPC::ChannelProxy and renderer
  // interface are reinitialized.
  ResetIPC();

  // We don't want to reinitialize the RenderProcessHost's IPC channel when
  // we are going to immediately get a call to RenderProcessHostDestroyed.
  if (!process_->IsDeletingSoon()) {
    // RenderProcessHostImpl will attempt to call this method later if it has
    // not already been called. We call it now since `SetUpIPC()` relies on it
    // being called, thus setting up the IPC channel and mojom::Renderer
    // interface.
    process_->EnableSendQueue();

    // We call this so that we can immediately queue IPC and mojo messages on
    // the new channel/interfaces that are bound for the next renderer process,
    // should one eventually be spun up.
    SetUpIPC();
  }
}

void AgentSchedulingGroupHost::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  if (RenderProcessHost::run_renderer_in_process()) {
    // In single process mode, RenderProcessExited call is sometimes omitted.
    if (state_ != LifecycleState::kBound) {
      RenderProcessExited(host, ChildProcessTerminationInfo());
    }
  }
  DCHECK(state_ == LifecycleState::kBound ||
         state_ == LifecycleState::kRenderProcessExited);

  DCHECK_EQ(host, &*process_);
  process_->RemoveObserver(this);
  SetState(LifecycleState::kRenderProcessHostDestroyed);
}

bool AgentSchedulingGroupHost::OnMessageReceived(const IPC::Message& message) {
  if (message.routing_id() == MSG_ROUTING_CONTROL) {
    bad_message::ReceivedBadMessage(&*process_,
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
  return process_->OnBadMessageReceived(message);
}

void AgentSchedulingGroupHost::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  // There shouldn't be any interfaces requested this way - process-wide
  // interfaces should be requested via the process-wide channel, and
  // ASG-related interfaces should go through `RouteProvider`.
  bad_message::ReceivedBadMessage(
      &*process_, bad_message::ASGH_ASSOCIATED_INTERFACE_REQUEST);
}

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
void AgentSchedulingGroupHost::AddFilter(BrowserMessageFilter* filter) {
  DCHECK(filter);
  // When MBI mode is disabled, we forward these kinds of requests straight to
  // the underlying `RenderProcessHost`.
  if (GetMBIMode() == features::MBIMode::kLegacy) {
    process_->AddFilter(filter);
    return;
  }

  channel_->AddFilter(filter->GetFilter());
}
#endif

RenderProcessHost* AgentSchedulingGroupHost::GetProcess() {
  // `process_` can still be accessed here even if `state_` has been set to
  // `kRenderProcessHostDestroyed`. This is because a `RenderProcessHostImpl` is
  // scheduled to be destroyed asynchronously after the
  // `RenderProcessHostDestroyed()` observer notification is dispatched, so
  // `process_` and `this` may still be around within that gap.
  return &*process_;
}

bool AgentSchedulingGroupHost::Init() {
  // If we are about to initialize the RenderProcessHost, it is expected that
  // `RenderProcessHost::InitializeChannelProxy()` has already been called, and
  // thus its IPC::ChannelProxy and renderer interface are usable, as are our
  // own mojos. This is because the lifetime of our mojos should match the
  // lifetime of the RenderProcessHost's IPC::ChannelProxy and renderer
  // interfaces.
  DCHECK(process_->GetRendererInterface());
  DCHECK(mojo_remote_.is_bound());
  DCHECK_EQ(state_, LifecycleState::kBound);

  return process_->Init();
}

base::SafeRef<AgentSchedulingGroupHost> AgentSchedulingGroupHost::GetSafeRef()
    const {
  return weak_ptr_factory_.GetSafeRef();
}

ChannelProxy* AgentSchedulingGroupHost::GetChannel() {
  DCHECK_EQ(state_, LifecycleState::kBound);

  if (GetMBIMode() == features::MBIMode::kLegacy)
    return process_->GetChannel();

  DCHECK(channel_);
  return channel_.get();
}

bool AgentSchedulingGroupHost::Send(IPC::Message* message) {
  DCHECK_EQ(state_, LifecycleState::kBound);

  std::unique_ptr<IPC::Message> msg(message);

  if (GetMBIMode() == features::MBIMode::kLegacy)
    return process_->Send(msg.release());

  // This DCHECK is too idealistic for now - messages that are handled by
  // filters are sent as control messages since they are intercepted before
  // routing. It is put here as documentation for now, since this code would not
  // be reached until we activate
  // `features::MBIMode::kEnabledPerRenderProcessHost` or
  // `features::MBIMode::kEnabledPerSiteInstance`.
  DCHECK_NE(message->routing_id(), MSG_ROUTING_CONTROL);

  DCHECK(channel_);
  return channel_->Send(msg.release());
}

void AgentSchedulingGroupHost::AddRoute(int32_t routing_id,
                                        Listener* listener) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  DCHECK(!listener_map_.Lookup(routing_id));
  listener_map_.AddWithID(listener, routing_id);
  process_->AddRoute(routing_id, listener);
}

void AgentSchedulingGroupHost::RemoveRoute(int32_t routing_id) {
  TRACE_EVENT0("navigation", "AgentSchedulingGroupHost::RemoveRoute");
  base::ScopedUmaHistogramTimer histogram_timer(
      "Navigation.AgentSchedulingGroupHost.RemoveRoute");
  DCHECK_EQ(state_, LifecycleState::kBound);
  listener_map_.Remove(routing_id);
  process_->RemoveRoute(routing_id);
}
mojom::RouteProvider* AgentSchedulingGroupHost::GetRemoteRouteProvider() {
  DCHECK_EQ(state_, LifecycleState::kBound);
  return remote_route_provider_.get();
}

void AgentSchedulingGroupHost::CreateFrame(mojom::CreateFrameParamsPtr params) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  DCHECK(process_->IsInitializedAndNotDead());
  DCHECK(mojo_remote_.is_bound());
  mojo_remote_.get()->CreateFrame(std::move(params));
}

void AgentSchedulingGroupHost::CreateView(mojom::CreateViewParamsPtr params) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  DCHECK(process_->IsInitializedAndNotDead());
  DCHECK(mojo_remote_.is_bound());
  mojo_remote_.get()->CreateView(std::move(params));
}

void AgentSchedulingGroupHost::CreateSharedStorageWorkletService(
    mojo::PendingReceiver<blink::mojom::SharedStorageWorkletService> receiver,
    blink::mojom::WorkletGlobalScopeCreationParamsPtr
        global_scope_creation_params) {
  DCHECK_EQ(state_, LifecycleState::kBound);
  DCHECK(process_->IsInitializedAndNotDead());
  DCHECK(mojo_remote_.is_bound());
  mojo_remote_.get()->CreateSharedStorageWorkletService(
      std::move(receiver), std::move(global_scope_creation_params));
}

// static
void AgentSchedulingGroupHost::
    set_agent_scheduling_group_host_factory_for_testing(
        AgentSchedulingGroupHostFactory* asgh_factory) {
  g_agent_scheduling_group_host_factory_ = asgh_factory;
}

// static
AgentSchedulingGroupHostFactory* AgentSchedulingGroupHost::
    get_agent_scheduling_group_host_factory_for_testing() {
  DCHECK(g_agent_scheduling_group_host_factory_);
  return g_agent_scheduling_group_host_factory_;
}

void AgentSchedulingGroupHost::DidUnloadRenderFrame(
    const blink::LocalFrameToken& frame_token) {
  // |frame_host| could be null if we decided to remove the RenderFrameHostImpl
  // because the Unload request took too long.
  if (auto* frame_host =
          RenderFrameHostImpl::FromFrameToken(process_->GetID(), frame_token)) {
    frame_host->OnUnloadACK();
  }
}

void AgentSchedulingGroupHost::ResetIPC() {
  DCHECK_EQ(state_, LifecycleState::kRenderProcessExited);
  receiver_.reset();
  mojo_remote_.reset();
  remote_route_provider_.reset();
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
  DCHECK(process_->GetRendererInterface());

  DCHECK(!channel_);
  DCHECK(!mojo_remote_.is_bound());
  DCHECK(!receiver_.is_bound());
  DCHECK(!remote_route_provider_.is_bound());

  // After this function returns, all of `this`'s mojo interfaces need to be
  // bound, and associated interfaces need to be associated "properly" - in
  // `features::MBIMode::kEnabledPerRenderProcessHost` and
  // `features::MBIMode::kEnabledPerSiteInstance` mode that means they are
  // associated with the ASG's legacy IPC channel, and in
  // `features::MBIMode::kLegacy` mode, with the process-global legacy IPC
  // channel. This initialization is done in a number of steps:
  // 1. If we're not in `kLegacy` mode, create an IPC Channel (i.e., initialize
  //    `channel_`). After this, regardless of which mode we're in, the
  //    ASGH would have a channel.
  // 2. Initialize `mojo_remote_`. In `kLegacy` mode, this can be done via the
  //    `mojom::Renderer` interface, but otherwise this *has* to be done via the
  //     just-created channel (so the interface is associated with the correct
  //     pipe).
  // 3. All the ASGH's other associated interfaces can now be initialized via
  //    `mojo_remote_`, and will be transitively associated with the appropriate
  //    IPC channel/pipe.
  if (GetMBIMode() == features::MBIMode::kLegacy) {
    process_->GetRendererInterface()->CreateAssociatedAgentSchedulingGroup(
        mojo_remote_.BindNewEndpointAndPassReceiver());
  } else {
    auto io_task_runner = GetIOThreadTaskRunner({});

    // Empty interface endpoint to pass pipes more easily.
    PendingRemote<IPC::mojom::ChannelBootstrap> bootstrap;

    process_->GetRendererInterface()->CreateAgentSchedulingGroup(
        bootstrap.InitWithNewPipeAndPassReceiver());

    auto channel_factory = ChannelMojo::CreateServerFactory(
        bootstrap.PassPipe(), /*ipc_task_runner=*/io_task_runner,
        /*proxy_task_runner=*/
        base::SingleThreadTaskRunner::GetCurrentDefault());

    channel_ =
        ChannelProxy::Create(std::move(channel_factory), /*listener=*/this,
                             /*ipc_task_runner=*/io_task_runner,
                             /*listener_task_runner=*/
                             base::SingleThreadTaskRunner::GetCurrentDefault());

    // TODO(crbug.com/40142495): Add necessary filters.
    // Most of the filters currently installed on the process-wide channel are:
    // 1. "Process-bound", that is, they do not handle messages sent using ASG,
    // 2. Pepper/NaCl-related, that are going away, and are not supported, or
    // 3. Related to Android WebViews, which are not currently supported.

    channel_->GetRemoteAssociatedInterface(&mojo_remote_);
  }

  DCHECK(mojo_remote_.is_bound());

  mojo_remote_.get()->BindAssociatedInterfaces(
      receiver_.BindNewEndpointAndPassRemote(),
      remote_route_provider_.BindNewEndpointAndPassReceiver());
  SetState(LifecycleState::kBound);
}

void AgentSchedulingGroupHost::SetState(
    AgentSchedulingGroupHost::LifecycleState state) {
  static const base::NoDestructor<base::StateTransitions<LifecycleState>>
      transitions(base::StateTransitions<LifecycleState>({
          {LifecycleState::kNewborn, {LifecycleState::kBound}},
          {LifecycleState::kBound,
           {LifecycleState::kRenderProcessExited,
            // Note: If a renderer process is never spawned to claim the
            //       mojo endpoint created at initialization, then we will
            //       skip straight to the destroyed state.
            LifecycleState::kRenderProcessHostDestroyed}},
          {LifecycleState::kRenderProcessExited,
           {LifecycleState::kBound,
            LifecycleState::kRenderProcessHostDestroyed}},
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
