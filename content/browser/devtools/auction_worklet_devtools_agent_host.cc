// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/auction_worklet_devtools_agent_host.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/child_process_host.h"

namespace content {

namespace {

RenderFrameHostImpl* ContainingLocalRoot(RenderFrameHostImpl* frame) {
  while (!frame->is_local_root())
    frame = frame->GetParent();
  return frame;
}

}  // namespace

bool AuctionWorkletDevToolsAgentHost::IsRelevantTo(
    RenderFrameHostImpl* frame,
    DebuggableAuctionWorklet* candidate) {
  return ContainingLocalRoot(frame) ==
         ContainingLocalRoot(candidate->owning_frame());
}

std::string AuctionWorkletDevToolsAgentHost::GetType() {
  return kTypeAuctionWorklet;
}

std::string AuctionWorkletDevToolsAgentHost::GetTitle() {
  return worklet_ ? worklet_->Title() : std::string();
}

GURL AuctionWorkletDevToolsAgentHost::GetURL() {
  return worklet_ ? worklet_->url() : GURL();
}

bool AuctionWorkletDevToolsAgentHost::Activate() {
  return false;
}

void AuctionWorkletDevToolsAgentHost::Reload() {}

bool AuctionWorkletDevToolsAgentHost::Close() {
  return false;
}

std::string AuctionWorkletDevToolsAgentHost::GetParentId() {
  if (!worklet_)
    return std::string();

  DevToolsAgentHostImpl* parent =
      RenderFrameDevToolsAgentHost::GetFor(worklet_->owning_frame());
  if (!parent)
    return std::string();

  return parent->GetId();
}

BrowserContext* AuctionWorkletDevToolsAgentHost::GetBrowserContext() {
  return worklet_ ? worklet_->owning_frame()->GetBrowserContext() : nullptr;
}

bool AuctionWorkletDevToolsAgentHost::AttachSession(DevToolsSession* session,
                                                    bool acquire_wake_lock) {
  // We use `force_using_io_session` as AuctionV8DevToolsSession can't handle
  // commands on main session when blocked on a breakpoint, only on IO session.
  session->AttachToAgent(associated_agent_remote_.get(),
                         /*force_using_io_session=*/true);
  return true;
}

// static
scoped_refptr<AuctionWorkletDevToolsAgentHost>
AuctionWorkletDevToolsAgentHost::Create(DebuggableAuctionWorklet* worklet) {
  auto self =
      base::WrapRefCounted(new AuctionWorkletDevToolsAgentHost(worklet));
  // This needs to be outside of constructor due to bind until we make the base
  // class a `StartRefCountFromOne` (i.e. REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE).
  if (auto pid_opt = worklet->GetPid(base::BindOnce(
          &AuctionWorkletDevToolsAgentHost::SetProcessId, self))) {
    self->SetProcessId(*pid_opt);
  }
  return self;
}

AuctionWorkletDevToolsAgentHost::AuctionWorkletDevToolsAgentHost(
    DebuggableAuctionWorklet* worklet)
    : DevToolsAgentHostImpl(worklet->UniqueId()), worklet_(worklet) {
  mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgent> agent;
  worklet->ConnectDevToolsAgent(agent.InitWithNewEndpointAndPassReceiver());
  NotifyCreated();

  associated_agent_remote_.Bind(std::move(agent));

  // Since we were just created, we don't have any sessions yet, so nothing to
  // attach here.
  DCHECK(sessions().empty());
}

AuctionWorkletDevToolsAgentHost::~AuctionWorkletDevToolsAgentHost() = default;

void AuctionWorkletDevToolsAgentHost::WorkletDestroyed() {
  worklet_ = nullptr;
  auto retain_this = ForceDetachAllSessionsImpl();
  associated_agent_remote_.reset();
}

AuctionWorkletDevToolsAgentHostManager&
AuctionWorkletDevToolsAgentHostManager::GetInstance() {
  static base::NoDestructor<AuctionWorkletDevToolsAgentHostManager> instance;
  return *instance;
}

AuctionWorkletDevToolsAgentHostManager::
    AuctionWorkletDevToolsAgentHostManager() {
  DebuggableAuctionWorkletTracker::GetInstance()->AddObserver(this);
}

AuctionWorkletDevToolsAgentHostManager::
    ~AuctionWorkletDevToolsAgentHostManager() {
  NOTREACHED_IN_MIGRATION();
}

void AuctionWorkletDevToolsAgentHostManager::GetAll(
    DevToolsAgentHost::List* out) {
  DevToolsAgentHost::List result;
  std::vector<DebuggableAuctionWorklet*> to_wrap =
      DebuggableAuctionWorkletTracker::GetInstance()->GetAll();
  for (DebuggableAuctionWorklet* item : to_wrap)
    out->push_back(GetOrCreateFor(item));
}

void AuctionWorkletDevToolsAgentHostManager::GetAllForFrame(
    RenderFrameHostImpl* frame,
    DevToolsAgentHost::List* out) {
  std::vector<DebuggableAuctionWorklet*> candidates =
      DebuggableAuctionWorkletTracker::GetInstance()->GetAll();
  for (DebuggableAuctionWorklet* item : candidates) {
    if (AuctionWorkletDevToolsAgentHost::IsRelevantTo(frame, item))
      out->push_back(GetOrCreateFor(item));
  }
}

scoped_refptr<AuctionWorkletDevToolsAgentHost>
AuctionWorkletDevToolsAgentHostManager::GetOrCreateFor(
    DebuggableAuctionWorklet* worklet) {
  auto it = hosts_.find(worklet);
  if (it != hosts_.end())
    return it->second;

  auto host = AuctionWorkletDevToolsAgentHost::Create(worklet);
  hosts_.insert(std::make_pair(worklet, host));
  return host;
}

void AuctionWorkletDevToolsAgentHostManager::AuctionWorkletCreated(
    DebuggableAuctionWorklet* worklet,
    bool& should_pause_on_start) {
  if (AuctionWorkletDevToolsAgentHost::ShouldForceCreation())
    GetOrCreateFor(worklet);
}

void AuctionWorkletDevToolsAgentHostManager::AuctionWorkletDestroyed(
    DebuggableAuctionWorklet* worklet) {
  auto it = hosts_.find(worklet);
  if (it == hosts_.end())
    return;
  it->second->WorkletDestroyed();
  hosts_.erase(it);
}

}  // namespace content
