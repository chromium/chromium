// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CONTENT_BROWSER_INTERNAL_AUTHENTICATOR_IMPL_H_
#define COMPONENTS_WEBAUTHN_CONTENT_BROWSER_INTERNAL_AUTHENTICATOR_IMPL_H_

#include <stdint.h>

#include <memory>

#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

namespace url {
class Origin;
}

namespace content {

class AuthenticatorCommon;
class RenderFrameHost;

// Implementation of the public InternalAuthenticator interface.
// This class is meant only for trusted and internal components of Chrome to
// use.
class InternalAuthenticatorImpl : public webauthn::InternalAuthenticator,
                                  public WebContentsObserver {
 public:
  explicit InternalAuthenticatorImpl(RenderFrameHost* render_frame_host);
  InternalAuthenticatorImpl(const InternalAuthenticatorImpl&) = delete;
  InternalAuthenticatorImpl& operator=(const InternalAuthenticatorImpl&) =
      delete;
  ~InternalAuthenticatorImpl() override;

  // InternalAuthenticator:
  void SetEffectiveOrigin(const url::Origin& origin) override;
  void SetPaymentOptions(blink::mojom::PaymentOptionsPtr payment) override;
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback) override;
  void GetAssertion(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::Authenticator::GetAssertionCallback callback) override;
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback)
      override;
  bool IsGetMatchingCredentialIdsSupported() override;
  void GetMatchingCredentialIds(
      const std::string& relying_party_id,
      const std::vector<std::vector<uint8_t>>& credential_ids,
      bool require_third_party_payment_bit,
      webauthn::GetMatchingCredentialIdsCallback callback) override;
  void Cancel() override;
  content::RenderFrameHost* GetRenderFrameHost() override;

  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

 private:
  friend class InternalAuthenticatorImplTest;

  AuthenticatorCommon* get_authenticator_common_for_testing() {
    return authenticator_common_.get();
  }

  url::Origin effective_origin_;
  blink::mojom::PaymentOptionsPtr payment_;
  std::unique_ptr<AuthenticatorCommon> authenticator_common_;

  base::WeakPtrFactory<InternalAuthenticatorImpl> weak_factory_{this};
};

}  // namespace content

#endif  // COMPONENTS_WEBAUTHN_CONTENT_BROWSER_INTERNAL_AUTHENTICATOR_IMPL_H_
