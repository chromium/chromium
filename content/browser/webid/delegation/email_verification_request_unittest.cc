// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verification_request.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/dns_request.h"
#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"
#include "content/browser/webid/delegation/jwt_signer.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "crypto/sha2.h"
#include "net/base/schemeful_site.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::EmailVerificationRequestResult;
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::WithArgs;

namespace content::webid {

using blink::mojom::EmailVerificationRequestResult;

// Mock DnsRequest for testing
class MockDnsRequest : public DnsRequest {
 public:
  explicit MockDnsRequest()
      : DnsRequest(base::BindRepeating(
            []() -> EmailVerifierNetworkRequestManager* { return nullptr; })) {}
  ~MockDnsRequest() override = default;

  MOCK_METHOD(void,
              SendRequest,
              (const std::string& hostname, DnsRequestCallback callback),
              (override));
};

class MockEmailVerifierNetworkRequestManager
    : public EmailVerifierNetworkRequestManager {
 public:
  MockEmailVerifierNetworkRequestManager()
      : EmailVerifierNetworkRequestManager(url::Origin(),
                                           nullptr,
                                           nullptr,
                                           content::FrameTreeNodeId()) {}
  ~MockEmailVerifierNetworkRequestManager() override = default;

  MOCK_METHOD(void,
              FetchWellKnown,
              (const GURL&, FetchWellKnownCallback),
              (override));
  MOCK_METHOD(void,
              SendTokenRequest,
              (const GURL&, const std::string&, TokenRequestCallback),
              (override));
  MOCK_METHOD(void,
              DownloadAndParseUncredentialedUrl,
              (const GURL&, ParseJsonCallback),
              (override));
};

class EmailVerificationRequestTest : public RenderViewHostTestHarness {
 public:
  EmailVerificationRequestTest() = default;

 protected:
  const url::Origin kRpOrigin =
      url::Origin::Create(GURL("https://rp.example.com"));
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(EmailVerificationRequestTest, SuccessfulVerification) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  NiceMock<MockIdpNetworkRequestManager>* mock_idp_network_manager_ =
      mock_idp_network_manager_ptr.get();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*main_rfh()));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const std::string kIssuer = "issuer.example.com";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");
  const GURL kJwksUri = GURL("https://issuer.example.com/jwks");

  const std::string kToken = "test_token";

  // Generate issuer key pair for signing and verification.
  auto issuer_key = crypto::keypair::PrivateKey::GenerateRsa2048();

  // Construct JWKS.
  base::DictValue jwks;
  base::ListValue keys;
  auto jwk = sdjwt::ExportPublicKey(issuer_key);
  ASSERT_TRUE(jwk);
  base::DictValue key_dict = jwk->ToDict();
  key_dict.Set("kid", "test_kid");

  keys.Append(std::move(key_dict));
  jwks.Set("keys", std::move(keys));

  EXPECT_CALL(*mock_dns_request,
              SendRequest("_email-verification.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(
            std::vector<std::string>{"iss=issuer.example.com"});
      }));

  EXPECT_CALL(*mock_network_manager, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](EmailVerifierNetworkRequestManager::FetchWellKnownCallback
                  callback) {
            EmailVerifierNetworkRequestManager::WellKnown well_known;
            well_known.issuance_endpoint = kIssuanceEndpoint;
            well_known.jwks_uri = kJwksUri;
            well_known.signing_alg_values_supported.push_back("RS256");
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_network_manager,
              DownloadAndParseUncredentialedUrl(kJwksUri, _))
      .WillOnce(WithArgs<1>([&](ParseJsonCallback callback) {
        std::move(callback).Run({ParseStatus::kSuccess},
                                base::Value(std::move(jwks)));
      }));

  const GURL kAccountsEndpoint = GURL("https://issuer.example.com/accounts");

  EXPECT_CALL(*mock_idp_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.accounts = kAccountsEndpoint;
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_,
              SendAccountsRequest(_, kAccountsEndpoint, _, _))
      .WillOnce(WithArgs<3>(
          [&](IdpNetworkRequestManager::AccountsRequestCallback callback) {
            IdpNetworkRequestManager::AccountsResponse response;
            auto account = base::MakeRefCounted<IdentityRequestAccount>(
                "id", "email", "name", kEmail, "name", "given_name", GURL(),
                "phone", "username", std::vector<std::string>(),
                std::vector<std::string>(), std::vector<std::string>(),
                std::vector<std::string>());
            response.accounts.push_back(account);
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    std::move(response));
            return true;
          }));

  EXPECT_CALL(*mock_network_manager, SendTokenRequest(kIssuanceEndpoint, _, _))
      .WillOnce(WithArgs<1, 2>(
          [&](const std::string& url_encoded_post_data,
              EmailVerifierNetworkRequestManager::TokenRequestCallback
                  callback) {
            base::StringPairs params;
            EXPECT_TRUE(base::SplitStringIntoKeyValuePairs(
                url_encoded_post_data, '=', '&', &params));
            EXPECT_EQ(params.size(), 1u);
            EXPECT_EQ(params[0].first, "request_token");
            EXPECT_FALSE(params[0].second.empty());

            auto jwt_json = sdjwt::Jwt::Parse(params[0].second);
            EXPECT_TRUE(jwt_json);

            auto jwt = sdjwt::Jwt::From(*jwt_json);
            EXPECT_TRUE(jwt);

            auto header = sdjwt::Header::From(*base::JSONReader::ReadDict(
                jwt->header.value(), base::JSON_PARSE_CHROMIUM_EXTENSIONS));
            EXPECT_TRUE(header);
            EXPECT_EQ(header->typ, "JWT");
            EXPECT_EQ(header->alg, "RS256");
            // Asserts that the JWK is present in the header.
            EXPECT_TRUE(header->jwk);

            auto payload = sdjwt::Payload::From(*base::JSONReader::ReadDict(
                jwt->payload.value(), base::JSON_PARSE_CHROMIUM_EXTENSIONS));
            EXPECT_TRUE(payload);
            EXPECT_EQ(payload->aud,
                      url::Origin::Create(kIssuerUrl).Serialize());
            EXPECT_EQ(payload->email, kEmail);

            sdjwt::SdJwt token;
            sdjwt::Header h;
            h.typ = "web-identity+sd-jwt";
            h.alg = "RS256";
            h.kid = "test_kid";
            sdjwt::Payload p;
            p.iss = url::Origin::Create(kIssuerUrl).Serialize();
            p.email = kEmail;
            p.email_verified = true;
            p.iat = base::Time::Now();
            sdjwt::ConfirmationKey cnf;
            cnf.jwk = *(header->jwk);
            p.cnf = cnf;

            auto signer = sdjwt::CreateJwtSigner(issuer_key);

            sdjwt::Jwt issued_jwt;
            issued_jwt.header = *(h.ToJson());
            issued_jwt.payload = *(p.ToJson());
            EXPECT_TRUE(issued_jwt.Sign(std::move(signer)));

            token.jwt = issued_jwt;

            EmailVerifierNetworkRequestManager::TokenResult result;
            result.token = base::Value(token.Serialize());
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    std::move(result));
          }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>> is_verifiable;
  std::string nonce = kNonce;
  base::Time before = base::Time::Now();
  email_verification_request_.CheckIfVerifiable(kEmail,
                                                is_verifiable.GetCallback());
  auto issuer = is_verifiable.Get();
  ASSERT_TRUE(issuer.has_value());

  base::test::TestFuture<std::optional<std::string>> future;
  email_verification_request_.Verify(*issuer, nonce, future.GetCallback());
  std::optional<std::string> result = future.Get();
  base::Time after = base::Time::Now();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(issuer->issuer_site,
            net::SchemefulSite(GURL("https://example.com")));

  auto sd_jwt_kb = sdjwt::SdJwtKb::Parse(*result);
  ASSERT_TRUE(sd_jwt_kb);

  auto kb_jwt_json = sdjwt::Jwt::Parse(sd_jwt_kb->kb_jwt.Serialize().value());
  ASSERT_TRUE(kb_jwt_json);
  auto kb_jwt = sdjwt::Jwt::From(*kb_jwt_json);
  ASSERT_TRUE(kb_jwt);
  auto kb_payload = sdjwt::Payload::From(*base::JSONReader::ReadDict(
      kb_jwt->payload.value(), base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  ASSERT_TRUE(kb_payload);
  ASSERT_EQ(kb_payload->aud, main_rfh()->GetLastCommittedOrigin().Serialize());
  ASSERT_EQ(kb_payload->nonce, kNonce);
  ASSERT_TRUE(kb_payload->iat);
  base::Time iat_time = kb_payload->iat.value();
  // The `iat` is seconds since the epoch, so there is a loss of precision.
  // We check that `iat` is between `before` and `after`, allowing for a
  // tolerance of 1 second for the loss of precision.
  EXPECT_GE(iat_time, before - base::Seconds(1));
  EXPECT_LE(iat_time, after + base::Seconds(1));

  std::string sd_jwt_sha256 =
      crypto::SHA256HashString(sd_jwt_kb->sd_jwt.Serialize());
  std::string sd_hash_expected;
  base::Base64UrlEncode(sd_jwt_sha256,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &sd_hash_expected);
  ASSERT_EQ(kb_payload->sd_hash.value(), sd_hash_expected);
  histogram_tester.ExpectUniqueSample("Blink.Evp.Status.IsVerifiable",
                                      EmailVerificationRequestResult::kSuccess,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.Verify", EmailVerificationRequestResult::kSuccess, 1);
  EXPECT_EQ(0, static_cast<TestRenderFrameHost*>(main_rfh())
                   ->GetEmailVerificationRequestIssueCount(std::nullopt));
}

TEST_F(EmailVerificationRequestTest, CaseInsensitiveEmailMatch) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  NiceMock<MockIdpNetworkRequestManager>* mock_idp_network_manager_ =
      mock_idp_network_manager_ptr.get();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      *static_cast<RenderFrameHostImpl*>(main_rfh()));

  const std::string kEmail = "test@issuer.example.com";
  const std::string kEmailUpper = "TEST@ISSUER.EXAMPLE.COM";
  const std::string kNonce = "test_nonce";
  const std::string kIssuer = "issuer.example.com";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");
  // Generate issuer key pair for signing and verification.
  auto issuer_key = crypto::keypair::PrivateKey::GenerateRsa2048();

  // Construct JWKS.
  base::DictValue jwks;
  base::ListValue keys;
  auto jwk = sdjwt::ExportPublicKey(issuer_key);
  ASSERT_TRUE(jwk);
  base::DictValue key_dict = jwk->ToDict();
  key_dict.Set("kid", "test_kid");

  keys.Append(std::move(key_dict));
  jwks.Set("keys", std::move(keys));

  const GURL kJwksUri = GURL("https://issuer.example.com/jwks");

  EXPECT_CALL(*mock_network_manager,
              DownloadAndParseUncredentialedUrl(kJwksUri, _))
      .WillOnce(WithArgs<1>([&](ParseJsonCallback callback) {
        std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                base::Value(std::move(jwks)));
      }));
  EXPECT_CALL(*mock_dns_request,
              SendRequest("_email-verification.issuer.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(
            std::vector<std::string>{"iss=issuer.example.com"});
      }));

  EXPECT_CALL(*mock_network_manager, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](EmailVerifierNetworkRequestManager::FetchWellKnownCallback
                  callback) {
            EmailVerifierNetworkRequestManager::WellKnown well_known;
            well_known.issuance_endpoint = kIssuanceEndpoint;
            well_known.jwks_uri = kJwksUri;
            well_known.signing_alg_values_supported.push_back("RS256");
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  const GURL kAccountsEndpoint = GURL("https://issuer.example.com/accounts");

  EXPECT_CALL(*mock_idp_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.accounts = kAccountsEndpoint;
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_,
              SendAccountsRequest(_, kAccountsEndpoint, _, _))
      .WillOnce(WithArgs<3>(
          [&](IdpNetworkRequestManager::AccountsRequestCallback callback) {
            IdpNetworkRequestManager::AccountsResponse response;
            auto account = base::MakeRefCounted<IdentityRequestAccount>(
                "id", "email", "name", kEmailUpper, "name", "given_name",
                GURL(), "phone", "username", std::vector<std::string>(),
                std::vector<std::string>(), std::vector<std::string>(),
                std::vector<std::string>());
            response.accounts.push_back(account);
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    std::move(response));
            return true;
          }));

  EXPECT_CALL(*mock_network_manager, SendTokenRequest(kIssuanceEndpoint, _, _))
      .WillOnce(WithArgs<1, 2>(
          [&](const std::string& url_encoded_post_data,
              EmailVerifierNetworkRequestManager::TokenRequestCallback
                  callback) {
            base::StringPairs params;
            EXPECT_TRUE(base::SplitStringIntoKeyValuePairs(
                url_encoded_post_data, '=', '&', &params));
            EXPECT_EQ(params.size(), 1u);
            EXPECT_EQ(params[0].first, "request_token");
            EXPECT_FALSE(params[0].second.empty());

            auto jwt_json = sdjwt::Jwt::Parse(params[0].second);
            EXPECT_TRUE(jwt_json);

            auto jwt = sdjwt::Jwt::From(*jwt_json);
            EXPECT_TRUE(jwt);

            auto header = sdjwt::Header::From(*base::JSONReader::ReadDict(
                jwt->header.value(), base::JSON_PARSE_CHROMIUM_EXTENSIONS));
            EXPECT_TRUE(header);
            EXPECT_EQ(header->typ, "JWT");
            EXPECT_EQ(header->alg, "RS256");
            EXPECT_TRUE(header->jwk);

            auto payload = sdjwt::Payload::From(*base::JSONReader::ReadDict(
                jwt->payload.value(), base::JSON_PARSE_CHROMIUM_EXTENSIONS));
            EXPECT_TRUE(payload);
            EXPECT_EQ(payload->aud,
                      url::Origin::Create(kIssuerUrl).Serialize());
            EXPECT_EQ(payload->email, kEmail);

            sdjwt::SdJwt token;
            sdjwt::Header h;
            h.typ = "web-identity+sd-jwt";
            h.alg = "RS256";
            sdjwt::Payload p;
            p.iss = url::Origin::Create(kIssuerUrl).Serialize();
            p.email = kEmail;
            p.iat = base::Time::Now();
            p.email_verified = true;
            sdjwt::ConfirmationKey cnf;
            cnf.jwk = *(header->jwk);
            p.cnf = cnf;

            auto signer = sdjwt::CreateJwtSigner(issuer_key);

            sdjwt::Jwt issued_jwt;
            issued_jwt.header = *(h.ToJson());
            issued_jwt.payload = *(p.ToJson());
            EXPECT_TRUE(issued_jwt.Sign(std::move(signer)));

            token.jwt = issued_jwt;

            EmailVerifierNetworkRequestManager::TokenResult result;
            result.token = base::Value(token.Serialize());
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    std::move(result));
          }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  std::optional<EmailVerifier::Result> result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->issuer_site,
            net::SchemefulSite(GURL("https://example.com")));

  base::test::TestFuture<std::optional<std::string>> verify_future;
  email_verification_request_.Verify(*result, kNonce,
                                     verify_future.GetCallback());
  EXPECT_TRUE(verify_future.Get().has_value());
}

TEST_F(EmailVerificationRequestTest, CrossOriginIssuanceEndpointRejected) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  NiceMock<MockIdpNetworkRequestManager>* mock_idp_network_manager_ =
      mock_idp_network_manager_ptr.get();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*main_rfh()));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");
  const GURL kCrossOriginIssuanceEndpoint = GURL("https://attacker.com/token");

  EXPECT_CALL(*mock_dns_request,
              SendRequest("_email-verification.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(
            std::vector<std::string>{"iss=issuer.example.com"});
      }));

  EXPECT_CALL(*mock_network_manager, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](EmailVerifierNetworkRequestManager::FetchWellKnownCallback
                  callback) {
            EmailVerifierNetworkRequestManager::WellKnown well_known;
            well_known.issuance_endpoint = kCrossOriginIssuanceEndpoint;
            well_known.signing_alg_values_supported.push_back("RS256");
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  const GURL kAccountsEndpoint = GURL("https://issuer.example.com/accounts");

  EXPECT_CALL(*mock_idp_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.accounts = kAccountsEndpoint;
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_,
              SendAccountsRequest(_, kAccountsEndpoint, _, _))
      .WillOnce(WithArgs<3>(
          [&](IdpNetworkRequestManager::AccountsRequestCallback callback) {
            IdpNetworkRequestManager::AccountsResponse response;
            auto account = base::MakeRefCounted<IdentityRequestAccount>(
                "id", "email", "name", kEmail, "name", "given_name", GURL(),
                "phone", "username", std::vector<std::string>(),
                std::vector<std::string>(), std::vector<std::string>(),
                std::vector<std::string>());
            response.accounts.push_back(account);
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    std::move(response));
            return true;
          }));

  // SendTokenRequest should NOT be called.
  EXPECT_CALL(*mock_network_manager, SendTokenRequest).Times(0);

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kWellKnownIssuanceEndpointCrossOrigin, 1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(main_rfh())
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::
                           kWellKnownIssuanceEndpointCrossOrigin));
}

TEST_F(EmailVerificationRequestTest, UserLoggedOut) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request_ = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager_ =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  NiceMock<MockIdpNetworkRequestManager>* mock_idp_network_manager_ =
      mock_idp_network_manager_ptr.get();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*main_rfh()));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");
  const GURL kAccountsEndpoint = GURL("https://issuer.example.com/accounts");

  EXPECT_CALL(*mock_dns_request_,
              SendRequest("_email-verification.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(
            std::vector<std::string>{"iss=issuer.example.com"});
      }));

  EXPECT_CALL(*mock_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](EmailVerifierNetworkRequestManager::FetchWellKnownCallback
                  callback) {
            EmailVerifierNetworkRequestManager::WellKnown well_known;
            well_known.issuance_endpoint = kIssuanceEndpoint;
            well_known.signing_alg_values_supported.push_back("RS256");
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.accounts = kAccountsEndpoint;
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_,
              SendAccountsRequest(_, kAccountsEndpoint, _, _))
      .WillOnce(WithArgs<3>(
          [&](IdpNetworkRequestManager::AccountsRequestCallback callback) {
            IdpNetworkRequestManager::AccountsResponse response;
            auto account = base::MakeRefCounted<IdentityRequestAccount>(
                "id", "email", "name", "different@example.com", "name",
                "given_name", GURL(), "phone", "username",
                std::vector<std::string>(), std::vector<std::string>(),
                std::vector<std::string>(), std::vector<std::string>());
            response.accounts.push_back(account);
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    std::move(response));
            return true;
          }));

  EXPECT_CALL(*mock_network_manager_, SendTokenRequest).Times(0);

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kUserLoggedOut, 1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(main_rfh())
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::kUserLoggedOut));
}

TEST_F(EmailVerificationRequestTest, AccountsListEmpty) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request_ = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager_ =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  NiceMock<MockIdpNetworkRequestManager>* mock_idp_network_manager_ =
      mock_idp_network_manager_ptr.get();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*main_rfh()));

  const std::string kEmail = "test@example.com";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");
  const GURL kAccountsEndpoint = GURL("https://issuer.example.com/accounts");

  EXPECT_CALL(*mock_dns_request_,
              SendRequest("_email-verification.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(
            std::vector<std::string>{"iss=issuer.example.com"});
      }));

  EXPECT_CALL(*mock_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](EmailVerifierNetworkRequestManager::FetchWellKnownCallback
                  callback) {
            EmailVerifierNetworkRequestManager::WellKnown well_known;
            well_known.issuance_endpoint = kIssuanceEndpoint;
            well_known.signing_alg_values_supported.push_back("RS256");
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.accounts = kAccountsEndpoint;
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_,
              SendAccountsRequest(_, kAccountsEndpoint, _, _))
      .WillOnce(WithArgs<3>(
          [&](IdpNetworkRequestManager::AccountsRequestCallback callback) {
            IdpNetworkRequestManager::AccountsResponse response;
            std::move(callback).Run(FetchStatus{ParseStatus::kEmptyListError},
                                    std::move(response));
            return true;
          }));

  EXPECT_CALL(*mock_network_manager_, SendTokenRequest).Times(0);

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kAccountsEmptyList, 1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(main_rfh())
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::kAccountsEmptyList));
}

TEST_F(EmailVerificationRequestTest, UnsupportedSigningAlgorithm) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request_ = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager_ =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  NiceMock<MockIdpNetworkRequestManager>* mock_idp_network_manager_ =
      mock_idp_network_manager_ptr.get();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*main_rfh()));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");
  const GURL kAccountsEndpoint = GURL("https://issuer.example.com/accounts");

  EXPECT_CALL(*mock_dns_request_,
              SendRequest("_email-verification.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(
            std::vector<std::string>{"iss=issuer.example.com"});
      }));

  EXPECT_CALL(*mock_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](EmailVerifierNetworkRequestManager::FetchWellKnownCallback
                  callback) {
            EmailVerifierNetworkRequestManager::WellKnown well_known;
            well_known.issuance_endpoint = kIssuanceEndpoint;
            // Add ONLY unsupported algorithms!
            well_known.signing_alg_values_supported.push_back("HS256");
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.accounts = kAccountsEndpoint;
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_,
              SendAccountsRequest(_, kAccountsEndpoint, _, _))
      .WillOnce(WithArgs<3>(
          [&](IdpNetworkRequestManager::AccountsRequestCallback callback) {
            IdpNetworkRequestManager::AccountsResponse response;
            auto account = base::MakeRefCounted<IdentityRequestAccount>(
                "id", "email", "name", kEmail, "name", "given_name", GURL(),
                "phone", "username", std::vector<std::string>(),
                std::vector<std::string>(), std::vector<std::string>(),
                std::vector<std::string>());
            response.accounts.push_back(account);
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    std::move(response));
            return true;
          }));

  EXPECT_CALL(*mock_network_manager_, SendTokenRequest).Times(0);

  base::test::TestFuture<std::optional<EmailVerifier::Result>> is_verifiable;
  email_verification_request_.CheckIfVerifiable(kEmail,
                                                is_verifiable.GetCallback());
  auto issuer = is_verifiable.Get();
  ASSERT_TRUE(issuer.has_value());

  base::test::TestFuture<std::optional<std::string>> future;
  email_verification_request_.Verify(*issuer, kNonce, future.GetCallback());
  std::optional<std::string> token = future.Get();
  EXPECT_FALSE(token.has_value());

  histogram_tester.ExpectUniqueSample("Blink.Evp.Status.IsVerifiable",
                                      EmailVerificationRequestResult::kSuccess,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.Verify",
      EmailVerificationRequestResult::kWellKnownUnsupportedSigningAlgorithm, 1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(main_rfh())
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::
                           kWellKnownUnsupportedSigningAlgorithm));
}

TEST_F(EmailVerificationRequestTest, WebIdentityWellKnownHttpNotFound) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request_ = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager_ =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  NiceMock<MockIdpNetworkRequestManager>* mock_idp_network_manager_ =
      mock_idp_network_manager_ptr.get();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*main_rfh()));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");

  EXPECT_CALL(*mock_dns_request_,
              SendRequest("_email-verification.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(
            std::vector<std::string>{"iss=issuer.example.com"});
      }));

  EXPECT_CALL(*mock_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](EmailVerifierNetworkRequestManager::FetchWellKnownCallback
                  callback) {
            EmailVerifierNetworkRequestManager::WellKnown well_known;
            well_known.issuance_endpoint = kIssuanceEndpoint;
            well_known.signing_alg_values_supported.push_back("RS256");
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  // Task 2 FAILS!
  EXPECT_CALL(*mock_idp_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            std::move(callback).Run(
                FetchStatus{ParseStatus::kHttpNotFoundError},
                IdpNetworkRequestManager::WellKnown());
          }));

  // SendTokenRequest should NOT be called.
  EXPECT_CALL(*mock_network_manager_, SendTokenRequest).Times(0);

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kWellKnownHttpNotFound, 1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(main_rfh())
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::kWellKnownHttpNotFound));
}

TEST_F(EmailVerificationRequestTest, OpaqueOriginRejected) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("data:text/html,<html></html>"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();

  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*main_rfh()));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";

  EXPECT_CALL(*mock_dns_request, SendRequest).Times(0);
  EXPECT_CALL(*mock_network_manager, FetchWellKnown).Times(0);
  EXPECT_CALL(*mock_network_manager, SendTokenRequest).Times(0);

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kRpOriginIsOpaque, 1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(main_rfh())
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::kRpOriginIsOpaque));
}

TEST_F(EmailVerificationRequestTest, DnsFetchFailed) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*main_rfh()));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";

  EXPECT_CALL(*mock_dns_request,
              SendRequest("_email-verification.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(std::nullopt);
      }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kDnsFetchFailed, 1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(main_rfh())
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::kDnsFetchFailed));
}

TEST_F(EmailVerificationRequestTest, WellKnownHttpNotFound) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  NiceMock<MockIdpNetworkRequestManager>* mock_idp_network_manager_ =
      mock_idp_network_manager_ptr.get();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*main_rfh()));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");

  EXPECT_CALL(*mock_dns_request,
              SendRequest("_email-verification.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(
            std::vector<std::string>{"iss=issuer.example.com"});
      }));

  EXPECT_CALL(*mock_network_manager, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](EmailVerifierNetworkRequestManager::FetchWellKnownCallback
                  callback) {
            std::move(callback).Run(
                FetchStatus{ParseStatus::kHttpNotFoundError},
                EmailVerifierNetworkRequestManager::WellKnown());
          }));

  const GURL kAccountsEndpoint = GURL("https://issuer.example.com/accounts");

  EXPECT_CALL(*mock_idp_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.accounts = kAccountsEndpoint;
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_,
              SendAccountsRequest(_, kAccountsEndpoint, _, _))
      .WillOnce(WithArgs<3>(
          [&](IdpNetworkRequestManager::AccountsRequestCallback callback) {
            IdpNetworkRequestManager::AccountsResponse response;
            auto account = base::MakeRefCounted<IdentityRequestAccount>(
                "id", "email", "name", kEmail, "name", "given_name", GURL(),
                "phone", "username", std::vector<std::string>(),
                std::vector<std::string>(), std::vector<std::string>(),
                std::vector<std::string>());
            response.accounts.push_back(account);
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    std::move(response));
            return true;
          }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kEmailVerificationWellKnownHttpNotFound,
      1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(main_rfh())
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::
                           kEmailVerificationWellKnownHttpNotFound));
}

TEST_F(EmailVerificationRequestTest, TokenInvalidResponse) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  NiceMock<MockIdpNetworkRequestManager>* mock_idp_network_manager_ =
      mock_idp_network_manager_ptr.get();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*main_rfh()));

  const std::string kEmail = "test@issuer.example.com";
  const std::string kNonce = "test_nonce";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");
  const GURL kJwksUri = GURL("https://issuer.example.com/jwks");

  EXPECT_CALL(*mock_network_manager, DownloadAndParseUncredentialedUrl(_, _))
      .WillOnce(WithArgs<1>([&](ParseJsonCallback callback) {
        base::DictValue empty_dict;
        std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                base::Value(std::move(empty_dict)));
      }));
  EXPECT_CALL(*mock_dns_request,
              SendRequest("_email-verification.issuer.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(
            std::vector<std::string>{"iss=issuer.example.com"});
      }));

  EXPECT_CALL(*mock_network_manager, FetchWellKnown(_, _))
      .WillOnce(WithArgs<1>(
          [&](EmailVerifierNetworkRequestManager::FetchWellKnownCallback
                  callback) {
            EmailVerifierNetworkRequestManager::WellKnown well_known;
            well_known.issuance_endpoint = kIssuanceEndpoint;
            well_known.jwks_uri = GURL("https://issuer.example.com/jwks");
            well_known.signing_alg_values_supported.push_back("RS256");
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  const GURL kAccountsEndpoint = GURL("https://issuer.example.com/accounts");

  EXPECT_CALL(*mock_idp_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            IdpNetworkRequestManager::WellKnown well_known;
            well_known.accounts = kAccountsEndpoint;
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_,
              SendAccountsRequest(_, kAccountsEndpoint, _, _))
      .WillOnce(WithArgs<3>(
          [&](IdpNetworkRequestManager::AccountsRequestCallback callback) {
            IdpNetworkRequestManager::AccountsResponse response;
            auto account = base::MakeRefCounted<IdentityRequestAccount>(
                "id", "email", "name", kEmail, "name", "given_name", GURL(),
                "phone", "username", std::vector<std::string>(),
                std::vector<std::string>(), std::vector<std::string>(),
                std::vector<std::string>());
            response.accounts.push_back(account);
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    std::move(response));
            return true;
          }));

  EXPECT_CALL(*mock_network_manager, SendTokenRequest(kIssuanceEndpoint, _, _))
      .WillOnce(WithArgs<2>(
          [&](EmailVerifierNetworkRequestManager::TokenRequestCallback
                  callback) {
            std::move(callback).Run(
                FetchStatus{ParseStatus::kInvalidResponseError},
                EmailVerifierNetworkRequestManager::TokenResult());
          }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>> is_verifiable;
  email_verification_request_.CheckIfVerifiable(kEmail,
                                                is_verifiable.GetCallback());
  auto issuer = is_verifiable.Get();
  ASSERT_TRUE(issuer.has_value());

  base::test::TestFuture<std::optional<std::string>> future;
  email_verification_request_.Verify(*issuer, kNonce, future.GetCallback());
  std::optional<std::string> token = future.Get();
  EXPECT_FALSE(token.has_value());
  histogram_tester.ExpectUniqueSample("Blink.Evp.Status.IsVerifiable",
                                      EmailVerificationRequestResult::kSuccess,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.Verify",
      EmailVerificationRequestResult::kTokenInvalidResponse, 1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(main_rfh())
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::kTokenInvalidResponse));
}

TEST_F(EmailVerificationRequestTest, FencedFrameRejected) {
  NavigateAndCommit(GURL("https://rp.example.com"));

  RenderFrameHost* fenced_frame =
      RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  ASSERT_TRUE(fenced_frame);

  GURL fenced_frame_url = GURL("https://fencedframe.com");
  std::unique_ptr<NavigationSimulator> navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(fenced_frame_url,
                                                   fenced_frame);
  navigation_simulator->Commit();
  fenced_frame = navigation_simulator->GetFinalRenderFrameHost();
  ASSERT_TRUE(fenced_frame);

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();

  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*fenced_frame));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(EmailVerificationRequestTest, CrossOriginFrameRejected) {
  NavigateAndCommit(GURL("https://rp.example.com"));

  RenderFrameHost* cross_origin_iframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://other-rp.com"),
          RenderFrameHostTester::For(main_rfh())
              ->AppendChild("cross_origin_iframe"));
  ASSERT_TRUE(cross_origin_iframe);

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();

  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*cross_origin_iframe));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(EmailVerificationRequestTest, SameOriginFrameAllowed) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  RenderFrameHost* same_origin_iframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://rp.example.com/iframe.html"),
          RenderFrameHostTester::For(main_rfh())
              ->AppendChild("same_origin_iframe"));
  ASSERT_TRUE(same_origin_iframe);

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  NiceMock<MockDnsRequest>* mock_dns_request_ = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager_ =
      mock_network_manager_ptr.get();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  NiceMock<MockIdpNetworkRequestManager>* mock_idp_network_manager_ =
      mock_idp_network_manager_ptr.get();

  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*same_origin_iframe));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");

  EXPECT_CALL(*mock_dns_request_,
              SendRequest("_email-verification.example.com", _))
      .WillOnce(WithArgs<1>([&](DnsRequest::DnsRequestCallback callback) {
        std::move(callback).Run(
            std::vector<std::string>{"iss=issuer.example.com"});
      }));

  EXPECT_CALL(*mock_network_manager_, FetchWellKnown(kIssuerUrl, _))
      .WillOnce(WithArgs<1>(
          [&](EmailVerifierNetworkRequestManager::FetchWellKnownCallback
                  callback) {
            EmailVerifierNetworkRequestManager::WellKnown well_known;
            std::move(callback).Run(
                FetchStatus{ParseStatus::kInvalidResponseError}, well_known);
          }));

  EXPECT_CALL(*mock_idp_network_manager_, FetchWellKnown(_, _))
      .WillOnce(WithArgs<1>(
          [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
            std::move(callback).Run(
                FetchStatus{ParseStatus::kHttpNotFoundError},
                IdpNetworkRequestManager::WellKnown());
          }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::
          kEmailVerificationWellKnownInvalidResponse,
      1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(same_origin_iframe)
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::
                           kEmailVerificationWellKnownInvalidResponse));
}

TEST_F(EmailVerificationRequestTest,
       SameOriginFrameNestedInCrossOriginFrameRejected) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://rp.example.com"));

  // Main Frame: https://rp.example.com
  // Subframe B: https://other-rp.com (cross-origin)
  RenderFrameHost* iframe_b =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://other-rp.com"),
          RenderFrameHostTester::For(main_rfh())->AppendChild("iframe_b"));
  ASSERT_TRUE(iframe_b);

  // Subframe A (nested inside B): https://rp.example.com (same-origin with main
  // frame)
  RenderFrameHost* iframe_a =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://rp.example.com"),
          RenderFrameHostTester::For(iframe_b)->AppendChild("iframe_a"));
  ASSERT_TRUE(iframe_a);

  auto mock_dns_request_ptr = std::make_unique<NiceMock<MockDnsRequest>>();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  auto mock_idp_network_manager_ptr =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();

  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr),
      std::move(mock_idp_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl&>(*iframe_a));

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";

  base::test::TestFuture<std::optional<EmailVerifier::Result>> future;
  email_verification_request_.CheckIfVerifiable(kEmail, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kRpOriginIsOpaque, 1);
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(iframe_a)
                   ->GetEmailVerificationRequestIssueCount(
                       EmailVerificationRequestResult::kRpOriginIsOpaque));
}

TEST(EmailVerificationRequestStaticTest, ValidEmail) {
  EXPECT_EQ(webid::GetDomainFromEmail("test@example.com"), "example.com");
}

TEST(EmailVerificationRequestStaticTest, ValidEmailWithSubdomain) {
  EXPECT_EQ(webid::GetDomainFromEmail("test@mail.example.com"),
            "mail.example.com");
}

TEST(EmailVerificationRequestStaticTest, EmptyEmail) {
  EXPECT_EQ(webid::GetDomainFromEmail(""), std::nullopt);
}

TEST(EmailVerificationRequestStaticTest, NoAtSign) {
  EXPECT_EQ(webid::GetDomainFromEmail("testexample.com"), std::nullopt);
}

TEST(EmailVerificationRequestStaticTest, NoDomain) {
  EXPECT_EQ(webid::GetDomainFromEmail("test@"), std::nullopt);
}

TEST(EmailVerificationRequestStaticTest, NoUsername) {
  EXPECT_EQ(webid::GetDomainFromEmail("@example.com"), std::nullopt);
}

TEST(EmailVerificationRequestStaticTest, NotADomain) {
  EXPECT_EQ(webid::GetDomainFromEmail("user@e x a m p l e"), std::nullopt);
}

TEST(EmailVerificationRequestStaticTest, MultipleAtSigns) {
  EXPECT_EQ(webid::GetDomainFromEmail("test@test@example.com"), "example.com");
}

}  // namespace content::webid
