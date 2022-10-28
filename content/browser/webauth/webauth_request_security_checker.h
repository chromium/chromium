// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_H_
#define CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"

namespace url {
class Origin;
}

namespace content {

class RenderFrameHost;

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
    kMakeCredential,
    kMakePaymentCredential,
    kGetAssertion,
    kGetPaymentCredentialAssertion
  };

  // Legacy App IDs, which google.com origins are allowed to assert for
  // compatibility reasons.
  static constexpr char kGstaticAppId[] =
      "https://www.gstatic.com/securitykey/origins.json";
  static constexpr char kGstaticCorpAppId[] =
      "https://www.gstatic.com/securitykey/a/google.com/origins.json";

  explicit WebAuthRequestSecurityChecker(RenderFrameHost* host);
  WebAuthRequestSecurityChecker(const WebAuthRequestSecurityChecker&) = delete;

  WebAuthRequestSecurityChecker& operator=(
      const WebAuthRequestSecurityChecker&) = delete;

  // Returns blink::mojom::AuthenticatorStatus::SUCCESS if |origin| is
  // same-origin with all ancestors in the frame tree, or else if
  // requests from cross-origin embeddings are allowed by policy and the
  // RequestType is |kGetAssertion| or |kMakePaymentCredential|.
  // Returns blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR otherwise.
  // |is_cross_origin| is an output parameter that is set to true if there is
  // a cross-origin embedding, regardless of policy, and false otherwise.
  blink::mojom::AuthenticatorStatus ValidateAncestorOrigins(
      const url::Origin& origin,
      RequestType type,
      bool* is_cross_origin);

  // Returns AuthenticatorStatus::SUCCESS if the origin domain is valid under
  // the referenced definitions, and also the requested RP ID is a registrable
  // domain suffix of, or is equal to, the origin's effective domain.
  //
  // If `remote_destop_client_override` is non-null, this method also validates
  // whether `caller_origin` is authorized to use that extension.
  //
  // References:
  //   https://url.spec.whatwg.org/#valid-domain-string
  //   https://html.spec.whatwg.org/multipage/origin.html#concept-origin-effective-domain
  //   https://html.spec.whatwg.org/multipage/origin.html#is-a-registrable-domain-suffix-of-or-is-equal-to
  blink::mojom::AuthenticatorStatus ValidateDomainAndRelyingPartyID(
      const url::Origin& caller_origin,
      const std::string& relying_party_id,
      RequestType request_type,
      const blink::mojom::RemoteDesktopClientOverridePtr&
          remote_desktop_client_override);

  // Validates whether `caller_origin` is authorized to claim the U2F AppID
  // `appid`, which per U2F's processing rules may be empty.
  // If `remote_destop_client_override` is non-null, this method also validates
  // whether `caller_origin` is authorized to use that extension.
  //
  // On success, this method returns `AuthenticatorStatus::SUCCESS` and sets
  // `out_app_id` to the AppID to use for the request. Otherwise, returns an
  // error which should be passed to the renderer.
  blink::mojom::AuthenticatorStatus ValidateAppIdExtension(
      std::string appid,
      url::Origin caller_origin,
      const blink::mojom::RemoteDesktopClientOverridePtr&
          remote_desktop_client_override,
      std::string* out_app_id);

  [[nodiscard]] bool DeduplicateCredentialDescriptorListAndValidateLength(
      std::vector<device::PublicKeyCredentialDescriptor>* list);

 protected:
  friend class base::RefCounted<WebAuthRequestSecurityChecker>;
  virtual ~WebAuthRequestSecurityChecker();

 private:
  // Returns whether the frame indicated by |host| is same-origin with its
  // entire ancestor chain. |origin| is the origin of the frame being checked.
  bool IsSameOriginWithAncestors(const url::Origin& origin);

  raw_ptr<RenderFrameHost> render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_H_
