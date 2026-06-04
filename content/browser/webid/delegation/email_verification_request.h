// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFICATION_REQUEST_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFICATION_REQUEST_H_

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/expected.h"
#include "content/browser/webid/delegation/dns_request.h"
#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"
#include "content/browser/webid/delegation/evt_verifier.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "crypto/keypair.h"
#include "third_party/blink/public/mojom/webid/email_verification_request.mojom-shared.h"
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

using WellKnownOrError = base::RefCountedData<
    base::expected<EmailVerifierNetworkRequestManager::WellKnown,
                   blink::mojom::EmailVerificationRequestResult>>;
using AccountsOrError = base::RefCountedData<
    base::expected<std::vector<IdentityRequestAccountPtr>,
                   blink::mojom::EmailVerificationRequestResult>>;
using TokenResultOrError = base::RefCountedData<
    base::expected<EmailVerifierNetworkRequestManager::TokenResult,
                   blink::mojom::EmailVerificationRequestResult>>;
using JwksResultOrError = base::RefCountedData<
    base::expected<base::Value, blink::mojom::EmailVerificationRequestResult>>;
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
      std::unique_ptr<IdpNetworkRequestManager> idp_network_manager,
      std::unique_ptr<DnsRequest> dns_request,
      RenderFrameHostImpl& render_frame_host);
  virtual ~EmailVerificationRequest();

  EmailVerificationRequest(const EmailVerificationRequest&) = delete;
  EmailVerificationRequest& operator=(const EmailVerificationRequest&) = delete;

  // Checks if the given `email` is verifiable. This also checks if the user is
  // logged in to the issuer.
  virtual void CheckIfVerifiable(const std::string& email,
                                 EmailVerifier::IsVerifiableCallback callback);

  // Issues the verification token.
  virtual void Verify(const EmailVerifier::Result& result,
                      const std::string& nonce,
                      EmailVerifier::OnEmailVerifiedCallback callback);

 private:
  sdjwt::Jwt CreateRequestToken(const std::string& email,
                                const sdjwt::Jwk& public_key,
                                const url::Origin& issuer);
  void OnDnsRequestComplete(
      const std::string& email,
      EmailVerifier::IsVerifiableCallback callback,
      const std::optional<std::vector<std::string>>& text_records);

  void OnEmailVerificationWellKnownFetched(
      base::RepeatingClosure barrier,
      const url::Origin& issuer,
      scoped_refptr<WellKnownOrError> well_known,
      FetchStatus status,
      EmailVerifierNetworkRequestManager::WellKnown fetched_well_known);
  void OnWebIdentityWellKnownFetched(
      const url::Origin& issuer,
      const std::string& email,
      base::RepeatingClosure barrier,
      scoped_refptr<AccountsOrError> accounts,
      FetchStatus status,
      const IdpNetworkRequestManager::WellKnown& well_known);
  void OnAccountsResponseReceived(
      const std::string& email,
      base::RepeatingClosure barrier,
      scoped_refptr<AccountsOrError> accounts,
      FetchStatus status,
      IdpNetworkRequestManager::AccountsResponse response);
  void OnAccountStatusFetched(scoped_refptr<WellKnownOrError> well_known,
                              scoped_refptr<AccountsOrError> accounts,
                              const url::Origin& issuer_origin,
                              const std::string& email,
                              EmailVerifier::IsVerifiableCallback callback);
  void OnTokenAndKeysFetchComplete(
      scoped_refptr<TokenResultOrError> token,
      scoped_refptr<JwksResultOrError> jwks,
      const url::Origin& issuer,
      const std::string& nonce,
      std::unique_ptr<crypto::keypair::PrivateKey> private_key,
      const std::string& email,
      EmailVerifier::OnEmailVerifiedCallback callback);

  void CompleteIsVerifiableRequest(
      EmailVerifier::IsVerifiableCallback callback,
      std::optional<EmailVerifier::Result> response,
      blink::mojom::EmailVerificationRequestResult status);

  void CompleteVerifyRequest(
      EmailVerifier::OnEmailVerifiedCallback callback,
      std::optional<std::string> response,
      blink::mojom::EmailVerificationRequestResult status);

  void AddDevToolsIssue(blink::mojom::EmailVerificationRequestResult status);

  std::unique_ptr<DnsRequest> dns_request_;
  std::unique_ptr<EmailVerifierNetworkRequestManager> network_manager_;
  std::unique_ptr<IdpNetworkRequestManager> idp_network_manager_;
  base::WeakPtr<RenderFrameHostImpl> render_frame_host_;

  base::WeakPtrFactory<EmailVerificationRequest> weak_ptr_factory_{this};
};

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFICATION_REQUEST_H_
