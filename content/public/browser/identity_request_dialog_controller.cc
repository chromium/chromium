// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/identity_request_dialog_controller.h"

#include <memory>

#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    const blink::mojom::RpContext& rp_context)
    : idp_for_display{idp_for_display},
      accounts{accounts},
      idp_metadata{idp_metadata},
      client_metadata{client_metadata},
      rp_context(rp_context) {}

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
    WebContents* rp_web_contents,
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_url_for_display,
    const std::vector<IdentityProviderData>& identity_provider_data,
    IdentityRequestAccount::SignInMode sign_in_mode,
    bool show_auto_reauthn_checkbox,
    AccountSelectionCallback on_selected,
    DismissCallback dismiss_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
  }
}

void IdentityRequestDialogController::ShowFailureDialog(
    WebContents* rp_web_contents,
    const std::string& top_frame_for_display,
    const std::string& idp_for_display,
    const IdentityProviderMetadata& idp_metadata,
    DismissCallback dismiss_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run(DismissReason::kOther);
  }
}

std::string IdentityRequestDialogController::GetTitle() const {
  return std::string();
}

absl::optional<std::string> IdentityRequestDialogController::GetSubtitle()
    const {
  return absl::nullopt;
}

void IdentityRequestDialogController::ShowIdpSigninFailureDialog(
    base::OnceClosure dismiss_callback) {
  if (!is_interception_enabled_) {
    std::move(dismiss_callback).Run();
  }
}

}  // namespace content
