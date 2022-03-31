// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_identity_dialog_controller.h"

namespace content {

void ShellIdentityDialogController::ShowAccountsDialog(
    content::WebContents* rp_web_contents,
    const GURL& idp_signin_url,
    base::span<const IdentityRequestAccount> accounts,
    const IdentityProviderMetadata& idp_metadata,
    const ClientIdData& client_id_data,
    IdentityRequestAccount::SignInMode sign_in_mode,
    AccountSelectionCallback on_selected) {
  // Similar in spirit to allowlisted permissions in ShellPermissionManager,
  // we automatically select the first account here so that tests can pass.
  DCHECK_GT(accounts.size(), 0ul);
  std::move(on_selected).Run(accounts[0].id, /* is_sign_in= */ false);
}

}  // namespace content
