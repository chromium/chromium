// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/auction_worklet_devtools_agent_host.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/child_process_host.h"

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
  return true;
}

AuctionWorkletDevToolsAgentHost::AuctionWorkletDevToolsAgentHost(
    DebuggableAuctionWorklet* worklet)
    : DevToolsAgentHostImpl(base::GenerateGUID()), worklet_(worklet) {
  mojo::PendingRemote<blink::mojom::DevToolsAgent> agent;
  worklet->ConnectDevToolsAgent(agent.InitWithNewPipeAndPassReceiver());
  NotifyCreated();
  GetRendererChannel()->SetRenderer(std::move(agent), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
}

AuctionWorkletDevToolsAgentHost::~AuctionWorkletDevToolsAgentHost() = default;

void AuctionWorkletDevToolsAgentHost::WorkletDestroyed() {
  worklet_ = nullptr;
  ForceDetachAllSessions();
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
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
  NOTREACHED();
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

  auto host =
      base::WrapRefCounted(new AuctionWorkletDevToolsAgentHost(worklet));
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
