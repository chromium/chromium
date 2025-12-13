// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webid/identity_request_dialog_controller.h"

#include <memory>
#include <optional>

#include "content/public/browser/web_contents.h"

namespace content {

ClientMetadata::ClientMetadata(const GURL& terms_of_service_url,
                               const GURL& privacy_policy_url,
                               const GURL& brand_icon_url,
                               const gfx::Image& brand_decoded_icon)
    : terms_of_service_url{terms_of_service_url},
      privacy_policy_url(privacy_policy_url),
      brand_icon_url(brand_icon_url),
      brand_decoded_icon(brand_decoded_icon) {}
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
    std::optional<blink::mojom::Format> format,
    const std::vector<IdentityRequestDialogDisclosureField>& disclosure_fields,
    bool has_login_status_mismatch)
    : idp_for_display{idp_for_display},
      idp_metadata{idp_metadata},
      client_metadata{client_metadata},
      rp_context(rp_context),
      format(format),
      disclosure_fields(disclosure_fields),
      has_login_status_mismatch(has_login_status_mismatch) {}

IdentityProviderData::~IdentityProviderData() = default;

RelyingPartyData::RelyingPartyData(const std::u16string& rp_for_display,
                                   const std::u16string& iframe_for_display,
                                   bool display_strings_may_change)
    : rp_for_display(rp_for_display),
      iframe_for_display(iframe_for_display),
      display_strings_may_change(display_strings_may_change) {}
RelyingPartyData::RelyingPartyData(const RelyingPartyData& other) = default;
RelyingPartyData::~RelyingPartyData() = default;

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

void IdentityRequestDialogController::ShouldShowAccountsPassiveDialog(
    ShouldShowAccountsPassiveDialogCallback cb) {
  std::move(cb).Run(true);
}

bool IdentityRequestDialogController::ShowAccountsDialog(
    content::RelyingPartyData rp_data,
    const std::vector<scoped_refptr<content::IdentityProviderData>>& idp_list,
    const std::vector<scoped_refptr<content::IdentityRequestAccount>>& accounts,
    blink::mojom::RpMode rp_mode,
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
    const RelyingPartyData& rp_data,
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
    const RelyingPartyData& rp_data,
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
    const RelyingPartyData& rp_data,
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

bool IdentityRequestDialogController::ShowVerifyingDialog(
    const content::RelyingPartyData& rp_data,
    const scoped_refptr<IdentityProviderData>& idp_data,
    const scoped_refptr<content::IdentityRequestAccount>& account,
    content::IdentityRequestAccount::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode,
    AccountsDisplayedCallback accounts_displayed_callback) {
  if (!is_interception_enabled_) {
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

void IdentityRequestDialogController::NotifyAutofillSourceReadyForTesting() {}

bool IdentityRequestDialogController::DidShowUi() const {
  return false;
}

}  // namespace content
