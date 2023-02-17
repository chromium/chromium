// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fake_identity_request_dialog_controller.h"

namespace content {

FakeIdentityRequestDialogController::FakeIdentityRequestDialogController(
    absl::optional<std::string> selected_account)
    : selected_account_(selected_account) {}

FakeIdentityRequestDialogController::~FakeIdentityRequestDialogController() =
    default;

void FakeIdentityRequestDialogController::ShowAccountsDialog(
    content::WebContents* rp_web_contents,
    const std::string& rp_for_display,
    const std::vector<content::IdentityProviderData>& identity_provider_data,
    IdentityRequestAccount::SignInMode sign_in_mode,
    bool show_auto_reauthn_checkbox,
    AccountSelectionCallback on_selected,
    DismissCallback dismiss_callback) {
  // TODO(crbug.com/1348262): Temporarily support only the first IDP, extend to
  // support multiple IDPs.
  std::vector<IdentityRequestAccount> accounts =
      identity_provider_data[0].accounts;
  DCHECK_GT(accounts.size(), 0ul);
  // Use the provided account, if any. Otherwise use the first one.
  std::move(on_selected)
      .Run(identity_provider_data[0].idp_metadata.config_url,
           selected_account_ ? *selected_account_ : accounts[0].id,
           /* is_sign_in= */ true);
}

}  // namespace content
