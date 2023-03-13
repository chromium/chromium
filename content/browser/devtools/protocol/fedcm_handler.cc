// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/fedcm_handler.h"

#include "base/strings/string_number_conversions.h"
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
  static int next_dialog_id_ = 0;

  dialog_id_ = base::NumberToString(next_dialog_id_++);

  DCHECK(frontend_);
  if (!enabled_) {
    return;
  }

  auto* auth_request = GetFederatedAuthRequest();
  const auto* idp_data = GetIdentityProviderData(auth_request);
  DCHECK(idp_data);
  DCHECK(!idp_data->empty());

  // We flatten the two-level IDP->account list into a single
  // list of accounts because:
  // - It's easier to work with for callers
  // - When used for automated testing by IDPs, I expect most of their
  //   tests to involve just one IDP (themselves), and the single level
  //   list is easier to use in that case
  // - If we decide to show, for example, returning accounts across IDPs
  //   at the top and remaining accounts below that, the two-level list
  //   requires the same IDP to be present more than once; a single-level
  //   list is less confusing.
  // The idpConfigUrl field allows callers to identify which IDP an account
  // belongs to.
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
  frontend_->DialogShown(dialog_id_, std::move(accounts));
}

DispatchResponse FedCmHandler::SelectAccount(const String& in_dialogId,
                                             int in_accountIndex) {
  if (in_dialogId != dialog_id_) {
    return DispatchResponse::InvalidParams(
        "Dialog ID does not match current dialog");
  }

  auto* auth_request = GetFederatedAuthRequest();
  const auto* idp_data = GetIdentityProviderData(auth_request);
  if (!idp_data) {
    return DispatchResponse::ServerError(
        "selectAccount called while no FedCm dialog is shown");
  }
  int current = 0;
  for (const auto& data : *idp_data) {
    for (const IdentityRequestAccount& account : data.accounts) {
      if (current == in_accountIndex) {
        auth_request->AcceptAccountsDialogForDevtools(
            data.idp_metadata.config_url, account);
        return DispatchResponse::Success();
      }
      ++current;
    }
  }
  return DispatchResponse::InvalidParams("Invalid account index");
}

DispatchResponse FedCmHandler::DismissDialog(const String& in_dialogId) {
  if (in_dialogId != dialog_id_) {
    return DispatchResponse::InvalidParams(
        "Dialog ID does not match current dialog");
  }

  auto* auth_request = GetFederatedAuthRequest();
  const auto* idp_data = GetIdentityProviderData(auth_request);
  if (!idp_data) {
    return DispatchResponse::ServerError(
        "cancelDialog called while no FedCm dialog is shown");
  }

  auth_request->DismissAccountsDialogForDevtools();
  return DispatchResponse::Success();
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

const std::vector<IdentityProviderData>* FedCmHandler::GetIdentityProviderData(
    FederatedAuthRequestImpl* auth_request) {
  if (!auth_request) {
    return nullptr;
  }
  const auto& idp_data = auth_request->GetSortedIdpData();
  // idp_data is empty iff no dialog is shown.
  if (idp_data.empty()) {
    return nullptr;
  }
  return &idp_data;
}

}  // namespace content::protocol
