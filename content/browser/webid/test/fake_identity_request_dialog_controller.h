// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_BROWSER_WEBID_TEST_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include <string>

#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

class WebContents;

// This fakes the request dialogs to always provide user consent.
// Tests that need to vary the responses or set test expectations should use
// MockIdentityRequestDialogController.
// This also fakes an IdP sign-in page until tests can be set up to
// verify the FederatedAuthResponse mechanics.
class FakeIdentityRequestDialogController
    : public IdentityRequestDialogController {
 public:
  explicit FakeIdentityRequestDialogController(
      absl::optional<std::string> dialog_selected_account);
  ~FakeIdentityRequestDialogController() override;

  FakeIdentityRequestDialogController(
      const FakeIdentityRequestDialogController&) = delete;
  FakeIdentityRequestDialogController& operator=(
      const FakeIdentityRequestDialogController&) = delete;

  void ShowAccountsDialog(WebContents* rp_web_contents,
                          const GURL& idp_signin_url,
                          base::span<const IdentityRequestAccount> accounts,
                          const IdentityProviderMetadata& idp_metadata,
                          const ClientIdData& client_id_data,
                          IdentityRequestAccount::SignInMode sign_in_mode,
                          AccountSelectionCallback on_selected) override;

 private:
  absl::optional<std::string> dialog_selected_account_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
