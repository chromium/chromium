// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/fedcm_handler.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/federated_auth_request_page_data.h"
#include "content/public/browser/identity_request_dialog_controller.h"

namespace content::protocol {

FedCmHandler::FedCmHandler()
    : DevToolsDomainHandler(FedCm::Metainfo::domainName) {}

FedCmHandler::~FedCmHandler() = default;

// static
std::vector<FedCmHandler*> FedCmHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<FedCmHandler>(FedCm::Metainfo::domainName);
}

void FedCmHandler::SetRenderer(int process_host_id,
                               RenderFrameHostImpl* frame_host) {
  frame_host_ = frame_host;
}

void FedCmHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<FedCm::Frontend>(dispatcher->channel());
  FedCm::Dispatcher::wire(dispatcher, this);
}

DispatchResponse FedCmHandler::Enable() {
  enabled_ = true;
  return DispatchResponse::Success();
}

DispatchResponse FedCmHandler::Disable() {
  enabled_ = false;
  return DispatchResponse::Success();
}

void FedCmHandler::OnDialogShown() {
  DCHECK(frontend_);
  if (enabled_) {
    frontend_->DialogShown();
  }
}

}  // namespace content::protocol
