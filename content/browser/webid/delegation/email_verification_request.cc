// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verification_request.h"

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/jwt_signer.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/keypair.h"
#include "url/origin.h"

namespace content::webid {

std::optional<std::string> GetDomainFromEmail(const std::string& email) {
  auto parts = base::RSplitStringOnce(email, "@");

  if (!parts) {
    return std::nullopt;
  }

  if (parts->first.empty() || parts->second.empty()) {
    return std::nullopt;
  }

  // Use GURL to validate that the domain is a valid host.
  // TODO(crbug.com/380367784): consider better ways to validate if
  // the email domain is well formed.
  GURL url("https://" + std::string(parts->second));
  if (!url.is_valid() || !url.has_host() || url.GetHost() != parts->second) {
    return std::nullopt;
  }

  return std::string(parts->second);
}

EmailVerificationRequest::EmailVerificationRequest(
    RenderFrameHostImpl& render_frame_host)
    : EmailVerificationRequest(
          EmailVerifierNetworkRequestManager::Create(&render_frame_host),
          std::make_unique<DnsRequest>(base::BindRepeating(
              [](RenderFrameHost* rfh) -> network::mojom::NetworkContext* {
                return rfh->GetStoragePartition()->GetNetworkContext();
              },
              &render_frame_host)),
          render_frame_host.GetSafeRef()) {}

EmailVerificationRequest::EmailVerificationRequest(
    std::unique_ptr<EmailVerifierNetworkRequestManager> network_manager,
    std::unique_ptr<DnsRequest> dns_request,
    base::SafeRef<RenderFrameHost> render_frame_host)
    : dns_request_(std::move(dns_request)),
      network_manager_(std::move(network_manager)),
      render_frame_host_(render_frame_host) {}

EmailVerificationRequest::~EmailVerificationRequest() = default;

sdjwt::Jwt EmailVerificationRequest::CreateRequestToken(
    const std::string& email,
    const sdjwt::Jwk& public_key) {
  sdjwt::Header header;
  header.alg = "RS256";
  header.typ = "JWT";
  header.jwk = public_key;
  CHECK(header.jwk);

  base::Time now = base::Time::Now();
  // TODO(crbug.com/380367784): figure out what's the right
  // expiration time for the request token.
  base::TimeDelta ttl = base::Minutes(5);
  base::Time expiration = now + ttl;

  sdjwt::Payload payload;
  payload.email = email;
  // TODO(crbug.com/380367784): figure out why/whether the
  // nonce is needed here. Use a hardcoded value for now.
  payload.nonce = "--a-fake-nonce--";
  // TODO(crbug.com/380367784): check if `render_frame_host_` isn't an
  // opaque origin, or any other validation that might be
  // necessary.
  payload.aud = render_frame_host_->GetLastCommittedOrigin().Serialize();
  payload.exp = expiration;
  payload.iat = now;

  sdjwt::Jwt jwt;
  jwt.header = *header.ToJson();
  jwt.payload = *payload.ToJson();

  return jwt;
}

// The email verification process starts once the user
// goes through Step 1 and 2 described here:
//
// https://github.com/dickhardt/email-verification-protocol?tab=readme-ov-file#3-token-request
void EmailVerificationRequest::Send(
    const std::string& email,
    const std::string& nonce,
    EmailVerifier::OnEmailVerifiedCallback callback) {
  // Step 3: Token Request

  // Step 3.1: the browser extracts the domain from the email address and
  // asks the DNS server who the issuer is:

  std::optional<std::string> domain = GetDomainFromEmail(email);
  if (!domain) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::string hostname = "_email-verification." + *domain;

  dns_request_->SendRequest(
      hostname, base::BindOnce(&EmailVerificationRequest::OnDnsRequestComplete,
                               weak_ptr_factory_.GetWeakPtr(), email, nonce,
                               std::move(callback)));
}

void EmailVerificationRequest::OnDnsRequestComplete(
    const std::string& email,
    const std::string& nonce,
    EmailVerifier::OnEmailVerifiedCallback callback,
    const std::optional<std::vector<std::string>>& text_records) {
  // Step 3.2: when the DNS response is received, the browser
  // parses the TXT record to extract the issuer's origin.
  if (!text_records || text_records->size() != 1) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const std::string& record = (*text_records)[0];
  static constexpr char kIssPrefix[] = "iss=";
  if (!base::StartsWith(record, kIssPrefix, base::CompareCase::SENSITIVE)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::string iss = record.substr(sizeof(kIssPrefix) - 1);
  if (iss.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // If the issuer is found, the browser fetches the issuer's
  // .well-known/web-identity file:

  GURL issuer("https://" + iss);
  network_manager_->FetchWellKnown(
      issuer,
      base::BindOnce(&EmailVerificationRequest::OnWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr(), email,
                     url::Origin::Create(issuer), nonce, std::move(callback)));
}

void EmailVerificationRequest::OnWellKnownFetched(
    const std::string& email,
    const url::Origin& issuer,
    const std::string& nonce,
    EmailVerifier::OnEmailVerifiedCallback callback,
    FetchStatus status,
    EmailVerifierNetworkRequestManager::WellKnown well_known) {
  // Step 3.3: when the .well-known/web-identity file is fetched,
  // the browser checks that the issuance_endpoint is present.

  if (status.parse_status != ParseStatus::kSuccess) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (well_known.issuance_endpoint.is_empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Step 3.4: if the issuance_endpoint is present, the browser
  // creates a signed request token.

  // TODO(crbug.com/380367784): understand and document why RSA was
  // preferred over ECDSA here.
  auto private_key = std::make_unique<crypto::keypair::PrivateKey>(
      crypto::keypair::PrivateKey::GenerateRsa2048());
  CHECK(private_key);

  std::optional<sdjwt::Jwk> public_key = sdjwt::ExportPublicKey(*private_key);
  CHECK(public_key);

  sdjwt::Jwt jwt = CreateRequestToken(email, *public_key);

  auto signer = sdjwt::CreateJwtSigner(*private_key);
  CHECK(jwt.Sign(std::move(signer)));

  auto request_token = jwt.Serialize();
  CHECK(!request_token->empty());

  // Step 3.5: finally, the browser sends a POST request to the
  // issuance_endpoint with the request_token as a form parameter.

  network_manager_->SendTokenRequest(
      well_known.issuance_endpoint, "request_token=" + request_token.value(),
      // TODO(crbug.com/380367784): figure out how to measure the feature
      // here.
      base::BindOnce(&EmailVerificationRequest::OnTokenRequestComplete,
                     weak_ptr_factory_.GetWeakPtr(), nonce,
                     std::move(private_key), std::move(callback)));
}

void EmailVerificationRequest::OnTokenRequestComplete(
    const std::string& nonce,
    std::unique_ptr<crypto::keypair::PrivateKey> private_key,
    EmailVerifier::OnEmailVerifiedCallback callback,
    FetchStatus token_status,
    EmailVerifierNetworkRequestManager::TokenResult&& result) {
  // Step 5: Token Presentation

  if (token_status.parse_status != ParseStatus::kSuccess || !result.token ||
      !result.token->is_string()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto token = sdjwt::SdJwt::Parse(result.token->GetString());

  // Step 5.1: The browser parses and verifies if the SD-JWT
  // is valid.
  // TODO: check if all of the necessary fields of the SD-JWT
  // are present and valid.

  if (!token) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto sd_jwt = sdjwt::SdJwt::From(*token);

  if (!sd_jwt) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Step 5.2: bind the nonce and the website to the
  // key binding JWT.

  sdjwt::SdJwtKb sd_jwt_kb;
  sd_jwt_kb.sd_jwt = *sd_jwt;

  sdjwt::Header header;
  header.alg = "RS256";
  header.typ = "kb+jwt";

  sdjwt::Payload payload;
  payload.aud = render_frame_host_->GetLastCommittedOrigin().Serialize();
  payload.nonce = nonce;

  sdjwt::Jwt kb_jwt;
  kb_jwt.header = *header.ToJson();
  kb_jwt.payload = *payload.ToJson();

  auto signer = sdjwt::CreateJwtSigner(*private_key);

  if (!kb_jwt.Sign(std::move(signer))) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  sd_jwt_kb.kb_jwt = kb_jwt;

  // Step 5.3: the browser notifies the page that
  // the SD-JWT+KB is ready.

  std::move(callback).Run(sd_jwt_kb.Serialize());
}

}  // namespace content::webid
