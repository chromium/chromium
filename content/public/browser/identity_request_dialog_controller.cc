// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/identity_request_dialog_controller.h"

#include <memory>
#include <optional>

#include "content/public/browser/web_contents.h"

namespace content {

ClientMetadata::ClientMetadata(const GURL& terms_of_service_url,
                               const GURL& privacy_policy_url)
    : terms_of_service_url{terms_of_service_url},
      privacy_policy_url(privacy_policy_url) {}
ClientMetadata::ClientMetadata(const ClientMetadata& other) = default;
ClientMetadata::~ClientMetadata() = default;

IdentityProviderMetadata::IdentityProviderMetadata() = default;
IdentityProviderMetadata::~IdentityProviderMetadata() = default;
IdentityProviderMetadata::IdentityProviderMetadata(
    const IdentityProviderMetadata& other) = default;

IdentityProviderData::IdentityProviderData(
    const std::string& idp_for_display,
    const std::vector<IdentityRequestAccount>& accounts,
    const IdentityProviderMetadata& idp_metadata,
    const ClientMetadata& client_metadata,
    blink::mojom::RpContext rp_context,
    bool request_permission,
    bool has_login_status_mismatch)
    : idp_for_display{idp_for_display},
      accounts{accounts},
      idp_metadata{idp_metadata},
      client_metadata{client_metadata},
      rp_context(rp_context),
      request_permission(request_permission),
      has_login_status_mismatch(has_login_status_mismatch) {}

IdentityProviderData::IdentityProviderData(const IdentityProviderData& other) =
    default;
IdentityProviderData::~IdentityProviderData() = default;

int IdentityRequestDialogController::GetBrandIconIdealSize() {
  return 0;
}

int IdentityRequestDialogController::GetBrandIconMinimumSize() {
  return 0;
}

void IdentityRequestDialogController::SetIsInterceptionEnabled(bool enabled) {
  is_interception_enabled_ = enabled;
}

void IdentityRequestDialogController::ShowAccountsDialog(
    const std::string& top_frame_for_display,
    const std::optional<std::string>& iframe_for_display,
    const std::vector<IdentityProviderData>& identity_provider_data,
    IdentityRequestAccount::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode,
    const std::optional<content::IdentityProviderData>& new_account_idp,
    AccountSelectionCallback on_selected,
    LoginToIdPCallback on_add_account,
    DismissCallback dismiss_callback,
    AccountsDisplayedCallback accounts_displayed_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
  }
}

void IdentityRequestDialogController::ShowFailureDialog(
    const std::string& top_frame_for_display,
    const std::optional<std::string>& iframe_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const IdentityProviderMetadata& idp_metadata,
    DismissCallback dismiss_callback,
    LoginToIdPCallback login_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
  }
}

void IdentityRequestDialogController::ShowErrorDialog(
    const std::string& top_frame_for_display,
    const std::optional<std::string>& iframe_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const IdentityProviderMetadata& idp_metadata,
    const std::optional<IdentityCredentialTokenError>& error,
    DismissCallback dismiss_callback,
    MoreDetailsCallback more_details_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
  }
}

void IdentityRequestDialogController::ShowLoadingDialog(
    const std::string& top_frame_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    DismissCallback dismiss_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
  }
}

std::string IdentityRequestDialogController::GetTitle() const {
  return std::string();
}

std::optional<std::string> IdentityRequestDialogController::GetSubtitle()
    const {
  return std::nullopt;
}

void IdentityRequestDialogController::ShowUrl(LinkType type, const GURL& url) {}

WebContents* IdentityRequestDialogController::ShowModalDialog(
    const GURL& url,
    DismissCallback dismiss_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
  }
  return nullptr;
}

void IdentityRequestDialogController::CloseModalDialog() {}

void IdentityRequestDialogController::RequestIdPRegistrationPermision(
    const url::Origin& origin,
    base::OnceCallback<void(bool accepted)> callback) {
  std::move(callback).Run(false);
}

}  // namespace content
