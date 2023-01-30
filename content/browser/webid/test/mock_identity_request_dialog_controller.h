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

  MOCK_METHOD7(ShowAccountsDialog,
               void(WebContents*,
                    const std::string&,
                    const std::vector<content::IdentityProviderData>&,
                    IdentityRequestAccount::SignInMode,
                    bool,
                    AccountSelectionCallback,
                    DismissCallback));
  MOCK_METHOD0(DestructorCalled, void());
  MOCK_METHOD4(ShowFailureDialog,
               void(WebContents*,
                    const std::string&,
                    const std::string&,
                    DismissCallback));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
