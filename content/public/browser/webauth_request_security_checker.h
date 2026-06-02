// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBAUTH_REQUEST_SECURITY_CHECKER_H_
#define CONTENT_PUBLIC_BROWSER_WEBAUTH_REQUEST_SECURITY_CHECKER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"
#include "url/origin.h"

namespace webauthn {
class RemoteValidation;
}

namespace content {

// A centralized class for enforcing security policies that apply to
// Web Authentication requests to create credentials or get authentication
// assertions. For security reasons it is important that these checks are
// performed in the browser process, and this makes the verification code
// available to both the desktop and Android implementations of the
// |Authenticator| mojom interface.
class CONTENT_EXPORT WebAuthRequestSecurityChecker
    : public base::RefCounted<WebAuthRequestSecurityChecker> {
 public:
  enum class RequestType {
    // A standard WebAuthn request to create a new credential
    // (e.g., navigator.credentials.create()).
    kMakeCredential,

    // A request to create a credential that can be used cross-origin for
    // payments via Secure Payment Confirmation (SPC).
    kMakePaymentCredential,

    // A standard WebAuthn request to retrieve an authentication assertion
    // (e.g., navigator.credentials.get()).
    kGetAssertion,

    // A Secure Payment Confirmation (SPC) request to retrieve an assertion
    // (e.g., via the Payment Request API).
    kGetPaymentCredentialAssertion,

    // WebAuthn Signal API request
    kReport
  };

  // Runs the given callback with AuthenticatorStatus::SUCCESS if the origin
  // domain is valid under the referenced definitions, and also the requested
  // RP ID is a registrable domain suffix of, or is equal to, the origin's
  // effective domain. In this case the callback will be called before this
  // function returns.
  //
  // If `remote_desktop_client_override_origin` is present, this method
  // validates whether `caller_origin` is authorized to use that extension
  // through enterprise policy allowlists. This prevents untrusted renderer
  // processes from being able to impersonate arbitrary origins for WebAuthn
  // operations. The `remote_desktop_client_override_origin` comes from the
  // renderer and must not be trusted without this validation.
  //
  // If the RP ID cannot be validated using the rule above then a remote
  // validation will be attempted by fetching `.well-known/webauthn`
  // from the RP ID. In this case the return value will be non-null and the
  // caller needs to retain it. If the return value is deleted then the
  // operation will be canceled.
  //
  // References:
  //   https://url.spec.whatwg.org/#valid-domain-string
  //   https://html.spec.whatwg.org/multipage/origin.html#concept-origin-effective-domain
  //   https://html.spec.whatwg.org/multipage/origin.html#is-a-registrable-domain-suffix-of-or-is-equal-to
  virtual std::unique_ptr<webauthn::RemoteValidation>
  ValidateDomainAndRelyingPartyID(
      const url::Origin& caller_origin,
      const std::string& relying_party_id,
      RequestType request_type,
      const std::optional<url::Origin>& remote_desktop_client_override_origin,
      base::OnceCallback<void(blink::mojom::AuthenticatorStatus)> callback) = 0;

 protected:
  friend class base::RefCounted<WebAuthRequestSecurityChecker>;
  virtual ~WebAuthRequestSecurityChecker();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBAUTH_REQUEST_SECURITY_CHECKER_H_
