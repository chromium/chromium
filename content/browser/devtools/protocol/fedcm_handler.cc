// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/fedcm_handler.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/federated_auth_request_page_data.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/identity_request_dialog_controller.h"

namespace content {
namespace {
namespace FedCm = content::protocol::FedCm;

FedCm::DialogType ConvertDialogType(
    content::FederatedAuthRequestImpl::DialogType type) {
  switch (type) {
    case content::FederatedAuthRequestImpl::kNone:
      NOTREACHED() << "This should only be called if there is a dialog";
    case content::FederatedAuthRequestImpl::kLoginToIdpPopup:
    case content::FederatedAuthRequestImpl::kContinueOnPopup:
    case content::FederatedAuthRequestImpl::kErrorUrlPopup:
      NOTREACHED()
          << "These dialog types are not currently exposed to automation";
    case content::FederatedAuthRequestImpl::kSelectAccount:
      return FedCm::DialogTypeEnum::AccountChooser;
    case content::FederatedAuthRequestImpl::kAutoReauth:
      return FedCm::DialogTypeEnum::AutoReauthn;
    case content::FederatedAuthRequestImpl::kConfirmIdpLogin:
      return FedCm::DialogTypeEnum::ConfirmIdpLogin;
    case content::FederatedAuthRequestImpl::kError:
      return FedCm::DialogTypeEnum::Error;
  }
}

}  // namespace

namespace protocol {

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

DispatchResponse FedCmHandler::Enable(Maybe<bool> in_disableRejectionDelay) {
  auto* auth_request = GetFederatedAuthRequest();
  bool was_enabled = enabled_;
  enabled_ = true;
  disable_delay_ = in_disableRejectionDelay.value_or(false);

  // OnDialogShown should have been called previously if was_enabled is true.
  // This could happen if FedCmHandler::Enable was called to enable/disable the
  // rejection delay.
  if (!was_enabled && auth_request &&
      auth_request->GetDialogType() != FederatedAuthRequestImpl::kNone) {
    DidShowDialog();
  }

  return DispatchResponse::Success();
}

DispatchResponse FedCmHandler::Disable() {
  enabled_ = false;
  return DispatchResponse::Success();
}

void FedCmHandler::DidShowDialog() {
  DCHECK(frontend_);
  if (!enabled_) {
    return;
  }

  static int next_dialog_id_ = 0;
  dialog_id_ = base::NumberToString(next_dialog_id_++);

  auto* auth_request = GetFederatedAuthRequest();
  const auto* accounts = GetAccounts(auth_request);
  // `accounts` can be empty if this is an IDP Signin Confirmation dialog.
  auto accounts_array = std::make_unique<Array<FedCm::Account>>();
  if (accounts) {
    for (const auto& account : *accounts) {
      FedCm::LoginState login_state;
      std::optional<std::string> tos_url;
      std::optional<std::string> pp_url;
      switch (*account->login_state) {
        case IdentityRequestAccount::LoginState::kSignUp:
          login_state = FedCm::LoginStateEnum::SignUp;
          // Because TOS and PP URLs are only used when the login state is
          // sign up, we only populate them in that case.
          pp_url = account->identity_provider->client_metadata
                       .privacy_policy_url.spec();
          tos_url = account->identity_provider->client_metadata
                        .terms_of_service_url.spec();
          break;
        case IdentityRequestAccount::LoginState::kSignIn:
          login_state = FedCm::LoginStateEnum::SignIn;
          break;
      }
      std::unique_ptr<FedCm::Account> entry =
          FedCm::Account::Create()
              .SetAccountId(account->id)
              .SetEmail(account->email)
              .SetName(account->name)
              .SetGivenName(account->given_name)
              .SetPictureUrl(account->picture.spec())
              .SetIdpConfigUrl(
                  account->identity_provider->idp_metadata.config_url.spec())
              .SetIdpLoginUrl(
                  account->identity_provider->idp_metadata.idp_login_url.spec())
              .SetLoginState(login_state)
              .Build();
      if (pp_url) {
        entry->SetPrivacyPolicyUrl(*pp_url);
      }
      if (tos_url) {
        entry->SetTermsOfServiceUrl(*tos_url);
      }
      accounts_array->push_back(std::move(entry));
    }
  }
  IdentityRequestDialogController* dialog = auth_request->GetDialogController();
  CHECK(dialog);

  FedCm::DialogType dialog_type =
      ConvertDialogType(auth_request->GetDialogType());
  Maybe<String> maybe_subtitle;
  std::optional<std::string> subtitle = dialog->GetSubtitle();
  if (subtitle) {
    maybe_subtitle = *subtitle;
  }
  frontend_->DialogShown(dialog_id_, dialog_type, std::move(accounts_array),
                         dialog->GetTitle(), std::move(maybe_subtitle));
}

void FedCmHandler::DidCloseDialog() {
  CHECK(frontend_);
  if (!enabled_) {
    return;
  }
  frontend_->DialogClosed(dialog_id_);
}

DispatchResponse FedCmHandler::SelectAccount(const String& in_dialogId,
                                             int in_accountIndex) {
  if (in_dialogId != dialog_id_) {
    return DispatchResponse::InvalidParams(
        "Dialog ID does not match current dialog");
  }

  auto* auth_request = GetFederatedAuthRequest();
  if (!GetIdentityProviderData(auth_request)) {
    return DispatchResponse::ServerError(
        "selectAccount called while no FedCm dialog is shown");
  }
  const auto* accounts = GetAccounts(auth_request);
  if (!accounts || in_accountIndex < 0 ||
      static_cast<size_t>(in_accountIndex) >= accounts->size()) {
    return DispatchResponse::InvalidParams("Invalid account index");
  }

  const auto& account = accounts->at(in_accountIndex);
  auth_request->AcceptAccountsDialogForDevtools(
      account->identity_provider->idp_metadata.config_url, *account);
  return DispatchResponse::Success();
}

DispatchResponse FedCmHandler::OpenUrl(
    const String& in_dialogId,
    int in_accountIndex,
    const FedCm::AccountUrlType& in_accountUrlType) {
  if (in_dialogId != dialog_id_) {
    return DispatchResponse::InvalidParams(
        "Dialog ID does not match current dialog");
  }

  auto* auth_request = GetFederatedAuthRequest();
  if (!GetIdentityProviderData(auth_request)) {
    return DispatchResponse::ServerError(
        "openUrl called while no FedCm dialog is shown");
  }

  const auto* accounts = GetAccounts(auth_request);
  if (!accounts || in_accountIndex < 0 ||
      static_cast<size_t>(in_accountIndex) >= accounts->size()) {
    return DispatchResponse::InvalidParams("Invalid account index");
  }

  const auto& account = accounts->at(in_accountIndex);
  IdentityRequestDialogController::LinkType type;
  GURL url;
  if (in_accountUrlType == FedCm::AccountUrlTypeEnum::TermsOfService) {
    type = IdentityRequestDialogController::LinkType::TERMS_OF_SERVICE;
    url = account->identity_provider->client_metadata.terms_of_service_url;
  } else if (in_accountUrlType == FedCm::AccountUrlTypeEnum::PrivacyPolicy) {
    type = IdentityRequestDialogController::LinkType::PRIVACY_POLICY;
    url = account->identity_provider->client_metadata.privacy_policy_url;
  } else {
    return DispatchResponse::InvalidParams("Invalid account URL type");
  }
  if (!url.is_valid() ||
      account->login_state != IdentityRequestAccount::LoginState::kSignUp) {
    return DispatchResponse::InvalidParams(
        "Account does not have requested URL");
  }
  auth_request->GetDialogController()->ShowUrl(type, url);
  return DispatchResponse::Success();
}

DispatchResponse FedCmHandler::ClickDialogButton(
    const String& in_dialogId,
    const FedCm::DialogButton& in_dialogButton) {
  if (in_dialogId != dialog_id_) {
    return DispatchResponse::InvalidParams(
        "Dialog ID does not match current dialog");
  }

  auto* auth_request = GetFederatedAuthRequest();
  if (!auth_request) {
    return DispatchResponse::ServerError(
        "clickDialogButton called while no FedCm dialog is shown");
  }

  FederatedAuthRequestImpl::DialogType type = auth_request->GetDialogType();
  if (in_dialogButton == FedCm::DialogButtonEnum::ConfirmIdpLoginContinue) {
    switch (type) {
      case FederatedAuthRequestImpl::kConfirmIdpLogin:
        auth_request->AcceptConfirmIdpLoginDialogForDevtools();
        return DispatchResponse::Success();
      case FederatedAuthRequestImpl::kSelectAccount: {
        const auto* idp_data = GetIdentityProviderData(auth_request);
        CHECK(idp_data) << "kSelectAccount should always have IDP data";
        CHECK(!idp_data->empty());
        if (idp_data->size() > 1) {
          return DispatchResponse::ServerError(
              "Multi-IDP not supported for ConfirmIdpLogin yet "
              "(crbug.com/328115461)");
        }
        if (!auth_request->UseAnotherAccountForDevtools(*idp_data->at(0))) {
          return DispatchResponse::ServerError(
              "'Use another account' not supported for this IDP");
        }
        return DispatchResponse::Success();
      }
      default:
        return DispatchResponse::ServerError(
            "clickDialogButton called with ConfirmIdpLoginContinue while no "
            "confirm IDP login dialog is shown");
    }
  } else if (in_dialogButton == FedCm::DialogButtonEnum::ErrorGotIt) {
    if (type != FederatedAuthRequestImpl::kError) {
      return DispatchResponse::ServerError(
          "clickDialogButton called with ErrorGotIt while no error dialog is "
          "shown");
    }
    auth_request->ClickErrorDialogGotItForDevtools();
    return DispatchResponse::Success();
  } else if (in_dialogButton == FedCm::DialogButtonEnum::ErrorMoreDetails) {
    if (type != FederatedAuthRequestImpl::kError) {
      return DispatchResponse::ServerError(
          "clickDialogButton called with ErrorMoreDetails while no error "
          "dialog is shown");
    } else if (!auth_request->HasMoreDetailsButtonForDevtools()) {
      return DispatchResponse::ServerError(
          "clickDialogButton called with ErrorMoreDetails but more details "
          "button is not shown");
    }
    auth_request->ClickErrorDialogMoreDetailsForDevtools();
    return DispatchResponse::Success();
  }
  return DispatchResponse::InvalidParams("Invalid dialog button");
}

DispatchResponse FedCmHandler::DismissDialog(const String& in_dialogId,
                                             Maybe<bool> in_triggerCooldown) {
  if (in_dialogId != dialog_id_) {
    return DispatchResponse::InvalidParams(
        "Dialog ID does not match current dialog");
  }

  auto* auth_request = GetFederatedAuthRequest();
  if (!auth_request){
    return DispatchResponse::ServerError(
        "dismissDialog called while no FedCm dialog is shown");
  }

  FederatedAuthRequestImpl::DialogType type = auth_request->GetDialogType();
  if (type == FederatedAuthRequestImpl::kConfirmIdpLogin) {
    auth_request->DismissConfirmIdpLoginDialogForDevtools();
    return DispatchResponse::Success();
  }
  if (type == FederatedAuthRequestImpl::kError) {
    auth_request->DismissErrorDialogForDevtools();
    return DispatchResponse::Success();
  }
  const auto* idp_data = GetIdentityProviderData(auth_request);
  if (!idp_data) {
    return DispatchResponse::ServerError(
        "cancelDialog called while no FedCm dialog is shown");
  }

  auth_request->DismissAccountsDialogForDevtools(
      in_triggerCooldown.value_or(false));
  return DispatchResponse::Success();
}

DispatchResponse FedCmHandler::ResetCooldown() {
  auto* context = GetApiPermissionContext();
  if (!context) {
    return DispatchResponse::ServerError("no frame host");
  }
  context->RemoveEmbargoAndResetCounts(GetEmbeddingOrigin());
  return DispatchResponse::Success();
}

url::Origin FedCmHandler::GetEmbeddingOrigin() {
  CHECK(frame_host_);
  CHECK(frame_host_->GetMainFrame());
  return frame_host_->GetMainFrame()->GetLastCommittedOrigin();
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

const std::vector<IdentityProviderDataPtr>*
FedCmHandler::GetIdentityProviderData(FederatedAuthRequestImpl* auth_request) {
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

const std::vector<IdentityRequestAccountPtr>* FedCmHandler::GetAccounts(
    FederatedAuthRequestImpl* auth_request) {
  if (!auth_request) {
    return nullptr;
  }
  const auto& accounts = auth_request->GetAccounts();
  if (accounts.empty()) {
    return nullptr;
  }
  return &accounts;
}

FederatedIdentityApiPermissionContextDelegate*
FedCmHandler::GetApiPermissionContext() {
  if (!frame_host_) {
    return nullptr;
  }
  return frame_host_->GetBrowserContext()
      ->GetFederatedIdentityApiPermissionContext();
}

}  // namespace protocol
}  // namespace content
