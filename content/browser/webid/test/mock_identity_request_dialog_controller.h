// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include "content/public/browser/identity_request_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

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

  MOCK_METHOD10(ShowAccountsDialog,
                bool(const std::string&,
                     const std::optional<std::string>&,
                     const std::vector<content::IdentityProviderData>&,
                     IdentityRequestAccount::SignInMode,
                     blink::mojom::RpMode rp_mode,
                     const std::optional<content::IdentityProviderData>&,
                     AccountSelectionCallback,
                     LoginToIdPCallback,
                     DismissCallback,
                     AccountsDisplayedCallback));
  MOCK_METHOD0(DestructorCalled, void());
  MOCK_METHOD8(ShowFailureDialog,
               bool(const std::string&,
                    const std::optional<std::string>&,
                    const std::string&,
                    blink::mojom::RpContext rp_context,
                    blink::mojom::RpMode rp_mode,
                    const content::IdentityProviderMetadata&,
                    DismissCallback,
                    LoginToIdPCallback));
  MOCK_METHOD9(ShowErrorDialog,
               bool(const std::string&,
                    const std::optional<std::string>&,
                    const std::string&,
                    blink::mojom::RpContext rp_context,
                    blink::mojom::RpMode rp_mode,
                    const content::IdentityProviderMetadata&,
                    const std::optional<IdentityCredentialTokenError>&,
                    DismissCallback,
                    MoreDetailsCallback));
  MOCK_METHOD5(ShowLoadingDialog,
               bool(const std::string&,
                    const std::string&,
                    blink::mojom::RpContext rp_context,
                    blink::mojom::RpMode rp_mode,
                    DismissCallback));
  MOCK_METHOD2(ShowModalDialog, WebContents*(const GURL&, DismissCallback));
  MOCK_METHOD0(CloseModalDialog, void());

  // Request the IdP Registration permission.
  MOCK_METHOD2(RequestIdPRegistrationPermision,
               void(const url::Origin&,
                    base::OnceCallback<void(bool accepted)>));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
