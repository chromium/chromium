// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_fedcm_manager.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/federated_auth_request_page_data.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

WebTestFedCmManager::WebTestFedCmManager(RenderFrameHost* render_frame_host)
    : render_frame_host_(
          static_cast<RenderFrameHostImpl*>(render_frame_host)->GetWeakPtr()) {}

WebTestFedCmManager::~WebTestFedCmManager() = default;

void WebTestFedCmManager::GetDialogType(
    blink::test::mojom::FederatedAuthRequestAutomation::GetDialogTypeCallback
        callback) {
  FederatedAuthRequestImpl* auth_request = GetAuthRequestImpl();
  if (!auth_request) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::string type_string;
  switch (auth_request->GetDialogType()) {
    case FederatedAuthRequestImpl::kNone:
      std::move(callback).Run(absl::nullopt);
      return;
    case FederatedAuthRequestImpl::kSelectAccount:
      type_string = "AccountChooser";
      break;
    case FederatedAuthRequestImpl::kAutoReauth:
      type_string = "AutoReauthn";
      break;
    case FederatedAuthRequestImpl::kConfirmIdpLogin:
      type_string = "ConfirmIdpLogin";
      break;
    case FederatedAuthRequestImpl::kError:
      type_string = "Error";
      break;
  };
  std::move(callback).Run(type_string);
}

void WebTestFedCmManager::GetFedCmDialogTitle(
    blink::test::mojom::FederatedAuthRequestAutomation::
        GetFedCmDialogTitleCallback callback) {
  FederatedAuthRequestImpl* auth_request = GetAuthRequestImpl();
  if (!auth_request) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  IdentityRequestDialogController* controller =
      auth_request->GetDialogController();
  if (!controller) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(controller->GetTitle());
}

void WebTestFedCmManager::SelectFedCmAccount(
    uint32_t account_index,
    SelectFedCmAccountCallback callback) {
  FederatedAuthRequestImpl* auth_request = GetAuthRequestImpl();
  if (!auth_request) {
    std::move(callback).Run(false);
    return;
  }
  const std::vector<IdentityProviderData>& idp_data =
      auth_request->GetSortedIdpData();
  if (idp_data.empty()) {
    std::move(callback).Run(false);
    return;
  }
  uint32_t current = 0;
  for (const auto& data : idp_data) {
    for (const IdentityRequestAccount& account : data.accounts) {
      if (current == account_index) {
        auth_request->AcceptAccountsDialogForDevtools(
            data.idp_metadata.config_url, account);
        std::move(callback).Run(true);
        return;
      }
      ++current;
    }
  }
  std::move(callback).Run(false);
}

void WebTestFedCmManager::DismissFedCmDialog(
    DismissFedCmDialogCallback callback) {
  FederatedAuthRequestImpl* auth_request = GetAuthRequestImpl();
  if (!auth_request) {
    std::move(callback).Run(false);
    return;
  }
  switch (auth_request->GetDialogType()) {
    case FederatedAuthRequestImpl::kNone:
      std::move(callback).Run(false);
      return;
    case FederatedAuthRequestImpl::kSelectAccount:
    case FederatedAuthRequestImpl::kAutoReauth:
      auth_request->DismissAccountsDialogForDevtools(false);
      std::move(callback).Run(true);
      return;
    case FederatedAuthRequestImpl::kConfirmIdpLogin:
      auth_request->DismissConfirmIdpLoginDialogForDevtools();
      std::move(callback).Run(true);
      return;
    case FederatedAuthRequestImpl::kError:
      auth_request->DismissErrorDialogForDevtools();
      std::move(callback).Run(true);
      return;
  }
}

void WebTestFedCmManager::ClickFedCmDialogButton(
    blink::test::mojom::DialogButton button,
    ClickFedCmDialogButtonCallback callback) {
  FederatedAuthRequestImpl* auth_request = GetAuthRequestImpl();
  if (!auth_request) {
    std::move(callback).Run(false);
    return;
  }
  switch (button) {
    case blink::test::mojom::DialogButton::kConfirmIdpLoginContinue:
      if (auth_request->GetDialogType() !=
          FederatedAuthRequestImpl::kConfirmIdpLogin) {
        std::move(callback).Run(false);
        return;
      }
      auth_request->AcceptConfirmIdpLoginDialogForDevtools();
      std::move(callback).Run(true);
      return;
    case blink::test::mojom::DialogButton::kErrorGotIt:
      if (auth_request->GetDialogType() != FederatedAuthRequestImpl::kError) {
        std::move(callback).Run(false);
        return;
      }
      auth_request->ClickErrorDialogGotItForDevtools();
      std::move(callback).Run(true);
      return;
    case blink::test::mojom::DialogButton::kErrorMoreDetails:
      if (auth_request->GetDialogType() != FederatedAuthRequestImpl::kError) {
        std::move(callback).Run(false);
        return;
      }
      auth_request->ClickErrorDialogMoreDetailsForDevtools();
      std::move(callback).Run(true);
      return;
  }
  std::move(callback).Run(false);
}

FederatedAuthRequestImpl* WebTestFedCmManager::GetAuthRequestImpl() {
  if (!render_frame_host_) {
    return nullptr;
  }
  FederatedAuthRequestPageData* page_data =
      PageUserData<FederatedAuthRequestPageData>::GetForPage(
          render_frame_host_->GetPage());
  if (!page_data) {
    return nullptr;
  }
  return page_data->PendingWebIdentityRequest();
}

}  // namespace content
