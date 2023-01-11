// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_INTERNAL_AUTHENTICATOR_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_INTERNAL_AUTHENTICATOR_H_

#include "base/functional/callback.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace webauthn {

// Callback for GetMatchingCredentialIds. This method is not exposed to the
// renderer directly, and so its callback is not defined in mojom.
using GetMatchingCredentialIdsCallback =
    base::OnceCallback<void(std::vector<std::vector<uint8_t>>)>;

// Interface similar to blink::mojom::Authenticator meant only for internal
// components in Chrome to use in order to direct authenticators to create or
// use a public key credential. Unlike Authenticator, the caller will be
// allowed to set its own effective origin.
class InternalAuthenticator {
 public:
  virtual ~InternalAuthenticator() = default;

  // Sets the effective origin of the caller. Since this may be a browser
  // process, the Relying Party ID may be different from the renderer's origin.
  virtual void SetEffectiveOrigin(const url::Origin& origin) = 0;

  // Sets the payment information to be added to the "clientDataJson". Should be
  // used only if the user has confirmed the payment information that was
  // displayed to the user.
  virtual void SetPaymentOptions(blink::mojom::PaymentOptionsPtr payment) = 0;

  // Gets the credential info for a new public key credential created by an
  // authenticator for the given |options|. Invokes |callback| with credentials
  // if authentication was successful.
  virtual void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback) = 0;

  // Uses an existing credential to produce an assertion for the given
  // |options|. Invokes |callback| with assertion response if authentication
  // was successful.
  virtual void GetAssertion(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::Authenticator::GetAssertionCallback callback) = 0;

  // Returns true if the user platform provides an authenticator. Relying
  // Parties use this method to determine whether they can create a new
  // credential using a user-verifying platform authenticator.
  virtual void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) = 0;

  // Returns true if an authenticator supports the GetMatchingCredentialIds API.
  virtual bool IsGetMatchingCredentialIdsSupported() = 0;

  // Queries the authenticator for known credentials for the given
  // |relying_party_id| which are also in the input |credential_ids| list.
  // Optionally, can restrict to only match third-party payment enabled
  // credentials.
  virtual void GetMatchingCredentialIds(
      const std::string& relying_party_id,
      const std::vector<std::vector<uint8_t>>& credential_ids,
      bool require_third_party_payment_bit,
      GetMatchingCredentialIdsCallback callback) = 0;

  // Cancel an ongoing MakeCredential or GetAssertion request.
  // Only one MakeCredential or GetAssertion call at a time is allowed,
  // any future calls are cancelled.
  virtual void Cancel() = 0;

  // Returns the non-owned RenderFrameHost associated with this authenticator.
  // Can be used for observing the host's deletion.
  virtual content::RenderFrameHost* GetRenderFrameHost() = 0;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_INTERNAL_AUTHENTICATOR_H_
