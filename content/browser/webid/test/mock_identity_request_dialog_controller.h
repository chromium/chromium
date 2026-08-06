// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
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

  MOCK_METHOD(bool,
              ShowAccountsDialog,
              (RelyingPartyData,
               const std::vector<scoped_refptr<IdentityProviderData>>&,
               const std::vector<scoped_refptr<IdentityRequestAccount>>&,
               const std::vector<scoped_refptr<IdentityRequestAccount>>&,
               blink::mojom::RpMode,
               AccountSelectionCallback,
               LoginToIdPCallback,
               DismissCallback,
               AccountsDisplayedCallback),
              (override));
  MOCK_METHOD(void, DestructorCalled, ());
  MOCK_METHOD(bool,
              ShowFailureDialog,
              (const RelyingPartyData&,
               const std::string&,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const IdentityProviderMetadata&,
               const std::vector<scoped_refptr<IdentityRequestAccount>>&,
               DismissCallback,
               LoginToIdPCallback),
              (override));
  MOCK_METHOD(bool,
              ShowErrorDialog,
              (const RelyingPartyData&,
               const std::string&,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const IdentityProviderMetadata&,
               const std::optional<IdentityCredentialTokenError>&,
               DismissCallback,
               MoreDetailsCallback),
              (override));
  MOCK_METHOD(bool,
              ShowLoadingDialog,
              (const RelyingPartyData&,
               const std::string&,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               DismissCallback),
              (override));
  MOCK_METHOD(bool,
              ShowVerifyingDialog,
              (const RelyingPartyData&,
               const scoped_refptr<IdentityProviderData>&,
               const scoped_refptr<IdentityRequestAccount>&,
               IdentityRequestAccount::SignInMode,
               blink::mojom::RpMode,
               AccountsDisplayedCallback),
              (override));
  MOCK_METHOD(WebContents*,
              ShowModalDialog,
              (const GURL&,
               blink::mojom::RpMode rp_mode,
               DismissCallback,
               ShownModalAsyncCallback,
               TokenCallback),
              (override));
  MOCK_METHOD(void, CloseModalDialog, (), (override));
  MOCK_METHOD(void, NotifyAutofillSourceReadyForTesting, (), (override));

  // Request the IdP Registration permission.
  MOCK_METHOD(void,
              RequestIdPRegistrationPermision,
              (const url::Origin&, base::OnceCallback<void(bool accepted)>),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
