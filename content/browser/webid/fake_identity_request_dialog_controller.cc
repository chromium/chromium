// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fake_identity_request_dialog_controller.h"

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

FakeIdentityRequestDialogController::FakeIdentityRequestDialogController(
    absl::optional<std::string> selected_account)
    : selected_account_(selected_account) {}

FakeIdentityRequestDialogController::~FakeIdentityRequestDialogController() =
    default;

void FakeIdentityRequestDialogController::ShowAccountsDialog(
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::vector<content::IdentityProviderData>& identity_provider_data,
    IdentityRequestAccount::SignInMode sign_in_mode,
    bool show_auto_reauthn_checkbox,
    AccountSelectionCallback on_selected,
    DismissCallback dismiss_callback) {
  // TODO(crbug.com/1348262): Temporarily support only the first IDP, extend to
  // support multiple IDPs.
  std::vector<IdentityRequestAccount> accounts =
      identity_provider_data[0].accounts;
  CHECK_GT(accounts.size(), 0ul);
  CHECK_GT(identity_provider_data.size(), 0ul);

  // We're faking this so that browser automation and tests can verify that
  // the RP context was read properly.
  switch (identity_provider_data[0].rp_context) {
    case blink::mojom::RpContext::kSignIn:
      title_ = "Sign in";
      break;
    case blink::mojom::RpContext::kSignUp:
      title_ = "Sign up";
      break;
    case blink::mojom::RpContext::kUse:
      title_ = "Use";
      break;
    case blink::mojom::RpContext::kContinue:
      title_ = "Continue";
      break;
  };

  if (is_interception_enabled_) {
    // Browser automation will handle selecting an account/canceling.
    return;
  }
  // Use the provided account, if any. Otherwise do not run the callback right
  // away.
  if (selected_account_) {
    std::move(on_selected)
        .Run(identity_provider_data[0].idp_metadata.config_url,
             *selected_account_,
             /* is_sign_in= */ true);
  } else if (sign_in_mode == IdentityRequestAccount::SignInMode::kAuto) {
    std::move(on_selected)
        .Run(identity_provider_data[0].idp_metadata.config_url,
             identity_provider_data[0].accounts[0].id, /* is_sign_in= */ true);
  }
}

std::string FakeIdentityRequestDialogController::GetTitle() const {
  return title_;
}

content::WebContents* FakeIdentityRequestDialogController::ShowModalDialog(
    const GURL& url,
    DismissCallback dismiss_callback) {
  return nullptr;
}

}  // namespace content
