// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/inspector_handler.h"

#include <memory>

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {
namespace protocol {

InspectorHandler::InspectorHandler()
    : DevToolsDomainHandler(Inspector::Metainfo::domainName) {}

InspectorHandler::~InspectorHandler() = default;

// static
std::vector<InspectorHandler*> InspectorHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<InspectorHandler>(
      Inspector::Metainfo::domainName);
}

void InspectorHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Inspector::Frontend>(dispatcher->channel());
  Inspector::Dispatcher::wire(dispatcher, this);
}

void InspectorHandler::SetRenderer(int process_host_id,
                                   RenderFrameHostImpl* frame_host) {
  host_ = frame_host;
}

void InspectorHandler::TargetCrashed() {
  target_crashed_ = true;
  frontend_->TargetCrashed();
}

void InspectorHandler::TargetReloadedAfterCrash() {
  // Only send the event if targetCrashed was previously sent in this session.
  if (!target_crashed_)
    return;
  frontend_->TargetReloadedAfterCrash();
}

void InspectorHandler::TargetDetached(const std::string& reason) {
  frontend_->Detached(reason);
}

Response InspectorHandler::Enable() {
  if (host_ && !host_->IsRenderFrameLive())
    TargetCrashed();
  return Response::Success();
}

Response InspectorHandler::Disable() {
  return Response::Success();
}

}  // namespace protocol
}  // namespace content
