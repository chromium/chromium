// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_attestation_mediator.h"

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
#include "services/network/attribution/attribution_attestation_mediator_metrics_recorder.h"
#include "services/network/attribution/attribution_test_utils.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/trust_token_key_commitments.h"
#include "services/network/trust_tokens/types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

class AttributionAttestationMediatorTest : public testing::Test {
 protected:
  void SetUp() override {
    key_commitment_getter_ = CreateTestTrustTokenKeyCommitments(
        /*key=*/example_verification_key_,
        /*protocol_version=*/example_protocol_version_,
        /*issuer_url=*/example_valid_request_url_);

    auto fake_cryptographer = std::make_unique<FakeCryptographer>();
    fake_cryptographer_ = fake_cryptographer.get();

    mediator_ = std::make_unique<AttributionAttestationMediator>(
        key_commitment_getter_.get(), std::move(fake_cryptographer),
        std::make_unique<AttributionAttestationMediatorMetricsRecorder>());
  }

  net::HttpRequestHeaders RunGetHeadersForAttestationWith(
      const GURL& url,
      const std::string& message) {
    base::RunLoop run_loop;

    net::HttpRequestHeaders headers;
    mediator_->GetHeadersForAttestation(
        url, message,
        base::BindLambdaForTesting(
            [&run_loop, &headers](net::HttpRequestHeaders h) {
              headers = h;
              run_loop.Quit();
            }));

    run_loop.Run();
    return headers;
  }

  void RunGetHeadersForAttestationWithValidParams() {
    RunGetHeadersForAttestationWith(
        /*url=*/example_valid_request_url_, /*message=*/"message");
  }

  absl::optional<std::string> RunProcessAttestationToGetTokenWith(
      net::HttpResponseHeaders& response_headers) {
    base::RunLoop run_loop;

    absl::optional<std::string> maybe_token;
    mediator_->ProcessAttestationToGetToken(
        response_headers,
        base::BindLambdaForTesting(
            [&run_loop, &maybe_token](absl::optional<std::string> m) {
              maybe_token = m;

              run_loop.Quit();
            }));

    run_loop.Run();

    return maybe_token;
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
  base::raw_ptr<FakeCryptographer> fake_cryptographer_;
  std::unique_ptr<AttributionAttestationMediator> mediator_;

  base::HistogramTester histograms_;
};

TEST_F(AttributionAttestationMediatorTest,
       GetHeadersForAttestation_HeadersReturned) {
  net::HttpRequestHeaders headers = RunGetHeadersForAttestationWith(
      /*url=*/example_valid_request_url_, /*message=*/"message");

  std::string attestation_header;
  headers.GetHeader("Sec-Attribution-Reporting-Private-State-Token",
                    &attestation_header);
  // Check that the message was blinded by the Cryptographer before being added
  // as an attestation header.
  EXPECT_TRUE(FakeCryptographer::IsBlindMessage(attestation_header, "message"));

  std::string version_header;
  headers.GetHeader("Sec-Trust-Token-Version", &version_header);
  EXPECT_EQ(version_header,
            internal::ProtocolVersionToString(example_protocol_version_));

  EXPECT_TRUE(
      base::Contains(fake_cryptographer_->keys, example_verification_key_));

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      AttributionAttestationMediator::GetHeadersStatus::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionAttestationMediatorTest,
       GetHeadersForAttestation_NonSuitableIssuer) {
  net::HttpRequestHeaders headers = RunGetHeadersForAttestationWith(
      /*url=*/GURL("http://not-https-url.example/path"),
      /*message=*/"does-not-matter");

  EXPECT_TRUE(headers.IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      AttributionAttestationMediator::GetHeadersStatus::
          kIssuerOriginNotSuitable,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionAttestationMediatorTest,
       GetHeadersForAttestation_NoIssuerReturned) {
  net::HttpRequestHeaders headers = RunGetHeadersForAttestationWith(
      /*url=*/GURL("https://not-registered-origin-url.example/path"),
      /*message=*/"does-not-matter");

  EXPECT_TRUE(headers.IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      AttributionAttestationMediator::GetHeadersStatus::kIssuerNotRegistered,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionAttestationMediatorTest,
       GetHeadersForAttestation_CryptographerInitializationFails) {
  fake_cryptographer_->set_should_fail_initialize(true);

  net::HttpRequestHeaders headers = RunGetHeadersForAttestationWith(
      /*url=*/example_valid_request_url_, /*message=*/"does-not-matter");

  EXPECT_TRUE(headers.IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      AttributionAttestationMediator::GetHeadersStatus::
          kUnableToInitializeCryptographer,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionAttestationMediatorTest,
       GetHeadersForAttestation_CryprographerAddKeyFails) {
  fake_cryptographer_->set_should_fail_add_key(true);

  net::HttpRequestHeaders headers = RunGetHeadersForAttestationWith(
      /*url=*/example_valid_request_url_, /*message=*/"does-not-matter");

  EXPECT_TRUE(headers.IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      AttributionAttestationMediator::GetHeadersStatus::
          kUnableToAddKeysOnCryptographer,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionAttestationMediatorTest,
       GetHeadersForAttestation_CryptographerReturnsNoBlindMessage) {
  fake_cryptographer_->set_should_fail_begin_issuance(true);

  net::HttpRequestHeaders headers = RunGetHeadersForAttestationWith(
      /*url=*/example_valid_request_url_, /*message=*/"does-not-matter");

  EXPECT_TRUE(headers.IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      AttributionAttestationMediator::GetHeadersStatus::kUnableToBlindMessage,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionAttestationMediatorTest,
       ProcessAttestationToGetToken_HeaderValueReturned) {
  RunGetHeadersForAttestationWithValidParams();

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");
  response_head->headers->AddHeader(
      "Sec-Attribution-Reporting-Private-State-Token", "blind-token");

  absl::optional<std::string> maybe_token =
      RunProcessAttestationToGetTokenWith(*response_head->headers.get());
  // Check that that the blind-token returned by the issuer has been formed in
  // a token by the Cryptographer.
  EXPECT_TRUE(FakeCryptographer::IsToken(maybe_token.value(), "blind-token"));

  // Check that the header has been removed after beein processed.
  EXPECT_FALSE(response_head->headers->HasHeader(
      "Sec-Attribution-Reporting-Private-State-Token"));

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.ProcessAttestationStatus",
      AttributionAttestationMediator::ProcessAttestationStatus::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionAttestationMediatorTest,
       ProcessAttestationToGetToken_ResponseHeaderIsMissing) {
  RunGetHeadersForAttestationWithValidParams();

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");

  absl::optional<std::string> maybe_token =
      RunProcessAttestationToGetTokenWith(*response_head->headers.get());

  EXPECT_FALSE(maybe_token.has_value());

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.ProcessAttestationStatus",
      AttributionAttestationMediator::ProcessAttestationStatus::
          kNoSignatureReceivedFromIssuer,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionAttestationMediatorTest,
       ProcessAttestationToGetToken_CryptographerReturnsNoToken) {
  RunGetHeadersForAttestationWithValidParams();

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");
  response_head->headers->AddHeader(
      "Sec-Attribution-Reporting-Private-State-Token", "blind-token");

  fake_cryptographer_->set_should_fail_confirm_issuance(true);

  absl::optional<std::string> maybe_token =
      RunProcessAttestationToGetTokenWith(*response_head->headers.get());

  EXPECT_FALSE(maybe_token.has_value());

  // The header should have been removed even if not able to get a
  // token from it.
  EXPECT_FALSE(response_head->headers->HasHeader(
      "Sec-Attribution-Reporting-Private-State-Token"));

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.ProcessAttestationStatus",
      AttributionAttestationMediator::ProcessAttestationStatus::
          kUnableToUnblindSignature,
      /*expected_bucket_count=*/1);
}

}  // namespace network
