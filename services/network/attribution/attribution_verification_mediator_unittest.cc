// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_verification_mediator.h"

#include <memory>
#include <set>
#include <string>

#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/attribution/attribution_test_utils.h"
#include "services/network/attribution/attribution_verification_mediator_metrics_recorder.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/trust_token_key_commitments.h"
#include "services/network/trust_tokens/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

using ::testing::IsEmpty;

using Message = AttributionVerificationMediator::Message;
using BlindedMessage = AttributionVerificationMediator::BlindedMessage;
using BlindedToken = AttributionVerificationMediator::BlindedToken;
using Token = AttributionVerificationMediator::Token;

class AttributionVerificationMediatorTest : public testing::Test {
 protected:
  void SetUp() override {
    key_commitment_getter_ = CreateTestTrustTokenKeyCommitments(
        /*key=*/example_verification_key_,
        /*protocol_version=*/example_protocol_version_,
        /*issuer_url=*/example_valid_request_url_);

    std::vector<std::unique_ptr<AttributionVerificationMediator::Cryptographer>>
        cryptographers;
    auto fake_cryptographer = std::make_unique<FakeCryptographer>();
    fake_cryptographer_ = fake_cryptographer.get();
    cryptographers.push_back(std::move(fake_cryptographer));

    mediator_ = std::make_unique<AttributionVerificationMediator>(
        key_commitment_getter_.get(), std::move(cryptographers),
        std::make_unique<AttributionVerificationMediatorMetricsRecorder>());
  }

  net::HttpRequestHeaders RunGetHeadersForVerificationWith(
      const GURL& url,
      std::vector<Message> messages) {
    base::RunLoop run_loop;

    net::HttpRequestHeaders headers;
    mediator_->GetHeadersForVerification(
        url, std::move(messages),
        base::BindLambdaForTesting(
            [&run_loop, &headers](net::HttpRequestHeaders h) {
              headers = std::move(h);
              run_loop.Quit();
            }));

    run_loop.Run();
    return headers;
  }

  void RecreateMediatorWithNCryptographers(size_t n) {
    fake_cryptographer_ = nullptr;

    std::vector<std::unique_ptr<AttributionVerificationMediator::Cryptographer>>
        cryptographers;
    for (size_t i = 0; i < n; ++i) {
      auto fake_cryptographer = std::make_unique<FakeCryptographer>();
      cryptographers.push_back(std::move(fake_cryptographer));
    }

    mediator_ = std::make_unique<AttributionVerificationMediator>(
        key_commitment_getter_.get(), std::move(cryptographers),
        std::make_unique<AttributionVerificationMediatorMetricsRecorder>());
  }

  void RunGetHeadersForVerificationWithValidParams() {
    RunGetHeadersForVerificationWith(
        /*url=*/example_valid_request_url_, /*messages=*/{"message"});
  }

  std::vector<Token> RunProcessVerificationToGetTokensWith(
      net::HttpResponseHeaders& response_headers) {
    base::RunLoop run_loop;

    std::vector<Token> tokens;
    mediator_->ProcessVerificationToGetTokens(
        response_headers,
        base::BindLambdaForTesting([&run_loop, &tokens](std::vector<Token> t) {
          tokens = std::move(t);

          run_loop.Quit();
        }));

    run_loop.Run();

    return tokens;
  }

  GURL example_valid_request_url_ =
      GURL("https://reporting-origin.example/test/path/#123");
  mojom::TrustTokenProtocolVersion example_protocol_version_ =
      mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb;
  std::string example_verification_key_ = "example-key";

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TrustTokenKeyCommitments> key_commitment_getter_;

  // We hold onto a raw ptr to configure the call expectations, the helper owns
  // the unique_ptr.
  raw_ptr<FakeCryptographer, DanglingUntriaged> fake_cryptographer_;
  std::unique_ptr<AttributionVerificationMediator> mediator_;

  base::HistogramTester histograms_;
};

TEST_F(AttributionVerificationMediatorTest,
       GetHeadersForVerification_HeadersReturned) {
  net::HttpRequestHeaders headers = RunGetHeadersForVerificationWith(
      /*url=*/example_valid_request_url_, /*messages=*/{"message"});

  std::string verification_header;
  headers.GetHeader("Sec-Attribution-Reporting-Private-State-Token",
                    &verification_header);
  std::vector<const std::string> verification_headers =
      DeserializeStructuredHeaderListOfStrings(verification_header);
  // Check that the message was blinded by the Cryptographer before being added
  ASSERT_EQ(verification_headers.size(), 1u);
  EXPECT_TRUE(
      FakeCryptographer::IsBlindMessage(verification_headers.at(0), "message"));

  std::string version_header;
  headers.GetHeader("Sec-Private-State-Token-Crypto-Version", &version_header);
  EXPECT_EQ(version_header,
            internal::ProtocolVersionToString(example_protocol_version_));

  EXPECT_TRUE(
      base::Contains(fake_cryptographer_->keys, example_verification_key_));

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      AttributionVerificationMediator::GetHeadersStatus::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       GetHeadersForVerification_NonSuitableIssuer) {
  net::HttpRequestHeaders headers = RunGetHeadersForVerificationWith(
      /*url=*/GURL("http://not-https-url.example/path"),
      /*messages=*/{"does-not-matter"});

  EXPECT_TRUE(headers.IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      AttributionVerificationMediator::GetHeadersStatus::
          kIssuerOriginNotSuitable,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       GetHeadersForVerification_NoIssuerReturned) {
  net::HttpRequestHeaders headers = RunGetHeadersForVerificationWith(
      /*url=*/GURL("https://not-registered-origin-url.example/path"),
      /*messages=*/{"does-not-matter"});

  EXPECT_TRUE(headers.IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      AttributionVerificationMediator::GetHeadersStatus::kIssuerNotRegistered,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       GetHeadersForVerification_CryptographerInitializationFails) {
  fake_cryptographer_->set_should_fail_initialize(true);

  net::HttpRequestHeaders headers = RunGetHeadersForVerificationWith(
      /*url=*/example_valid_request_url_, /*messages=*/{"does-not-matter"});

  EXPECT_TRUE(headers.IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      AttributionVerificationMediator::GetHeadersStatus::
          kUnableToInitializeCryptographer,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       GetHeadersForVerification_CryprographerAddKeyFails) {
  fake_cryptographer_->set_should_fail_add_key(true);

  net::HttpRequestHeaders headers = RunGetHeadersForVerificationWith(
      /*url=*/example_valid_request_url_, /*messages=*/{"does-not-matter"});

  EXPECT_TRUE(headers.IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      AttributionVerificationMediator::GetHeadersStatus::
          kUnableToAddKeysOnCryptographer,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       GetHeadersForVerification_CryptographerReturnsNoBlindMessage) {
  fake_cryptographer_->set_should_fail_begin_issuance(true);

  net::HttpRequestHeaders headers = RunGetHeadersForVerificationWith(
      /*url=*/example_valid_request_url_, /*messages=*/{"does-not-matter"});

  EXPECT_TRUE(headers.IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      AttributionVerificationMediator::GetHeadersStatus::kUnableToBlindMessage,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       ProcessVerificationToGetTokens_HeaderValueReturned) {
  RunGetHeadersForVerificationWith(
      /*url=*/example_valid_request_url_,
      /*messages=*/{"message"});

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");
  response_head->headers->AddHeader(
      "Sec-Attribution-Reporting-Private-State-Token",
      SerializeStructureHeaderListOfStrings({"blind-token"}));

  std::vector<Token> maybe_tokens =
      RunProcessVerificationToGetTokensWith(*response_head->headers.get());
  // Check that that the blind-token returned by the issuer has been formed in
  // a token by the Cryptographer.
  ASSERT_EQ(maybe_tokens.size(), 1u);
  EXPECT_TRUE(FakeCryptographer::IsToken(maybe_tokens.at(0), "blind-token"));

  // Check that the header has been removed after having been processed.
  EXPECT_FALSE(response_head->headers->HasHeader(
      "Sec-Attribution-Reporting-Private-State-Token"));

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.ProcessVerificationStatus",
      AttributionVerificationMediator::ProcessVerificationStatus::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       ProcessVerificationToGetTokens_HeaderWithMultipleTokensReturned) {
  RecreateMediatorWithNCryptographers(2);
  RunGetHeadersForVerificationWith(
      /*url=*/example_valid_request_url_,
      /*messages=*/{"message-1", "message-2"});

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");
  response_head->headers->AddHeader(
      "Sec-Attribution-Reporting-Private-State-Token",
      SerializeStructureHeaderListOfStrings(
          {"blind-token-1", "blind-token-2"}));

  std::vector<Token> tokens =
      RunProcessVerificationToGetTokensWith(*response_head->headers.get());
  // Check that that the blind-token returned by the issuer has been formed in
  // a token by the Cryptographer.
  ASSERT_EQ(tokens.size(), 2u);
  EXPECT_TRUE(FakeCryptographer::IsToken(tokens.at(0), "blind-token-1"));
  EXPECT_TRUE(FakeCryptographer::IsToken(tokens.at(1), "blind-token-2"));

  // Check that the header has been removed after having been processed.
  EXPECT_FALSE(response_head->headers->HasHeader(
      "Sec-Attribution-Reporting-Private-State-Token"));

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.ProcessVerificationStatus",
      AttributionVerificationMediator::ProcessVerificationStatus::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       ProcessVerificationToGetTokens_ResponseHeaderIsMissing) {
  RecreateMediatorWithNCryptographers(1);
  RunGetHeadersForVerificationWithValidParams();

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");

  std::vector<Token> tokens =
      RunProcessVerificationToGetTokensWith(*response_head->headers.get());

  EXPECT_THAT(tokens, IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.ProcessVerificationStatus",
      AttributionVerificationMediator::ProcessVerificationStatus::
          kNoSignatureReceivedFromIssuer,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       ProcessVerificationToGetTokens_ResponseHeaderIsInvalid) {
  std::vector<std::string> invalid_headers({
      SerializeStructureHeaderListOfStrings({}),    // empty list
      SerializeStructureHeaderListOfStrings({""}),  // list with empty string
      "",                                           // empty string
      ",,,",                                        // empty elements
      "?1",                                         // boolean
      "4.5",                                        // decimal
      "44",                                         // integer
      "valid, ?1",                                  // 1 valid & 1 invalid
      "(\"valid\")",                                // inner-list
      SerializeStructureHeaderListOfStrings(
          {"valid", ""}),  // 1 valid & 1 empty
  });

  for (const std::string& header_value : invalid_headers) {
    base::HistogramTester histograms;
    RecreateMediatorWithNCryptographers(1);
    RunGetHeadersForVerificationWithValidParams();

    auto response_head = mojom::URLResponseHead::New();
    response_head->headers = net::HttpResponseHeaders::TryToCreate("");
    response_head->headers->AddHeader(
        "Sec-Attribution-Reporting-Private-State-Token", header_value);

    std::vector<Token> tokens =
        RunProcessVerificationToGetTokensWith(*response_head->headers.get());

    EXPECT_THAT(tokens, IsEmpty()) << header_value;

    histograms.ExpectUniqueSample(
        "Conversions.ReportVerification.ProcessVerificationStatus",
        AttributionVerificationMediator::ProcessVerificationStatus::
            kBadSignaturesHeaderReceivedFromIssuer,
        /*expected_bucket_count=*/1);
  }
}

TEST_F(AttributionVerificationMediatorTest,
       ProcessVerificationToGetTokens_ResponseContainsFewerTokensThanMessages) {
  RecreateMediatorWithNCryptographers(2);
  RunGetHeadersForVerificationWith(
      /*url=*/example_valid_request_url_,
      /*messages=*/{"message-1", "message-2"});

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");
  response_head->headers->AddHeader(
      "Sec-Attribution-Reporting-Private-State-Token",
      SerializeStructureHeaderListOfStrings({"blind-token-1"}));

  std::vector<Token> tokens =
      RunProcessVerificationToGetTokensWith(*response_head->headers.get());

  // Should process the token received successfully
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_TRUE(FakeCryptographer::IsToken(tokens.at(0), "blind-token-1"));

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.ProcessVerificationStatus",
      AttributionVerificationMediator::ProcessVerificationStatus::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       ProcessVerificationToGetTokens_ResponseContainsTooManyTokens) {
  RunGetHeadersForVerificationWithValidParams();

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");
  response_head->headers->AddHeader(
      "Sec-Attribution-Reporting-Private-State-Token",
      SerializeStructureHeaderListOfStrings(
          {"blind-token-1", "blind-token-2"}));

  std::vector<Token> tokens =
      RunProcessVerificationToGetTokensWith(*response_head->headers.get());
  EXPECT_THAT(tokens, IsEmpty());
  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.ProcessVerificationStatus",
      AttributionVerificationMediator::ProcessVerificationStatus::
          kTooManySignaturesReceivedFromIssuer,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionVerificationMediatorTest,
       ProcessVerificationToGetTokens_CryptographerReturnsNoToken) {
  RunGetHeadersForVerificationWithValidParams();

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");
  response_head->headers->AddHeader(
      "Sec-Attribution-Reporting-Private-State-Token",
      SerializeStructureHeaderListOfStrings({"blind-token"}));

  fake_cryptographer_->set_should_fail_confirm_issuance(true);

  EXPECT_THAT(
      RunProcessVerificationToGetTokensWith(*response_head->headers.get()),
      IsEmpty());

  // The header should have been removed even if not able to get a
  // token from it.
  EXPECT_FALSE(response_head->headers->HasHeader(
      "Sec-Attribution-Reporting-Private-State-Token"));

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.ProcessVerificationStatus",
      AttributionVerificationMediator::ProcessVerificationStatus::
          kUnableToUnblindSignature,
      /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace network
