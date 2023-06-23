// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_MOCK_INTERNAL_AUTHENTICATOR_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_MOCK_INTERNAL_AUTHENTICATOR_H_

#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace webauthn {

class MockInternalAuthenticator : public webauthn::InternalAuthenticator {
 public:
  explicit MockInternalAuthenticator(content::WebContents* web_contents);
  ~MockInternalAuthenticator() override;

  MOCK_METHOD1(SetEffectiveOrigin, void(const url::Origin&));
  MOCK_METHOD1(SetPaymentOptions, void(blink::mojom::PaymentOptionsPtr));
  MOCK_METHOD2(
      MakeCredential,
      void(blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
           blink::mojom::Authenticator::MakeCredentialCallback callback));
  MOCK_METHOD2(
      GetAssertion,
      void(blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
           blink::mojom::Authenticator::GetAssertionCallback callback));
  MOCK_METHOD1(IsUserVerifyingPlatformAuthenticatorAvailable,
               void(blink::mojom::Authenticator::
                        IsUserVerifyingPlatformAuthenticatorAvailableCallback));
  MOCK_METHOD0(IsGetMatchingCredentialIdsSupported, bool());
  MOCK_METHOD4(GetMatchingCredentialIds,
               void(const std::string& relying_party_id,
                    const std::vector<std::vector<uint8_t>>& credential_ids,
                    bool require_third_party_payment_bit,
                    webauthn::GetMatchingCredentialIdsCallback callback));
  MOCK_METHOD0(Cancel, void());

  content::RenderFrameHost* GetRenderFrameHost() override {
    return web_contents_->GetPrimaryMainFrame();
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_MOCK_INTERNAL_AUTHENTICATOR_H_
