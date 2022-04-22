// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/identity_request_dialog_controller.h"

#include <memory>

#include "content/public/browser/web_contents.h"

namespace content {

// `Sec-` prefix makes this a forbidden header and cannot be added by
// JavaScript.
// This header tags browser-generated requests resulting from calls to the
// FedCM API. Its presence can be used for, among other things, CSRF protection
// on the identity provider's server. This originally omitted "-CSRF" but was
// made more specific on speculation that we might need other headers later,
// though it is unclear what they would be for. It can change back later if
// no such requirements appear.
// See https://fetch.spec.whatwg.org/#forbidden-header-name
const char kSecFedCmCsrfHeader[] = "Sec-FedCM-CSRF";
const char kSecFedCmCsrfHeaderValue[] = "?1";

IdentityRequestAccount::IdentityRequestAccount(
    const std::string& id,
    const std::string& email,
    const std::string& name,
    const std::string& given_name,
    const GURL& picture,
    absl::optional<LoginState> login_state)
    : id{id},
      email{email},
      name{name},
      given_name{given_name},
      picture{picture},
      login_state{login_state} {}

IdentityRequestAccount::IdentityRequestAccount(const IdentityRequestAccount&) =
    default;
IdentityRequestAccount::~IdentityRequestAccount() = default;

ClientIdData::ClientIdData(const GURL& terms_of_service_url,
                           const GURL& privacy_policy_url)
    : terms_of_service_url{terms_of_service_url},
      privacy_policy_url(privacy_policy_url) {}

IdentityProviderMetadata::IdentityProviderMetadata() = default;
IdentityProviderMetadata::~IdentityProviderMetadata() = default;
IdentityProviderMetadata::IdentityProviderMetadata(
    const IdentityProviderMetadata& other) = default;

int IdentityRequestDialogController::GetBrandIconIdealSize() {
  return 0;
}

int IdentityRequestDialogController::GetBrandIconMinimumSize() {
  return 0;
}

void IdentityRequestDialogController::ShowAccountsDialog(
    content::WebContents* rp_web_contents,
    const GURL& idp_signin_url,
    base::span<const IdentityRequestAccount> accounts,
    const IdentityProviderMetadata& idp_metadata,
    const ClientIdData& client_id_data,
    IdentityRequestAccount::SignInMode sign_in_mode,
    AccountSelectionCallback on_selected) {
  std::move(on_selected)
      .Run(/*account_id=*/"", /*is_sign_in=*/false, /*should_embargo=*/false);
}

}  // namespace content
