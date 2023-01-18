// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_request_helper.h"

#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/attribution/attribution_attestation_mediator.h"
#include "services/network/attribution/attribution_test_utils.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/trust_token_key_commitments.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

class AttributionRequestHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    trust_token_key_commitments_ = CreateTestTrustTokenKeyCommitments(
        /*key=*/"any-key",
        /*protocol_version=*/mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb,
        /*issuer_url=*/example_valid_request_url_);

    auto fake_cryptographer = std::make_unique<FakeCryptographer>();

    auto mediator = std::make_unique<AttributionAttestationMediator>(
        trust_token_key_commitments_.get(), std::move(fake_cryptographer));
    net::HttpRequestHeaders request_headers;
    request_headers.SetHeader("Attribution-Reporting-Eligible", "trigger");
    helper_ = AttributionRequestHelper::CreateForTesting(request_headers,
                                                         std::move(mediator));

    context_ = net::CreateTestURLRequestContextBuilder()->Build();
  }

  std::unique_ptr<net::URLRequest> CreateTestUrlRequestFrom(
      const GURL& to_url,
      const GURL& from_url) {
    auto request = CreateTestUrlRequest(to_url);
    request->set_isolation_info(net::IsolationInfo::CreateForInternalRequest(
        url::Origin::Create(from_url)));

    return request;
  }

  std::unique_ptr<net::URLRequest> CreateTestUrlRequest(const GURL& to_url) {
    auto request =
        context_->CreateRequest(to_url, net::RequestPriority::DEFAULT_PRIORITY,
                                &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);

    return request;
  }

  void RunBeginWith(net::URLRequest& request) {
    base::RunLoop run_loop;
    helper_->Begin(request, run_loop.QuitClosure());
    run_loop.Run();
  }

  void RunFinalizeWith(mojom::URLResponseHead& response) {
    base::RunLoop run_loop;
    helper_->Finalize(response, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AttributionRequestHelper> helper_;
  GURL example_valid_request_url_ =
      GURL("https://reporting-origin.example/test/path/#123");

 private:
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<TrustTokenKeyCommitments> trust_token_key_commitments_;
  net::TestDelegate delegate_;
};

TEST_F(AttributionRequestHelperTest, Begin_HeadersAdded) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/example_valid_request_url_,
      /*from_url=*/GURL("https://origin.example/path/123#foo"));

  RunBeginWith(*request);

  // Should have added the protocol version header
  EXPECT_TRUE(request->extra_request_headers().HasHeader(
      kTrustTokensSecTrustTokenVersionHeader));

  // Should have added the blind message header
  ASSERT_TRUE(request->extra_request_headers().HasHeader(
      AttributionAttestationMediator::kTriggerAttestationHeader));

  // The generated message should be composed of:
  // a. The origin from which the request was made which corresponds to the
  //    attribution destination origin.
  // b. A generated uuid that represents the id of a future aggregatable report.
  std::string blind_message_header;
  request->extra_request_headers().GetHeader(
      AttributionAttestationMediator::kTriggerAttestationHeader,
      &blind_message_header);
  std::string message = FakeCryptographer::UnblindMessage(blind_message_header);
  std::string expected_origin = "https://origin.example";

  EXPECT_TRUE(base::EndsWith(message, expected_origin));
  std::string potential_id =
      message.substr(0, message.length() - expected_origin.length());
  EXPECT_TRUE(base::GUID::ParseLowercase(potential_id).is_valid());
}

TEST_F(AttributionRequestHelperTest, Begin_NoDestinationOnTheRequest) {
  std::unique_ptr<net::URLRequest> request =
      CreateTestUrlRequest(/*to_url=*/example_valid_request_url_);

  RunBeginWith(*request);

  EXPECT_TRUE(request->extra_request_headers().IsEmpty());
}

TEST_F(AttributionRequestHelperTest, Begin_NoHeadersReturned) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/GURL("https://not-registered-origin.example/path/123#foo"),
      /*from_url=*/GURL("https://origin.example/path/123#foo"));

  RunBeginWith(*request);

  EXPECT_TRUE(request->extra_request_headers().IsEmpty());
}

TEST_F(AttributionRequestHelperTest, Finalize_AttestationTokenAdded) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/example_valid_request_url_,
      /*from_url=*/GURL("https://origin.example/path/123#foo"));

  RunBeginWith(*request);

  mojom::URLResponseHeadPtr response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");
  response_head->headers->AddHeader(
      AttributionAttestationMediator::kTriggerAttestationHeader,
      "blind-signature");

  RunFinalizeWith(*response_head);

  // TODO(crbug.com/1405832): Update to test that the attestation token has been
  // been added to the response.
}

TEST_F(AttributionRequestHelperTest, Finalize_NotBegun) {
  mojom::URLResponseHeadPtr response_head = mojom::URLResponseHead::New();
  response_head->headers = net::HttpResponseHeaders::TryToCreate("");
  response_head->headers->AddHeader(
      AttributionAttestationMediator::kTriggerAttestationHeader,
      "blind-signature");

  RunFinalizeWith(*response_head);

  // TODO(crbug.com/1405832): Update to test that the attestation token has not
  // been been added to the response.
}

struct CreateIfNeededTestCase {
  std::string header_name;
  std::string header_value;
  bool expect_instance_to_be_created;
};
TEST_F(AttributionRequestHelperTest, CreateIfNeeded) {
  CreateIfNeededTestCase test_cases[] = {
      {"Some-Random-Header", "dont-care", false},
      {"Attribution-Reporting-Eligible", "source", false},
      {"Attribution-Reporting-Eligible", "source,trigger", true},
      {"Attribution-Reporting-Eligible", "source,Trigger", false},
  };

  auto key_commitment = CreateTestTrustTokenKeyCommitments(
      "dont-care", mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb,
      example_valid_request_url_);

  for (CreateIfNeededTestCase test_case : test_cases) {
    net::HttpRequestHeaders request_headers;
    request_headers.SetHeader(test_case.header_name, test_case.header_value);

    auto instance = AttributionRequestHelper::CreateIfNeeded(
        request_headers, key_commitment.get());
    bool instance_created = !!instance;
    EXPECT_EQ(instance_created, test_case.expect_instance_to_be_created);
  }
}

}  // namespace network
