// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/fedcm_handler.h"

#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/federated_auth_request_page_data.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
namespace FedCm = content::protocol::FedCm;

FedCm::DialogType ConvertDialogType(
    content::FederatedAuthRequestImpl::DialogType type) {
  switch (type) {
    case content::FederatedAuthRequestImpl::kNone:
      NOTREACHED_NORETURN()
          << "This should only be called if there is a dialog";
    case content::FederatedAuthRequestImpl::kSelectAccount:
      return FedCm::DialogTypeEnum::AccountChooser;
    case content::FederatedAuthRequestImpl::kAutoReauth:
      return FedCm::DialogTypeEnum::AutoReauthn;
    case content::FederatedAuthRequestImpl::kConfirmIdpLogin:
      return FedCm::DialogTypeEnum::ConfirmIdpLogin;
  }
}
}  // namespace

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
    OnDialogShown();
  }

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

  static int next_dialog_id_ = 0;
  dialog_id_ = base::NumberToString(next_dialog_id_++);

  auto* auth_request = GetFederatedAuthRequest();
  const auto* idp_data = GetIdentityProviderData(auth_request);
  // idp_data can be empty if this is an IDP Signin Confirmation dialog.

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
  if (idp_data) {
    for (const auto& data : *idp_data) {
      for (const IdentityRequestAccount& account : data.accounts) {
        FedCm::LoginState login_state;
        absl::optional<std::string> tos_url;
        absl::optional<std::string> pp_url;
        switch (*account.login_state) {
          case IdentityRequestAccount::LoginState::kSignUp:
            login_state = FedCm::LoginStateEnum::SignUp;
            // Because TOS and PP URLs are only used when the login state is
            // sign up, we only populate them in that case.
            pp_url = data.client_metadata.privacy_policy_url.spec();
            tos_url = data.client_metadata.terms_of_service_url.spec();
            break;
          case IdentityRequestAccount::LoginState::kSignIn:
            login_state = FedCm::LoginStateEnum::SignIn;
            break;
        }
        std::unique_ptr<FedCm::Account> entry =
            FedCm::Account::Create()
                .SetAccountId(account.id)
                .SetEmail(account.email)
                .SetName(account.name)
                .SetGivenName(account.given_name)
                .SetPictureUrl(account.picture.spec())
                .SetIdpConfigUrl(data.idp_metadata.config_url.spec())
                .SetIdpLoginUrl(data.idp_metadata.idp_login_url.spec())
                .SetLoginState(login_state)
                .Build();
        if (pp_url) {
          entry->SetPrivacyPolicyUrl(*pp_url);
        }
        if (tos_url) {
          entry->SetTermsOfServiceUrl(*tos_url);
        }
        accounts->push_back(std::move(entry));
      }
    }
  }
  IdentityRequestDialogController* dialog = auth_request->GetDialogController();
  CHECK(dialog);

  FedCm::DialogType dialog_type =
      ConvertDialogType(auth_request->GetDialogType());
  Maybe<String> maybe_subtitle;
  absl::optional<std::string> subtitle = dialog->GetSubtitle();
  if (subtitle) {
    maybe_subtitle = *subtitle;
  }
  frontend_->DialogShown(dialog_id_, dialog_type, std::move(accounts),
                         dialog->GetTitle(), std::move(maybe_subtitle));
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

DispatchResponse FedCmHandler::ConfirmIdpLogin(const String& in_dialogId) {
  if (in_dialogId != dialog_id_) {
    return DispatchResponse::InvalidParams(
        "Dialog ID does not match current dialog");
  }

  auto* auth_request = GetFederatedAuthRequest();
  if (!auth_request) {
    return DispatchResponse::ServerError(
        "dismissDialog called while no FedCm dialog is shown");
  }

  FederatedAuthRequestImpl::DialogType type = auth_request->GetDialogType();
  if (type != FederatedAuthRequestImpl::kConfirmIdpLogin) {
    return DispatchResponse::ServerError(
        "dismissDialog called while no confirm IDP login dialog is shown");
  }
  auth_request->AcceptConfirmIdpLoginDialogForDevtools();
  return DispatchResponse::Success();
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

FederatedIdentityApiPermissionContextDelegate*
FedCmHandler::GetApiPermissionContext() {
  if (!frame_host_) {
    return nullptr;
  }
  return frame_host_->GetBrowserContext()
      ->GetFederatedIdentityApiPermissionContext();
}

}  // namespace content::protocol
