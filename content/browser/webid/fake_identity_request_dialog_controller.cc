// Copyright 2022 The Chromium Authors. All rights reserved.
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
    const GURL& idp_signin_url,
    base::span<const IdentityRequestAccount> accounts,
    const IdentityProviderMetadata& idp_metadata,
    const ClientIdData& client_id_data,
    IdentityRequestAccount::SignInMode sign_in_mode,
    AccountSelectionCallback on_selected,
    DismissCallback dismiss_callback) {
  DCHECK_GT(accounts.size(), 0ul);
  // Use the provided account, if any. Otherwise use the first one.
  std::move(on_selected)
      .Run(selected_account_ ? *selected_account_ : accounts[0].id,
           /* is_sign_in= */ true);
}

}  // namespace content
