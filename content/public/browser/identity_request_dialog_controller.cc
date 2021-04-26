// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/identity_request_dialog_controller.h"

#include <memory>

#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace content {

// `Sec-` prefix makes this a forbidden header and cannot be added by
// JavaScript.
// This header tags browser-generated requests resulting from calls to the
// WebID API. Its presence can be used for, among other things, CSRF protection
// on the identity provider's server. This was originally "Sec-WebID" but was
// made more specific on speculation that we might need other headers later,
// though it is unclear what they would be for. It can change back later if
// no such requirements appear.
// See https://fetch.spec.whatwg.org/#forbidden-header-name
const char kSecWebIdCsrfHeader[] = "Sec-WebID-CSRF";

IdentityRequestAccount::IdentityRequestAccount(const std::string& sub,
                                               const std::string& email,
                                               const std::string& name,
                                               const std::string& given_name,
                                               const std::string& picture)
    : sub{sub},
      email{email},
      name{name},
      given_name{given_name},
      picture{picture} {}

IdentityRequestAccount::IdentityRequestAccount(const IdentityRequestAccount&) =
    default;
IdentityRequestAccount::~IdentityRequestAccount() = default;

void IdentityRequestDialogController::ShowInitialPermissionDialog(
    WebContents* rp_web_contents,
    const GURL& idp_url,
    InitialApprovalCallback approval_callback) {
  std::move(approval_callback).Run(UserApproval::kDenied);
}

void IdentityRequestDialogController::ShowIdProviderWindow(
    content::WebContents* rp_web_contents,
    content::WebContents* idp_web_contents,
    const GURL& idp_signin_url,
    IdProviderWindowClosedCallback on_closed) {
  std::move(on_closed).Run();
}

void IdentityRequestDialogController::CloseIdProviderWindow() {}

void IdentityRequestDialogController::ShowTokenExchangePermissionDialog(
    content::WebContents* rp_web_contents,
    const GURL& idp_url,
    TokenExchangeApprovalCallback approval_callback) {
  std::move(approval_callback).Run(UserApproval::kDenied);
}

}  // namespace content
