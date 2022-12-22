// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_COMMON_H_
#define CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_COMMON_H_

#include <memory>

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class RenderFrameHost;

// Interface for any WebAuthn Authenticator common code.
class CONTENT_EXPORT AuthenticatorCommon {
 public:
  static std::unique_ptr<AuthenticatorCommon> Create(
      RenderFrameHost* render_frame_host);

  virtual ~AuthenticatorCommon() = default;

  // This is not-quite an implementation of blink::mojom::Authenticator.
  // Gets the credential info for a new public key credential created by an
  // authenticator for the given |options|. It takes the |caller_origin|
  // explicitly be overridden if needed. Invokes |callback| with credentials if
  // authentication was successful.
  virtual void MakeCredential(
      url::Origin caller_origin,
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback) = 0;

  // This is not-quite an implementation of blink::mojom::Authenticator.
  // Uses an existing credential to produce an assertion for the given
  // |options|. It takes the |caller_origin| explicitly be overridden if needed.
  // It takes the optional |payment| to add to "clientDataJson" after the
  // browser displays the payment confirmation dialog to the user. Invokes
  // |callback| with assertion response if authentication was successful.
  virtual void GetAssertion(
      url::Origin caller_origin,
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::PaymentOptionsPtr payment,
      blink::mojom::Authenticator::GetAssertionCallback callback) = 0;

  // Returns true if the user platform provides an authenticator. Relying
  // Parties use this method to determine whether they can create a new
  // credential using a user-verifying platform authenticator.
  virtual void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) = 0;

  // Returns true if Conditional Mediation is available. Relying Parties can use
  // this method to determine whether they can pass "conditional" to the
  // "mediation" parameter of a webauthn get call and have the browser autofill
  // webauthn credentials on form inputs.
  virtual void IsConditionalMediationAvailable(
      blink::mojom::Authenticator::IsConditionalMediationAvailableCallback
          callback) = 0;

  // Cancel an ongoing MakeCredential or GetAssertion request.
  // Only one MakeCredential or GetAssertion call at a time is allowed,
  // any future calls are cancelled.
  virtual void Cancel() = 0;

  // Cleanup after the request completion
  virtual void Cleanup() = 0;

  // Disable UI
  virtual void DisableUI() = 0;

  // GetRenderFrameHost returns a pointer to the RenderFrameHost that was given
  // to the constructor. Use this rather than keeping a copy of the
  // RenderFrameHost* that was passed in.
  //
  // This object assumes that the RenderFrameHost overlives it but, in case it
  // doesn't, this avoids holding a raw pointer and creating a use-after-free.
  // If the RenderFrameHost has been destroyed then this function will return
  // nullptr and the process will crash when it tries to use it.
  virtual RenderFrameHost* GetRenderFrameHost() const = 0;

  // Enables support for the webAuthenticationRequestProxy extensions API.  If
  // called, remote desktop Chrome extensions may choose to act as a request
  // proxy for all requests sent to this instance.
  virtual void EnableRequestProxyExtensionsAPISupport() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_COMMON_H_
