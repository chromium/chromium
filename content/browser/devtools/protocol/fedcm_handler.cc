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
  if (!enabled_) {
    return;
  }

  auto* auth_request = GetFederatedAuthRequest();
  const auto* idp_data =
      auth_request ? &auth_request->GetSortedIdpData() : nullptr;
  DCHECK(idp_data);
  DCHECK(!idp_data->empty());

  auto accounts = std::make_unique<Array<FedCm::Account>>();
  for (const auto& data : *idp_data) {
    for (const IdentityRequestAccount& account : data.accounts) {
      std::unique_ptr<FedCm::Account> entry =
          FedCm::Account::Create()
              .SetAccountId(account.id)
              .SetEmail(account.email)
              .SetName(account.name)
              .SetGivenName(account.given_name)
              .SetPictureUrl(account.picture.spec())
              .SetIdpConfigUrl(data.idp_metadata.config_url.spec())
              .Build();
      accounts->push_back(std::move(entry));
    }
  }
  frontend_->DialogShown(std::move(accounts));
}

FederatedAuthRequestPageData* FedCmHandler::GetPageData() {
  if (!frame_host_) {
    return nullptr;
  }
  Page& page = frame_host_->GetPage();
  return PageUserData<FederatedAuthRequestPageData>::GetOrCreateForPage(page);
}

FederatedAuthRequestImpl* FedCmHandler::GetFederatedAuthRequest() {
  FederatedAuthRequestPageData* page_data = GetPageData();
  if (!page_data) {
    return nullptr;
  }
  return page_data->PendingWebIdentityRequest();
}

}  // namespace content::protocol
