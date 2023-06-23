// Copyright 2022 The Chromium Authors
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

  // MakeCredential attempts to create a new WebAuthn credential on behalf of
  // `caller_origin` using the supplied `options` and invokes `callback` with
  // the result.
  virtual void MakeCredential(
      url::Origin caller_origin,
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback) = 0;

  // GetAssertion attempts to generate a WebAuthn assertion on behalf of
  // `caller_origin` using the supplied `options` and invokes `callback` with
  // the result.
  //
  // The optional `payment` is inserted into the asserted `clientDataJson` after
  // the browser displays the Secure Payment Confirmation dialog to the user.
  virtual void GetAssertion(
      url::Origin caller_origin,
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::PaymentOptionsPtr payment,
      blink::mojom::Authenticator::GetAssertionCallback callback) = 0;

  // Invokes `callback` with a boolean indicating whether a user-verifying
  // platform authenticator is available for WebAuthn requests on
  // `caller_origin`.
  virtual void IsUserVerifyingPlatformAuthenticatorAvailable(
      url::Origin caller_origin,
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) = 0;

  // Invokes `callback` with a boolean indicating whether the WebAuthn
  // "Conditional Mediation" feature is available for WebAuthn requests on
  // `caller_origin`.
  //
  // Conditional mediation lets relying parties make WebAuthn GetAssertion calls
  // using browser autofill.
  virtual void IsConditionalMediationAvailable(
      url::Origin caller_origin,
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

  // Disable the TLS security level check for the tab hosting this request.
  virtual void DisableTLSCheck() = 0;

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
