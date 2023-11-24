// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include "content/public/browser/identity_request_dialog_controller.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockIdentityRequestDialogController
    : public IdentityRequestDialogController {
 public:
  MockIdentityRequestDialogController();

  ~MockIdentityRequestDialogController() override;

  MockIdentityRequestDialogController(
      const MockIdentityRequestDialogController&) = delete;
  MockIdentityRequestDialogController& operator=(
      const MockIdentityRequestDialogController&) = delete;

  MOCK_METHOD8(ShowAccountsDialog,
               void(const std::string&,
                    const absl::optional<std::string>&,
                    const std::vector<content::IdentityProviderData>&,
                    IdentityRequestAccount::SignInMode,
                    bool,
                    AccountSelectionCallback,
                    LoginToIdPCallback,
                    DismissCallback));
  MOCK_METHOD0(DestructorCalled, void());
  MOCK_METHOD7(ShowFailureDialog,
               void(const std::string&,
                    const absl::optional<std::string>&,
                    const std::string&,
                    const blink::mojom::RpContext& rp_context,
                    const content::IdentityProviderMetadata&,
                    DismissCallback,
                    LoginToIdPCallback));
  MOCK_METHOD8(ShowErrorDialog,
               void(const std::string&,
                    const absl::optional<std::string>&,
                    const std::string&,
                    const blink::mojom::RpContext& rp_context,
                    const content::IdentityProviderMetadata&,
                    const absl::optional<IdentityCredentialTokenError>&,
                    DismissCallback,
                    MoreDetailsCallback));
  MOCK_METHOD2(ShowModalDialog, WebContents*(const GURL&, DismissCallback));
  MOCK_METHOD0(CloseModalDialog, void());
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
