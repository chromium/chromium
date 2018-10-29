// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_renderer_channel.h"

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/public/common/child_process_host.h"
#include "ui/gfx/geometry/point.h"

namespace content {

DevToolsRendererChannel::DevToolsRendererChannel(DevToolsAgentHostImpl* owner)
    : owner_(owner),
      binding_(this),
      process_id_(ChildProcessHost::kInvalidUniqueID) {}

DevToolsRendererChannel::~DevToolsRendererChannel() = default;

void DevToolsRendererChannel::SetRenderer(
    blink::mojom::DevToolsAgentAssociatedPtr agent_ptr,
    blink::mojom::DevToolsAgentHostAssociatedRequest host_request,
    int process_id,
    RenderFrameHostImpl* frame_host) {
  binding_.Close();
  if (host_request)
    binding_.Bind(std::move(host_request));
  agent_ptr_ = std::move(agent_ptr);
  process_id_ = process_id;
  frame_host_ = frame_host;
  for (DevToolsSession* session : owner_->sessions()) {
    for (auto& pair : session->handlers())
      pair.second->SetRenderer(process_id_, frame_host_);
    session->AttachToAgent(agent_ptr_.get());
  }
}

void DevToolsRendererChannel::AttachSession(DevToolsSession* session) {
  if (!agent_ptr_)
    owner_->UpdateRendererChannel(true /* force */);
  for (auto& pair : session->handlers())
    pair.second->SetRenderer(process_id_, frame_host_);
  session->AttachToAgent(agent_ptr_.get());
}

void DevToolsRendererChannel::InspectElement(const gfx::Point& point) {
  if (!agent_ptr_)
    owner_->UpdateRendererChannel(true /* force */);
  // Previous call might update |agent_ptr_|
  // via SetRenderer(), so we should check it again.
  if (agent_ptr_)
    agent_ptr_->InspectElement(point);
}

}  // namespace content
