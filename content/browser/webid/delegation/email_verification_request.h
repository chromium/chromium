// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFICATION_REQUEST_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFICATION_REQUEST_H_

#include "base/functional/callback.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/webid/delegation/dns_request.h"
#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/common/content_export.h"
#include "content/public/browser/webid/email_verifier.h"
#include "crypto/keypair.h"
#include "url/gurl.h"
#include "url/origin.h"

// This class implements the Email Verification Protocol as described here:
// https://github.com/dickhardt/email-verification-protocol

namespace content {
class RenderFrameHostImpl;
}

namespace content::webid {

// For a given email address, returns the domain. Returns std::nullopt if the
// email is not valid.
// e.g. "test@example.com" -> "example.com"
CONTENT_EXPORT std::optional<std::string> GetDomainFromEmail(
    const std::string& email);

// Performs the email verification process, which involves making a DNS TXT
// record request to determine the issuer, and then fetching a token from the
// issuer.
// This class is associated with a valid and alive RenderFrameHost which has
// to outlive it.
class CONTENT_EXPORT EmailVerificationRequest {
 public:
  explicit EmailVerificationRequest(RenderFrameHostImpl& render_frame_host);
  EmailVerificationRequest(
      std::unique_ptr<EmailVerifierNetworkRequestManager> network_manager,
      std::unique_ptr<DnsRequest> dns_request,
      base::SafeRef<RenderFrameHost> render_frame_host);
  virtual ~EmailVerificationRequest();

  EmailVerificationRequest(const EmailVerificationRequest&) = delete;
  EmailVerificationRequest& operator=(const EmailVerificationRequest&) = delete;

  // Starts the verification process for the given `email`.
  virtual void Send(const std::string& email,
                    const std::string& nonce,
                    EmailVerifier::OnEmailVerifiedCallback callback);

 private:
  sdjwt::Jwt CreateRequestToken(const std::string& email,
                                const sdjwt::Jwk& public_key);
  void OnDnsRequestComplete(
      const std::string& email,
      const std::string& nonce,
      EmailVerifier::OnEmailVerifiedCallback callback,
      const std::optional<std::vector<std::string>>& text_records);
  void OnWellKnownFetched(
      const std::string& email,
      const url::Origin& issuer,
      const std::string& nonce,
      EmailVerifier::OnEmailVerifiedCallback callback,
      FetchStatus status,
      EmailVerifierNetworkRequestManager::WellKnown well_known);
  void OnTokenRequestComplete(
      const std::string& nonce,
      std::unique_ptr<crypto::keypair::PrivateKey> private_key,
      EmailVerifier::OnEmailVerifiedCallback callback,
      FetchStatus token_status,
      EmailVerifierNetworkRequestManager::TokenResult&& result);

  std::unique_ptr<DnsRequest> dns_request_;
  std::unique_ptr<EmailVerifierNetworkRequestManager> network_manager_;
  base::SafeRef<RenderFrameHost> render_frame_host_;

  base::WeakPtrFactory<EmailVerificationRequest> weak_ptr_factory_{this};
};

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFICATION_REQUEST_H_
