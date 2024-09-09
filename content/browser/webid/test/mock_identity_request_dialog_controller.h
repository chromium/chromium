// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace content {

using IdentityProviderDataPtr = scoped_refptr<IdentityProviderData>;
using IdentityRequestAccountPtr = scoped_refptr<IdentityRequestAccount>;

class MockIdentityRequestDialogController
    : public IdentityRequestDialogController {
 public:
  MockIdentityRequestDialogController();

  ~MockIdentityRequestDialogController() override;

  MockIdentityRequestDialogController(
      const MockIdentityRequestDialogController&) = delete;
  MockIdentityRequestDialogController& operator=(
      const MockIdentityRequestDialogController&) = delete;

  MOCK_METHOD(bool,
              ShowAccountsDialog,
              (const std::string&,
               const std::vector<IdentityProviderDataPtr>&,
               const std::vector<IdentityRequestAccountPtr>&,
               IdentityRequestAccount::SignInMode,
               blink::mojom::RpMode,
               const std::vector<IdentityRequestAccountPtr>&,
               AccountSelectionCallback,
               LoginToIdPCallback,
               DismissCallback,
               AccountsDisplayedCallback),
              (override));
  MOCK_METHOD(void, DestructorCalled, ());
  MOCK_METHOD(bool,
              ShowFailureDialog,
              (const std::string&,
               const std::string&,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const content::IdentityProviderMetadata&,
               DismissCallback,
               LoginToIdPCallback),
              (override));
  MOCK_METHOD(bool,
              ShowErrorDialog,
              (const std::string&,
               const std::string&,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const content::IdentityProviderMetadata&,
               const std::optional<IdentityCredentialTokenError>&,
               DismissCallback,
               MoreDetailsCallback),
              (override));
  MOCK_METHOD(bool,
              ShowLoadingDialog,
              (const std::string&,
               const std::string&,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               DismissCallback),
              (override));
  MOCK_METHOD(WebContents*,
              ShowModalDialog,
              (const GURL&, blink::mojom::RpMode rp_mode, DismissCallback),
              (override));
  MOCK_METHOD(void, CloseModalDialog, (), (override));

  // Request the IdP Registration permission.
  MOCK_METHOD(void,
              RequestIdPRegistrationPermision,
              (const url::Origin&, base::OnceCallback<void(bool accepted)>),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
