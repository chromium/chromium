// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_fedcm_manager.h"

#include <optional>

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/request_page_data.h"
#include "content/browser/webid/request_service.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"

namespace content {

using DialogType = webid::RequestService::DialogType;

WebTestFedCmManager::WebTestFedCmManager(RenderFrameHost* render_frame_host)
    : render_frame_host_(
          static_cast<RenderFrameHostImpl*>(render_frame_host)->GetWeakPtr()) {}

WebTestFedCmManager::~WebTestFedCmManager() = default;

void WebTestFedCmManager::GetDialogType(
    blink::test::mojom::FederatedAuthRequestAutomation::GetDialogTypeCallback
        callback) {
  webid::RequestService* auth_request = GetAuthRequestService();
  if (!auth_request) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::string type_string;
  switch (auth_request->GetDialogType()) {
    case DialogType::kNone:
    // We do not expose these three types to browser automation currently.
    case DialogType::kLoginToIdpPopup:
    case DialogType::kContinueOnPopup:
    case DialogType::kErrorUrlPopup:
      std::move(callback).Run(std::nullopt);
      return;
    case DialogType::kSelectAccount:
      type_string = "AccountChooser";
      break;
    case DialogType::kAutoReauth:
      type_string = "AutoReauthn";
      break;
    case DialogType::kConfirmIdpLogin:
      type_string = "ConfirmIdpLogin";
      break;
    case DialogType::kError:
      type_string = "Error";
      break;
  }
  std::move(callback).Run(type_string);
}

void WebTestFedCmManager::GetFedCmDialogTitleAndSubtitle(
    blink::test::mojom::FederatedAuthRequestAutomation::
        GetFedCmDialogTitleAndSubtitleCallback callback) {
  webid::RequestService* auth_request = GetAuthRequestService();
  if (!auth_request) {
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }
  IdentityRequestDialogController* controller =
      auth_request->GetDialogController();
  if (!controller) {
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }
  std::move(callback).Run(controller->GetTitle(), controller->GetSubtitle());
}

void WebTestFedCmManager::SelectFedCmAccount(
    uint32_t account_index,
    SelectFedCmAccountCallback callback) {
  webid::RequestService* auth_request = GetAuthRequestService();
  if (!auth_request) {
    std::move(callback).Run(false);
    return;
  }
  const std::vector<IdentityRequestAccountPtr>& accounts =
      auth_request->GetAccounts();
  if (accounts.empty()) {
    std::move(callback).Run(false);
    return;
  }
  if (account_index >= accounts.size()) {
    std::move(callback).Run(false);
    return;
  }
  const IdentityRequestAccount& account = *accounts[account_index];
  auth_request->AcceptAccountsDialogForDevtools(
      account.identity_provider->idp_metadata.config_url, account);
  std::move(callback).Run(true);
}

void WebTestFedCmManager::DismissFedCmDialog(
    DismissFedCmDialogCallback callback) {
  webid::RequestService* auth_request = GetAuthRequestService();
  if (!auth_request) {
    std::move(callback).Run(false);
    return;
  }
  switch (auth_request->GetDialogType()) {
    case DialogType::kNone:
    // We do not expose these three types to browser automation currently.
    case DialogType::kLoginToIdpPopup:
    case DialogType::kContinueOnPopup:
    case DialogType::kErrorUrlPopup:
      std::move(callback).Run(false);
      return;
    case DialogType::kSelectAccount:
    case DialogType::kAutoReauth:
      auth_request->DismissAccountsDialogForDevtools(false);
      std::move(callback).Run(true);
      return;
    case DialogType::kConfirmIdpLogin:
      auth_request->DismissConfirmIdpLoginDialogForDevtools();
      std::move(callback).Run(true);
      return;
    case DialogType::kError:
      auth_request->DismissErrorDialogForDevtools();
      std::move(callback).Run(true);
      return;
  }
}

void WebTestFedCmManager::ClickFedCmDialogButton(
    blink::test::mojom::DialogButton button,
    ClickFedCmDialogButtonCallback callback) {
  webid::RequestService* auth_request = GetAuthRequestService();
  if (!auth_request) {
    std::move(callback).Run(false);
    return;
  }
  switch (button) {
    case blink::test::mojom::DialogButton::kConfirmIdpLoginContinue:
      switch (auth_request->GetDialogType()) {
        case DialogType::kConfirmIdpLogin:
          auth_request->AcceptConfirmIdpLoginDialogForDevtools();
          std::move(callback).Run(true);
          return;
        case DialogType::kSelectAccount: {
          const auto& data = auth_request->GetSortedIdpData();
          if (data.size() != 1) {
            std::move(callback).Run(false);
            return;
          }
          std::move(callback).Run(
              auth_request->UseAnotherAccountForDevtools(*data[0]));
          return;
        }
        default:
          std::move(callback).Run(false);
          return;
      }
    case blink::test::mojom::DialogButton::kErrorGotIt:
      if (auth_request->GetDialogType() != DialogType::kError) {
        std::move(callback).Run(false);
        return;
      }
      auth_request->ClickErrorDialogGotItForDevtools();
      std::move(callback).Run(true);
      return;
    case blink::test::mojom::DialogButton::kErrorMoreDetails:
      if (auth_request->GetDialogType() != DialogType::kError) {
        std::move(callback).Run(false);
        return;
      }
      auth_request->ClickErrorDialogMoreDetailsForDevtools();
      std::move(callback).Run(true);
      return;
  }
  std::move(callback).Run(false);
}

webid::RequestService* WebTestFedCmManager::GetAuthRequestService() {
  if (!render_frame_host_) {
    return nullptr;
  }
  webid::RequestPageData* page_data =
      PageUserData<webid::RequestPageData>::GetForPage(
          render_frame_host_->GetPage());
  if (!page_data) {
    return nullptr;
  }
  return page_data->PendingWebIdentityRequest();
}

}  // namespace content
