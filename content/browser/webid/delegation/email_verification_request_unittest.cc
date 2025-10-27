// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verification_request.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/dns_request.h"
#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"
#include "content/browser/webid/delegation/jwt_signer.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::WithArgs;

namespace content::webid {

// Mock DnsRequest for testing
class MockDnsRequest : public DnsRequest {
 public:
  explicit MockDnsRequest(network::mojom::NetworkContext* network_context)
      : DnsRequest(base::BindRepeating(
            [](network::mojom::NetworkContext* network_context) {
              return network_context;
            },
            network_context)) {}
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
};

class EmailVerificationRequestTest : public RenderViewHostTestHarness {
 public:
  EmailVerificationRequestTest() = default;

 protected:
  network::TestNetworkContext mock_network_context_;
  const url::Origin kRpOrigin =
      url::Origin::Create(GURL("https://rp.example.com"));
};

TEST_F(EmailVerificationRequestTest, SuccessfulVerification) {
  auto mock_dns_request_ptr =
      std::make_unique<NiceMock<MockDnsRequest>>(&mock_network_context_);
  NiceMock<MockDnsRequest>* mock_dns_request_ = mock_dns_request_ptr.get();
  auto mock_network_manager_ptr =
      std::make_unique<NiceMock<MockEmailVerifierNetworkRequestManager>>();
  NiceMock<MockEmailVerifierNetworkRequestManager>* mock_network_manager_ =
      mock_network_manager_ptr.get();
  webid::EmailVerificationRequest email_verification_request_(
      std::move(mock_network_manager_ptr), std::move(mock_dns_request_ptr),
      static_cast<RenderFrameHostImpl*>(main_rfh())->GetSafeRef());

  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const std::string kIssuer = "issuer.example.com";
  const GURL kIssuerUrl = GURL("https://issuer.example.com");
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");

  const std::string kToken = "test_token";

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
            std::move(callback).Run(FetchStatus{ParseStatus::kSuccess},
                                    well_known);
          }));

  EXPECT_CALL(*mock_network_manager_, SendTokenRequest(kIssuanceEndpoint, _, _))
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
                      main_rfh()->GetLastCommittedOrigin().Serialize());
            EXPECT_EQ(payload->email, kEmail);

            sdjwt::SdJwt token;
            sdjwt::Header h;
            h.typ = "web-identity+sd-jwt";
            h.alg = "RS256";
            sdjwt::Payload p;
            p.iss = url::Origin::Create(kIssuerUrl).Serialize();
            p.email = kEmail;
            sdjwt::ConfirmationKey cnf;
            cnf.jwk = *(header->jwk);
            p.cnf = cnf;

            auto key = crypto::keypair::PrivateKey::GenerateRsa2048();
            auto signer = sdjwt::CreateJwtSigner(key);

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

  base::test::TestFuture<std::optional<std::string>> future;
  std::string nonce = kNonce;
  email_verification_request_.Send(kEmail, nonce, future.GetCallback());
  std::optional<std::string> token = future.Get();
  EXPECT_TRUE(token.has_value());

  auto sd_jwt_kb = sdjwt::SdJwtKb::Parse(*token);
  EXPECT_TRUE(sd_jwt_kb);

  auto kb_jwt_json = sdjwt::Jwt::Parse(sd_jwt_kb->kb_jwt.Serialize().value());
  EXPECT_TRUE(kb_jwt_json);
  auto kb_jwt = sdjwt::Jwt::From(*kb_jwt_json);
  EXPECT_TRUE(kb_jwt);
  auto kb_payload = sdjwt::Payload::From(*base::JSONReader::ReadDict(
      kb_jwt->payload.value(), base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  EXPECT_TRUE(kb_payload);
  EXPECT_EQ(kb_payload->aud, main_rfh()->GetLastCommittedOrigin().Serialize());
  EXPECT_EQ(kb_payload->nonce, kNonce);
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
