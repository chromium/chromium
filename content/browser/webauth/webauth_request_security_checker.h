// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_H_
#define CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"
#include "url/origin.h"

namespace base {
class Value;
}

namespace network {
class SimpleURLLoader;
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
    kGetPaymentCredentialAssertion,
    kReport
  };

  // A RemoteValidation represents a pending remote validation of an RP ID.
  class CONTENT_EXPORT RemoteValidation {
   public:
    ~RemoteValidation();

    // Create and start a remote validation. The `callback` argument may be
    // invoked before this function returns if the network request could not be
    // started. In that case, the return value will be `nullptr`. Otherwise the
    // caller should hold the result and wait for |callback| to be invoked. If
    // the return value is destroyed then the fetch will be canceled and
    // |callback| will never be invoked.
    static std::unique_ptr<RemoteValidation> Create(
        const url::Origin& caller_origin,
        const std::string& relying_party_id,
        base::OnceCallback<void(blink::mojom::AuthenticatorStatus)> callback);

    // ValidateWellKnownJSON implements the core of remote validation. It isn't
    // intended to be called externally except for testing.
    [[nodiscard]] static blink::mojom::AuthenticatorStatus
    ValidateWellKnownJSON(const url::Origin& caller_origin,
                          const base::Value& json);

   private:
    RemoteValidation(
        const url::Origin& caller_origin,
        base::OnceCallback<void(blink::mojom::AuthenticatorStatus)> callback);

    void OnFetchComplete(std::unique_ptr<std::string> body);
    void OnDecodeComplete(base::expected<base::Value, std::string> maybe_value);

    const url::Origin caller_origin_;
    base::OnceCallback<void(blink::mojom::AuthenticatorStatus)> callback_;
    std::unique_ptr<network::SimpleURLLoader> loader_;
    std::unique_ptr<std::string> json_;

    base::WeakPtrFactory<RemoteValidation> weak_factory_{this};
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

  // Runs the given callback with AuthenticatorStatus::SUCCESS if the origin
  // domain is valid under the referenced definitions, and also the requested
  // RP ID is a registrable domain suffix of, or is equal to, the origin's
  // effective domain. In this case the callback will be called before this
  // function returns.
  //
  // If `remote_destop_client_override` is non-null, this method also validates
  // whether `caller_origin` is authorized to use that extension.
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
  std::unique_ptr<RemoteValidation> ValidateDomainAndRelyingPartyID(
      const url::Origin& caller_origin,
      const std::string& relying_party_id,
      RequestType request_type,
      const blink::mojom::RemoteDesktopClientOverridePtr&
          remote_desktop_client_override,
      base::OnceCallback<void(blink::mojom::AuthenticatorStatus)> callback);

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
