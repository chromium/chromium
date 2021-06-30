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
  FakeIdentityRequestDialogController(
      absl::optional<UserApproval> initial_permission_response,
      absl::optional<UserApproval> token_exchange_permission_response,
      std::string id_token);
  ~FakeIdentityRequestDialogController() override;

  FakeIdentityRequestDialogController(
      const FakeIdentityRequestDialogController&) = delete;
  FakeIdentityRequestDialogController& operator=(
      const FakeIdentityRequestDialogController&) = delete;

  void ShowInitialPermissionDialog(WebContents*,
                                   const GURL&,
                                   PermissionDialogMode mode,
                                   InitialApprovalCallback callback) override;
  void ShowIdProviderWindow(WebContents*,
                            WebContents* idp_web_contents,
                            const GURL&,
                            IdProviderWindowClosedCallback callback) override;
  void CloseIdProviderWindow() override;
  void ShowTokenExchangePermissionDialog(
      WebContents*,
      const GURL&,
      TokenExchangeApprovalCallback callback) override;

 private:
  // User action on the initial IdP tracking permission prompt. nullopt
  // prevents the callback from being invoked.
  absl::optional<UserApproval> initial_permission_response_;

  // User action on the token exchange permission prompt. nullopt
  // prevents the callback from being invoked.
  absl::optional<UserApproval> token_exchange_permission_response_;

  std::string id_token_;

  base::OnceClosure close_idp_window_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
