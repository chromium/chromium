// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/fake_identity_request_dialog_controller.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

FakeIdentityRequestDialogController::FakeIdentityRequestDialogController(
    absl::optional<std::string> dialog_selected_account)
    : dialog_selected_account_(dialog_selected_account) {}

FakeIdentityRequestDialogController::~FakeIdentityRequestDialogController() =
    default;

void FakeIdentityRequestDialogController::ShowAccountsDialog(
    WebContents* rp_web_contents,
    const GURL& idp_signin_url,
    base::span<const IdentityRequestAccount> accounts,
    const IdentityProviderMetadata& idp_metadata,
    const ClientIdData& client_id_data,
    IdentityRequestAccount::SignInMode sign_in_mode,
    AccountSelectionCallback on_selected) {
  if (dialog_selected_account_) {
    std::move(on_selected)
        .Run(*dialog_selected_account_, true /* is_sign_in */);
  }
}

}  // namespace content
