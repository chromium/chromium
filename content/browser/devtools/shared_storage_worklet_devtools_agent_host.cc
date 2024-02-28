// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_storage_worklet_devtools_agent_host.h"

#include <memory>
#include <utility>

#include "base/strings/strcat.h"
#include "content/browser/devtools/devtools_renderer_channel.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace content {

namespace {

RenderFrameHostImpl* ContainingLocalRoot(RenderFrameHostImpl* frame) {
  while (!frame->is_local_root()) {
    frame = frame->GetParent();
  }
  return frame;
}

}  // namespace

SharedStorageWorkletDevToolsAgentHost::SharedStorageWorkletDevToolsAgentHost(
    SharedStorageWorkletHost& worklet_host,
    const base::UnguessableToken& devtools_worklet_token)
    : DevToolsAgentHostImpl(devtools_worklet_token.ToString()),
      auto_attacher_(std::make_unique<protocol::RendererAutoAttacherBase>(
          GetRendererChannel())),
      worklet_host_(&worklet_host) {
  NotifyCreated();
}

SharedStorageWorkletDevToolsAgentHost::
    ~SharedStorageWorkletDevToolsAgentHost() = default;

BrowserContext* SharedStorageWorkletDevToolsAgentHost::GetBrowserContext() {
  if (!worklet_host_ || !worklet_host_->GetProcessHost()) {
    return nullptr;
  }

  return worklet_host_->GetProcessHost()->GetBrowserContext();
}

std::string SharedStorageWorkletDevToolsAgentHost::GetType() {
  return kTypeSharedStorageWorklet;
}

std::string SharedStorageWorkletDevToolsAgentHost::GetTitle() {
  if (!worklet_host_) {
    return std::string();
  }

  return base::StrCat({"Shared storage worklet for ",
                       worklet_host_->script_source_url().spec()});
}

GURL SharedStorageWorkletDevToolsAgentHost::GetURL() {
  return worklet_host_ ? worklet_host_->script_source_url() : GURL();
}

bool SharedStorageWorkletDevToolsAgentHost::Activate() {
  return false;
}

void SharedStorageWorkletDevToolsAgentHost::Reload() {}

bool SharedStorageWorkletDevToolsAgentHost::Close() {
  return false;
}

bool SharedStorageWorkletDevToolsAgentHost::AttachSession(
    DevToolsSession* session,
    bool acquire_wake_lock) {
  session->CreateAndAddHandler<protocol::InspectorHandler>();
  session->CreateAndAddHandler<protocol::TargetHandler>(
      protocol::TargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      auto_attacher_.get(), session);
  return true;
}

void SharedStorageWorkletDevToolsAgentHost::WorkletReadyForInspection(
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
        agent_host_receiver) {
  // The process can be null here when the worklet is in its keep-alive stage
  // and the browser is shutting down.
  if (!worklet_host_->GetProcessHost()) {
    return;
  }

  GetRendererChannel()->SetRenderer(std::move(agent_remote),
                                    std::move(agent_host_receiver),
                                    worklet_host_->GetProcessHost()->GetID());
}

void SharedStorageWorkletDevToolsAgentHost::WorkletDestroyed() {
  CHECK(worklet_host_);
  worklet_host_ = nullptr;

  for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this)) {
    inspector->TargetCrashed();
  }
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
}

bool SharedStorageWorkletDevToolsAgentHost::IsRelevantTo(
    RenderFrameHostImpl* frame) {
  if (!worklet_host_->GetFrame()) {
    return false;
  }

  return ContainingLocalRoot(frame) ==
         ContainingLocalRoot(worklet_host_->GetFrame());
}

protocol::TargetAutoAttacher*
SharedStorageWorkletDevToolsAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

}  // namespace content
