// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verification_request.h"

#include <optional>

#include "base/barrier_closure.h"
#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"
#include "content/browser/webid/delegation/evt_verifier.h"
#include "content/browser/webid/delegation/jwt_signer.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/browser/webid/mappers.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/webid/federated_identity_permission_context_delegate.h"
#include "crypto/keypair.h"
#include "crypto/sha2.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "url/origin.h"

using blink::mojom::EmailVerificationRequestResult;

namespace content::webid {

using blink::mojom::EmailVerificationRequestResult;

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
          IdpNetworkRequestManager::Create(&render_frame_host),
          std::make_unique<DnsRequest>(base::BindRepeating(
              [](EmailVerificationRequest* request)
                  -> EmailVerifierNetworkRequestManager* {
                return request->network_manager_.get();
              },
              this)),
          render_frame_host) {}

EmailVerificationRequest::EmailVerificationRequest(
    std::unique_ptr<EmailVerifierNetworkRequestManager> network_manager,
    std::unique_ptr<IdpNetworkRequestManager> idp_network_manager,
    std::unique_ptr<DnsRequest> dns_request,
    RenderFrameHostImpl& render_frame_host)
    : dns_request_(std::move(dns_request)),
      network_manager_(std::move(network_manager)),
      idp_network_manager_(std::move(idp_network_manager)),
      render_frame_host_(render_frame_host.GetWeakPtr()) {}

EmailVerificationRequest::~EmailVerificationRequest() {
  observers_.Notify(&Observer::OnRequestDestroyed, this);
}

void EmailVerificationRequest::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EmailVerificationRequest::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

sdjwt::Jwt EmailVerificationRequest::CreateRequestToken(
    const std::string& email,
    const sdjwt::Jwk& public_key,
    const url::Origin& issuer) {
  sdjwt::Header header;
  header.alg = public_key.alg;
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
  payload.aud = issuer.Serialize();
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
// https://github.com/dickhardt/email-verification-protocol#3-token-request

void EmailVerificationRequest::CheckIfVerifiable(
    const std::string& email,
    EmailVerifier::IsVerifiableCallback callback) {
  observers_.Notify(&Observer::OnIsVerifiableStart, this);
  if (!render_frame_host_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (render_frame_host_->GetLastCommittedOrigin().opaque() ||
      render_frame_host_->IsNestedWithinFencedFrame() ||
      !IsSameOriginWithAncestors(render_frame_host_->GetLastCommittedOrigin(),
                                 render_frame_host_.get())) {
    CompleteIsVerifiableRequest(
        std::move(callback), std::nullopt,
        EmailVerificationRequestResult::kRpOriginIsOpaque);
    return;
  }

  // Step 3: Token Request

  // Step 3.1: the browser extracts the domain from the email address and
  // asks the DNS server who the issuer is:

  std::optional<std::string> domain = GetDomainFromEmail(email);
  if (!domain) {
    CompleteIsVerifiableRequest(std::move(callback), std::nullopt,
                                EmailVerificationRequestResult::kInvalidEmail);
    return;
  }
  std::string hostname = "_email-verification." + *domain;

  dns_request_->SendRequest(
      hostname, base::BindOnce(&EmailVerificationRequest::OnDnsRequestComplete,
                               weak_ptr_factory_.GetWeakPtr(), email,
                               std::move(callback)));
}

void EmailVerificationRequest::OnDnsRequestComplete(
    const std::string& email,
    EmailVerifier::IsVerifiableCallback callback,
    const std::optional<std::vector<std::string>>& text_records) {
  if (!render_frame_host_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  // Step 3.2: when the DNS response is received, the browser
  // parses the TXT record to extract the issuer's origin.
  if (!text_records || text_records->size() != 1) {
    CompleteIsVerifiableRequest(
        std::move(callback), std::nullopt,
        EmailVerificationRequestResult::kDnsFetchFailed);
    return;
  }

  const std::string& record = (*text_records)[0];
  static constexpr char kIssPrefix[] = "iss=";
  if (!base::StartsWith(record, kIssPrefix, base::CompareCase::SENSITIVE)) {
    CompleteIsVerifiableRequest(
        std::move(callback), std::nullopt,
        EmailVerificationRequestResult::kDnsInvalidRecord);
    return;
  }

  std::string iss = record.substr(sizeof(kIssPrefix) - 1);
  if (iss.empty()) {
    CompleteIsVerifiableRequest(
        std::move(callback), std::nullopt,
        EmailVerificationRequestResult::kDnsInvalidRecord);
    return;
  }

  GURL issuer("https://" + iss);

  auto well_known = base::MakeRefCounted<WellKnownOrError>();
  auto accounts = base::MakeRefCounted<AccountsOrError>();

  // Create a barrier closure that will run OnAccountStatusFetched when called
  // twice.
  base::RepeatingClosure barrier = base::BarrierClosure(
      2,
      base::BindOnce(&EmailVerificationRequest::OnAccountStatusFetched,
                     weak_ptr_factory_.GetWeakPtr(), well_known, accounts,
                     url::Origin::Create(issuer), email, std::move(callback)));

  // --- Task 1: Fetch .well-known/email-verification ---
  network_manager_->FetchWellKnown(
      issuer,
      base::BindOnce(
          &EmailVerificationRequest::OnEmailVerificationWellKnownFetched,
          weak_ptr_factory_.GetWeakPtr(), barrier, url::Origin::Create(issuer),
          well_known));

  // --- Task 2: Verify User is Logged In with the provided Email ---
  // If the issuer is found, the browser fetches the issuer's
  // .well-known/web-identity file:
  auto* permission_delegate = render_frame_host_->GetBrowserContext()
                                  ->GetFederatedIdentityPermissionContext();
  std::optional<bool> login_status =
      permission_delegate
          ? permission_delegate->GetIdpSigninStatus(url::Origin::Create(issuer))
          : std::nullopt;

  // Treat "unknown" login state as "logged-in" to avoid stopping early
  // unnecessarily. We only stop if we explicitly know the user is logged out.
  bool is_logged_in = login_status.value_or(true);
  if (!is_logged_in) {
    accounts->data =
        base::unexpected(EmailVerificationRequestResult::kUserLoggedOut);
    barrier.Run();
    return;
  }

  idp_network_manager_->FetchWellKnown(
      issuer,
      base::BindOnce(&EmailVerificationRequest::OnWebIdentityWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     url::Origin::Create(issuer), email, barrier, accounts));
}

void EmailVerificationRequest::OnEmailVerificationWellKnownFetched(
    base::RepeatingClosure barrier,
    const url::Origin& issuer,
    scoped_refptr<WellKnownOrError> well_known,
    FetchStatus status,
    EmailVerifierNetworkRequestManager::WellKnown fetched_well_known) {
  if (status.parse_status != ParseStatus::kSuccess) {
    well_known->data = base::unexpected(
        EmailVerificationWellKnownParseStatusToEvpRequestStatus(
            status.parse_status));
  } else {
    well_known->data = std::move(fetched_well_known);
  }
  barrier.Run();
}

void EmailVerificationRequest::OnWebIdentityWellKnownFetched(
    const url::Origin& issuer,
    const std::string& email,
    base::RepeatingClosure barrier,
    scoped_refptr<AccountsOrError> accounts,
    FetchStatus status,
    const IdpNetworkRequestManager::WellKnown& well_known) {
  if (status.parse_status != ParseStatus::kSuccess) {
    accounts->data = base::unexpected(
        WellKnownParseStatusToEvpRequestStatus(status.parse_status));
    barrier.Run();
    return;
  }

  if (well_known.accounts.is_empty()) {
    accounts->data = base::unexpected(
        EmailVerificationRequestResult::kWellKnownMissingAccountsEndpoint);
    barrier.Run();
    return;
  }

  if (!issuer.IsSameOriginWith(well_known.accounts)) {
    accounts->data = base::unexpected(
        EmailVerificationRequestResult::kWellKnownAccountsEndpointCrossOrigin);
    barrier.Run();
    return;
  }

  idp_network_manager_->SendAccountsRequest(
      url::Origin::Create(well_known.accounts), well_known.accounts,
      base::BindOnce(&EmailVerificationRequest::OnAccountsResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), email, barrier, accounts));
}

void EmailVerificationRequest::OnAccountsResponseReceived(
    const std::string& email,
    base::RepeatingClosure barrier,
    scoped_refptr<AccountsOrError> accounts,
    FetchStatus status,
    IdpNetworkRequestManager::AccountsResponse response) {
  if (status.parse_status != ParseStatus::kSuccess) {
    accounts->data = base::unexpected(
        AccountsListParseStatusToEvpRequestStatus(status.parse_status));
  } else {
    accounts->data = std::move(response.accounts);
  }
  barrier.Run();
}

void EmailVerificationRequest::OnAccountStatusFetched(
    scoped_refptr<WellKnownOrError> well_known,
    scoped_refptr<AccountsOrError> accounts,
    const url::Origin& issuer_origin,
    const std::string& email,
    EmailVerifier::IsVerifiableCallback callback) {
  if (!well_known->data.has_value()) {
    CompleteIsVerifiableRequest(std::move(callback), std::nullopt,
                                well_known->data.error());
    return;
  }

  if (!accounts->data.has_value()) {
    CompleteIsVerifiableRequest(std::move(callback), std::nullopt,
                                accounts->data.error());
    return;
  }

  // Step 3.3: when the .well-known/email-verification file is fetched,
  // the browser checks that the issuance_endpoint is present.
  if (well_known->data->issuance_endpoint.is_empty()) {
    CompleteIsVerifiableRequest(
        std::move(callback), std::nullopt,
        EmailVerificationRequestResult::kWellKnownMissingIssuanceEndpoint);
    return;
  }

  if (!issuer_origin.IsSameOriginWith(well_known->data->issuance_endpoint)) {
    CompleteIsVerifiableRequest(
        std::move(callback), std::nullopt,
        EmailVerificationRequestResult::kWellKnownIssuanceEndpointCrossOrigin);
    return;
  }

  bool email_matched = false;
  for (const auto& account : accounts->data.value()) {
    if (base::EqualsCaseInsensitiveASCII(account->email, email)) {
      email_matched = true;
      break;
    }
  }

  if (!email_matched) {
    CompleteIsVerifiableRequest(std::move(callback), std::nullopt,
                                EmailVerificationRequestResult::kUserLoggedOut);
    return;
  }

  EmailVerifier::Result result;
  result.email = email;
  result.issuer_site = net::SchemefulSite(issuer_origin.GetURL());
  result.issuance_endpoint = well_known->data->issuance_endpoint;
  result.jwks_uri = well_known->data->jwks_uri;
  result.signing_alg_values_supported =
      well_known->data->signing_alg_values_supported;

  CompleteIsVerifiableRequest(std::move(callback), std::move(result),
                              EmailVerificationRequestResult::kSuccess);
}

void EmailVerificationRequest::Verify(
    const EmailVerifier::Result& result,
    const std::string& nonce,
    EmailVerifier::OnEmailVerifiedCallback callback) {
  observers_.Notify(&Observer::OnVerifyStart, this);
  if (!render_frame_host_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  // Both conditions are met! Proceed to create token and send request.

  // TODO(crbug.com/380367784): understand and document why RSA was
  // preferred over ECDSA here.
  std::unique_ptr<crypto::keypair::PrivateKey> private_key;
  for (const auto& supported_alg : result.signing_alg_values_supported) {
    if (supported_alg == "EdDSA") {
      private_key = std::make_unique<crypto::keypair::PrivateKey>(
          crypto::keypair::PrivateKey::GenerateEd25519());
      break;
    } else if (supported_alg == "RS256") {
      private_key = std::make_unique<crypto::keypair::PrivateKey>(
          crypto::keypair::PrivateKey::GenerateRsa2048());
      break;
    } else if (supported_alg == "ES256") {
      private_key = std::make_unique<crypto::keypair::PrivateKey>(
          crypto::keypair::PrivateKey::GenerateEcP256());
      break;
    }
    // TODO(crbug.com/380367784): figure out what to do if we get an unsupported
    // algorithm here (should we reject? ignore?).
  }

  if (!private_key) {
    CompleteVerifyRequest(
        std::move(callback), std::nullopt,
        EmailVerificationRequestResult::kWellKnownUnsupportedSigningAlgorithm);
    return;
  }

  std::optional<sdjwt::Jwk> public_key = sdjwt::ExportPublicKey(*private_key);
  CHECK(public_key);

  sdjwt::Jwt jwt = CreateRequestToken(
      result.email, *public_key, url::Origin::Create(result.issuance_endpoint));

  auto signer = sdjwt::CreateJwtSigner(*private_key);
  CHECK(jwt.Sign(std::move(signer)));

  auto request_token = jwt.Serialize();
  CHECK(!request_token->empty());

  // Create shared objects to hold the results
  auto token = base::MakeRefCounted<TokenResultOrError>();
  auto jwks = base::MakeRefCounted<JwksResultOrError>();

  // Create BarrierClosure to wait for both requests.
  auto done_closure = base::BarrierClosure(
      2, base::BindOnce(&EmailVerificationRequest::OnTokenAndKeysFetchComplete,
                        weak_ptr_factory_.GetWeakPtr(), token, jwks,
                        url::Origin::Create(result.issuance_endpoint), nonce,
                        std::move(private_key), result.email,
                        std::move(callback)));

  // Step 3.5: finally, the browser sends a POST request to the
  // issuance_endpoint with the request_token as a form parameter.
  network_manager_->SendTokenRequest(
      result.issuance_endpoint, "request_token=" + request_token.value(),
      base::BindOnce(
          [](scoped_refptr<TokenResultOrError> token,
             base::RepeatingClosure closure, FetchStatus status,
             EmailVerifierNetworkRequestManager::TokenResult result) {
            if (status.parse_status == ParseStatus::kSuccess) {
              token->data = std::move(result);
            } else {
              token->data = base::unexpected(
                  TokenParseStatusToEvpRequestStatus(status.parse_status));
            }
            closure.Run();
          },
          token, done_closure));

  // Start JWKS Fetch.
  network_manager_->DownloadAndParseUncredentialedUrl(
      result.jwks_uri,
      base::BindOnce(
          [](scoped_refptr<JwksResultOrError> jwks,
             base::RepeatingClosure closure, FetchStatus status,
             std::optional<base::DictValue> result) {
            if (status.parse_status != ParseStatus::kSuccess) {
              jwks->data = base::unexpected(
                  EmailVerificationRequestResult::kJwksHttpNotFound);
            } else {
              jwks->data = std::move(*result);
            }
            closure.Run();
          },
          jwks, done_closure));
}

void EmailVerificationRequest::OnTokenAndKeysFetchComplete(
    scoped_refptr<TokenResultOrError> token,
    scoped_refptr<JwksResultOrError> jwks,
    const url::Origin& issuer,
    const std::string& nonce,
    std::unique_ptr<crypto::keypair::PrivateKey> private_key,
    const std::string& email,
    EmailVerifier::OnEmailVerifiedCallback callback) {
  if (!render_frame_host_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  // Step 5: Token Presentation

  // Check results.
  if (!token->data.has_value()) {
    CompleteVerifyRequest(std::move(callback), std::nullopt,
                          token->data.error());
    return;
  }

  if (!token->data.value().token || !token->data.value().token->is_string()) {
    CompleteVerifyRequest(
        std::move(callback), std::nullopt,
        EmailVerificationRequestResult::kTokenInvalidResponse);
    return;
  }

  auto parsed_token =
      sdjwt::SdJwt::Parse(token->data.value().token->GetString());

  // Step 5.1: The browser parses and verifies if the SD-JWT
  // is valid.
  if (!parsed_token) {
    CompleteVerifyRequest(std::move(callback), std::nullopt,
                          EmailVerificationRequestResult::kTokenMalformedSdJwt);
    return;
  }

  if (!jwks->data.has_value()) {
    CompleteVerifyRequest(std::move(callback), std::nullopt,
                          jwks->data.error());
    return;
  }

  auto sd_jwt = sdjwt::SdJwt::From(*parsed_token);

  if (!sd_jwt) {
    CompleteVerifyRequest(std::move(callback), std::nullopt,
                          EmailVerificationRequestResult::kTokenInvalidSdJwt);
    return;
  }

  // Step 5.2: bind the nonce and the website to the
  // key binding JWT.

  sdjwt::SdJwtKb sd_jwt_kb;
  sd_jwt_kb.sd_jwt = *sd_jwt;

  std::optional<sdjwt::Jwk> holder_pub_key =
      sdjwt::ExportPublicKey(*private_key);
  CHECK(holder_pub_key);

  sdjwt::Header header;
  header.alg = holder_pub_key->alg;
  header.typ = "kb+jwt";

  sdjwt::Payload payload;
  CHECK(render_frame_host_);
  payload.aud = render_frame_host_->GetLastCommittedOrigin().Serialize();
  payload.nonce = nonce;
  payload.iat = base::Time::Now();

  std::string sd_jwt_sha256 =
      crypto::SHA256HashString(token->data.value().token->GetString());
  std::string sd_hash;
  base::Base64UrlEncode(sd_jwt_sha256,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &sd_hash);
  payload.sd_hash = sdjwt::Base64String(sd_hash);

  sdjwt::Jwt kb_jwt;
  kb_jwt.header = *header.ToJson();
  kb_jwt.payload = *payload.ToJson();

  auto signer = sdjwt::CreateJwtSigner(*private_key);

  if (!kb_jwt.Sign(std::move(signer))) {
    CompleteVerifyRequest(
        std::move(callback), std::nullopt,
        EmailVerificationRequestResult::kKeyBindingSigningFailed);
    return;
  }

  sd_jwt_kb.kb_jwt = kb_jwt;

  std::string result = sd_jwt_kb.Serialize();

  EvtVerifier::Result verification_result =
      EvtVerifier::Verify(result, issuer, std::move(jwks->data).value(),
                          render_frame_host_->GetLastCommittedOrigin(), email,
                          nonce, *holder_pub_key);

  if (verification_result == EvtVerifier::Result::kVerified) {
    // Step 5.3: the browser notifies the page that
    // the SD-JWT+KB is ready.
    CompleteVerifyRequest(std::move(callback), result,
                          EmailVerificationRequestResult::kSuccess);
  } else {
    CompleteVerifyRequest(
        std::move(callback), std::nullopt,
        VerificationResultToEvpRequestStatus(verification_result));
  }
}

void EmailVerificationRequest::CompleteIsVerifiableRequest(
    EmailVerifier::IsVerifiableCallback callback,
    std::optional<EmailVerifier::Result> response,
    blink::mojom::EmailVerificationRequestResult status) {
  base::UmaHistogramEnumeration("Blink.Evp.Status.IsVerifiable", status);
  observers_.Notify(&Observer::OnIsVerifiableComplete, this, status);
  if (status != EmailVerificationRequestResult::kSuccess) {
    MaybeAddDevToolsIssue(status);
  }
  std::move(callback).Run(std::move(response));
}

void EmailVerificationRequest::CompleteVerifyRequest(
    EmailVerifier::OnEmailVerifiedCallback callback,
    std::optional<std::string> response,
    blink::mojom::EmailVerificationRequestResult status) {
  base::UmaHistogramEnumeration("Blink.Evp.Status.Verify", status);
  observers_.Notify(&Observer::OnVerifyComplete, this, status);
  if (status != EmailVerificationRequestResult::kSuccess) {
    MaybeAddDevToolsIssue(status);
  }
  std::move(callback).Run(std::move(response));
}

void EmailVerificationRequest::MaybeAddDevToolsIssue(
    EmailVerificationRequestResult status) {
  DCHECK_NE(status, EmailVerificationRequestResult::kSuccess);

  if (!render_frame_host_) {
    return;
  }

  switch (status) {
    case EmailVerificationRequestResult::kSuccess:
      NOTREACHED();

    // Do not report DNS fetch failures to DevTools. This prevents spamming the
    // DevTools console with issues when the user autofills an email from a
    // non-EVP-compatible provider (which is currently the common case and
    // fails the DNS check).
    case EmailVerificationRequestResult::kDnsFetchFailed:
      return;

    case EmailVerificationRequestResult::kRpOriginIsOpaque:
    case EmailVerificationRequestResult::kInvalidEmail:
    case EmailVerificationRequestResult::kDnsInvalidRecord:
    case EmailVerificationRequestResult::kWellKnownHttpNotFound:
    case EmailVerificationRequestResult::kWellKnownNoResponse:
    case EmailVerificationRequestResult::kWellKnownInvalidResponse:
    case EmailVerificationRequestResult::kWellKnownListEmpty:
    case EmailVerificationRequestResult::kWellKnownInvalidContentType:
    case EmailVerificationRequestResult::kWellKnownMissingIssuanceEndpoint:
    case EmailVerificationRequestResult::kWellKnownIssuanceEndpointCrossOrigin:
    case EmailVerificationRequestResult::kWellKnownUnsupportedSigningAlgorithm:
    case EmailVerificationRequestResult::kTokenHttpNotFound:
    case EmailVerificationRequestResult::kTokenNoResponse:
    case EmailVerificationRequestResult::kTokenInvalidResponse:
    case EmailVerificationRequestResult::kTokenInvalidContentType:
    case EmailVerificationRequestResult::kTokenMalformedSdJwt:
    case EmailVerificationRequestResult::kTokenInvalidSdJwt:
    case EmailVerificationRequestResult::kKeyBindingSigningFailed:
    case EmailVerificationRequestResult::kWellKnownMissingAccountsEndpoint:
    case EmailVerificationRequestResult::kUserLoggedOut:
    case EmailVerificationRequestResult::kWellKnownAccountsEndpointCrossOrigin:
    case EmailVerificationRequestResult::kAccountsHttpNotFound:
    case EmailVerificationRequestResult::kAccountsNoResponse:
    case EmailVerificationRequestResult::kAccountsInvalidResponse:
    case EmailVerificationRequestResult::kAccountsInvalidContentType:
    case EmailVerificationRequestResult::kAccountsEmptyList:
    case EmailVerificationRequestResult::
        kEmailVerificationWellKnownHttpNotFound:
    case EmailVerificationRequestResult::kEmailVerificationWellKnownNoResponse:
    case EmailVerificationRequestResult::
        kEmailVerificationWellKnownInvalidResponse:
    case EmailVerificationRequestResult::
        kEmailVerificationWellKnownInvalidContentType:
    case EmailVerificationRequestResult::kJwksHttpNotFound:
    case EmailVerificationRequestResult::kJwksInvalidResponse:
    case EmailVerificationRequestResult::
        kTokenVerificationSdJwtUnsupportedHeaderAlg:
    case EmailVerificationRequestResult::kTokenVerificationSdJwtMissingIss:
    case EmailVerificationRequestResult::kTokenVerificationSdJwtMissingIat:
    case EmailVerificationRequestResult::kTokenVerificationSdJwtMissingCnf:
    case EmailVerificationRequestResult::kTokenVerificationSdJwtMissingEmail:
    case EmailVerificationRequestResult::kTokenVerificationSdJwtInvalidIssuedAt:
    case EmailVerificationRequestResult::kTokenVerificationSdJwtInvalidIssuer:
    case EmailVerificationRequestResult::kTokenVerificationSdJwtJwksMissingKeys:
    case EmailVerificationRequestResult::kTokenVerificationSdJwtSignatureFailed:
    case EmailVerificationRequestResult::
        kTokenVerificationSdJwtInvalidEmailVerified:
    case EmailVerificationRequestResult::kTokenVerificationSdJwtInvalidEmail:
    case EmailVerificationRequestResult::
        kTokenVerificationSdJwtInvalidHolderKey:
    case EmailVerificationRequestResult::kTokenVerificationKbInvalidTyp:
    case EmailVerificationRequestResult::kTokenVerificationKbMissingAud:
    case EmailVerificationRequestResult::kTokenVerificationKbMissingNonce:
    case EmailVerificationRequestResult::kTokenVerificationKbMissingIat:
    case EmailVerificationRequestResult::kTokenVerificationKbMissingSdHash:
    case EmailVerificationRequestResult::kTokenVerificationKbInvalidIssuedAt:
    case EmailVerificationRequestResult::kTokenVerificationKbInvalidAudience:
    case EmailVerificationRequestResult::kTokenVerificationKbInvalidNonce:
    case EmailVerificationRequestResult::kTokenVerificationKbInvalidSdHash:
    case EmailVerificationRequestResult::kTokenVerificationKbMissingCnf:
    case EmailVerificationRequestResult::kTokenVerificationKbSignatureFailed:
      break;
  }

  auto details = blink::mojom::InspectorIssueDetails::New();
  auto email_verification_request_details =
      blink::mojom::EmailVerificationRequestIssueDetails::New(status);
  details->email_verification_request_details =
      std::move(email_verification_request_details);
  render_frame_host_->ReportInspectorIssue(
      blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kEmailVerificationRequestIssue,
          std::move(details)));
}

}  // namespace content::webid
