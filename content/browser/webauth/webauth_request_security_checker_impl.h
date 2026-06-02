// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/webauthn/core/browser/remote_validation.h"
#include "content/common/content_export.h"
#include "content/public/browser/webauth_request_security_checker.h"
#include "device/fido/public/public_key_credential_descriptor.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"
#include "url/origin.h"

class GURL;

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT WebAuthRequestSecurityCheckerImpl
    : public WebAuthRequestSecurityChecker {
 public:
  // Legacy App IDs, which google.com origins are allowed to assert for
  // compatibility reasons.
  static constexpr char kGstaticAppId[] =
      "https://www.gstatic.com/securitykey/origins.json";
  static constexpr char kGstaticCorpAppId[] =
      "https://www.gstatic.com/securitykey/a/google.com/origins.json";

  explicit WebAuthRequestSecurityCheckerImpl(RenderFrameHost* host);
  WebAuthRequestSecurityCheckerImpl(const WebAuthRequestSecurityCheckerImpl&) =
      delete;
  WebAuthRequestSecurityCheckerImpl& operator=(
      const WebAuthRequestSecurityCheckerImpl&) = delete;

  // WebAuthRequestSecurityChecker:
  std::unique_ptr<webauthn::RemoteValidation> ValidateDomainAndRelyingPartyID(
      const url::Origin& caller_origin,
      const std::string& relying_party_id,
      RequestType request_type,
      const std::optional<url::Origin>& remote_desktop_client_override_origin,
      base::OnceCallback<void(blink::mojom::AuthenticatorStatus)> callback)
      override;

  blink::mojom::AuthenticatorStatus ValidateAncestorOrigins(
      const url::Origin& origin,
      RequestType type,
      bool* is_cross_origin);
  blink::mojom::AuthenticatorStatus ValidateAppIdExtension(
      std::string appid,
      url::Origin caller_origin,
      const blink::mojom::RemoteDesktopClientOverridePtr&
          remote_desktop_client_override,
      std::string* out_app_id);

  [[nodiscard]] bool DeduplicateCredentialDescriptorListAndValidateLength(
      std::vector<device::PublicKeyCredentialDescriptor>* list);

  // Validates the cross-device fallback URL. Returns true if it is valid,
  // secure, matches the RP ID, and is allowed by CSP. Otherwise returns false.
  bool ValidateCrossDeviceFallbackUrl(const std::string& relying_party_id,
                                      const GURL& fallback_url);

  static bool& UseSystemSharedURLLoaderFactoryForTesting();

 private:
  ~WebAuthRequestSecurityCheckerImpl() override;

  // Returns whether the frame indicated by |host| is same-origin with its
  // entire ancestor chain. |origin| is the origin of the frame being checked.
  bool IsSameOriginWithAncestors(const url::Origin& origin);

  raw_ptr<RenderFrameHost> render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_IMPL_H_
