// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/identity_request_dialog_controller.h"

#include <memory>

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

void IdentityRequestDialogController::ShowAccountsDialog(
    WebContents* rp_web_contents,
    const std::string& rp_for_display,
    const std::vector<IdentityProviderData>& identity_provider_data,
    IdentityRequestAccount::SignInMode sign_in_mode,
    AccountSelectionCallback on_selected,
    DismissCallback dismiss_callback) {
  std::move(dismiss_callback).Run(DismissReason::OTHER);
}

void IdentityRequestDialogController::ShowFailureDialog(
    WebContents* rp_web_contents,
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    DismissCallback dismiss_callback) {
  std::move(dismiss_callback).Run(DismissReason::OTHER);
}

}  // namespace content
