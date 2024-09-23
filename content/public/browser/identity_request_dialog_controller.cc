// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/identity_request_dialog_controller.h"

#include <memory>
#include <optional>

#include "content/public/browser/web_contents.h"

namespace content {

ClientMetadata::ClientMetadata(const GURL& terms_of_service_url,
                               const GURL& privacy_policy_url,
                               const GURL& brand_icon_url)
    : terms_of_service_url{terms_of_service_url},
      privacy_policy_url(privacy_policy_url),
      brand_icon_url(brand_icon_url) {}
ClientMetadata::ClientMetadata(const ClientMetadata& other) = default;
ClientMetadata::~ClientMetadata() = default;

IdentityProviderMetadata::IdentityProviderMetadata() = default;
IdentityProviderMetadata::~IdentityProviderMetadata() = default;
IdentityProviderMetadata::IdentityProviderMetadata(
    const IdentityProviderMetadata& other) = default;

IdentityProviderData::IdentityProviderData(
    const std::string& idp_for_display,
    const IdentityProviderMetadata& idp_metadata,
    const ClientMetadata& client_metadata,
    blink::mojom::RpContext rp_context,
    const std::vector<IdentityRequestDialogDisclosureField>& disclosure_fields,
    bool has_login_status_mismatch)
    : idp_for_display{idp_for_display},
      idp_metadata{idp_metadata},
      client_metadata{client_metadata},
      rp_context(rp_context),
      disclosure_fields(disclosure_fields),
      has_login_status_mismatch(has_login_status_mismatch) {}

IdentityProviderData::~IdentityProviderData() = default;

int IdentityRequestDialogController::GetBrandIconIdealSize(
    blink::mojom::RpMode rp_mode) {
  return 0;
}

int IdentityRequestDialogController::GetBrandIconMinimumSize(
    blink::mojom::RpMode rp_mode) {
  return 0;
}

void IdentityRequestDialogController::SetIsInterceptionEnabled(bool enabled) {
  is_interception_enabled_ = enabled;
}

bool IdentityRequestDialogController::ShowAccountsDialog(
    const std::string& rp_for_display,
    const std::vector<scoped_refptr<content::IdentityProviderData>>& idp_list,
    const std::vector<scoped_refptr<content::IdentityRequestAccount>>& accounts,
    content::IdentityRequestAccount::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode,
    const std::vector<scoped_refptr<content::IdentityRequestAccount>>&
        new_accounts,
    AccountSelectionCallback on_selected,
    LoginToIdPCallback on_add_account,
    DismissCallback dismiss_callback,
    AccountsDisplayedCallback accounts_displayed_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
    return false;
  }
  return true;
}

bool IdentityRequestDialogController::ShowFailureDialog(
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const IdentityProviderMetadata& idp_metadata,
    DismissCallback dismiss_callback,
    LoginToIdPCallback login_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
    return false;
  }
  return true;
}

bool IdentityRequestDialogController::ShowErrorDialog(
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const IdentityProviderMetadata& idp_metadata,
    const std::optional<IdentityCredentialTokenError>& error,
    DismissCallback dismiss_callback,
    MoreDetailsCallback more_details_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
    return false;
  }
  return true;
}

bool IdentityRequestDialogController::ShowLoadingDialog(
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    DismissCallback dismiss_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
    return false;
  }
  return true;
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
    blink::mojom::RpMode rp_mode,
    DismissCallback dismiss_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
  }
  return nullptr;
}

void IdentityRequestDialogController::CloseModalDialog() {}

WebContents* IdentityRequestDialogController::GetRpWebContents() {
  return nullptr;
}

void IdentityRequestDialogController::RequestIdPRegistrationPermision(
    const url::Origin& origin,
    base::OnceCallback<void(bool accepted)> callback) {
  std::move(callback).Run(false);
}

}  // namespace content
