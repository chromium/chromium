// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_request_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "net/http/structured_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/attribution/attribution_test_utils.h"
#include "services/network/attribution/attribution_verification_mediator.h"
#include "services/network/attribution/attribution_verification_mediator_metrics_recorder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/trust_token_key_commitments.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
namespace {

using ::network::mojom::AttributionReportingEligibility;
using ::testing::IsEmpty;

constexpr char kAttributionReportingEligible[] =
    "Attribution-Reporting-Eligible";

class AttributionRequestHelperTest : public testing::Test {
 protected:
  static constexpr char kTestBlindSignature[] = "blind-signature";
  enum WithVerificationHeader {
    kYes,
    kNo,
  };

  void SetUp() override {
    trust_token_key_commitments_ = CreateTestTrustTokenKeyCommitments(
        /*key=*/"any-key",
        /*protocol_version=*/mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb,
        /*issuer_url=*/example_valid_request_url_);
    helper_ = AttributionRequestHelper::CreateForTesting(
        AttributionReportingEligibility::kTrigger,
        /*create_mediator=*/base::BindRepeating(
            &CreateTestVerificationMediator,
            trust_token_key_commitments_.get()));

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

  net::RedirectInfo CreateRedirectInfo(const net::URLRequest& request,
                                       const GURL& to_url) {
    // We only care about the `new_location`, other properties are set to look
    // valid but are of no importance to the tests using the helper method.
    return net::RedirectInfo::ComputeRedirectInfo(
        /*original_method=*/request.method(), /*original_url=*/request.url(),
        /*original_site_for_cookies=*/request.site_for_cookies(),
        /*original_first_party_url_policy=*/
        net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT,
        /*original_referrer_policy=*/request.referrer_policy(),
        /*original_referrer=*/request.referrer(),
        /*http_status_code=*/net::HTTP_FOUND,
        /*new_location=*/to_url, /*referrer_policy_header=*/absl::nullopt,
        /*insecure_scheme_was_upgraded=*/false, /*copy_fragment=*/false,
        /*is_signed_exchange_fallback_redirect=*/false);
  }

  mojom::URLResponseHeadPtr CreateResponse(WithVerificationHeader with) {
    auto response = mojom::URLResponseHead::New();
    response->response_time = base::Time::Now();
    response->headers = net::HttpResponseHeaders::TryToCreate("");
    if (with == WithVerificationHeader::kYes) {
      response->headers->AddHeader(
          AttributionVerificationMediator::kReportVerificationHeader,
          SerializeStructureHeaderListOfStrings(std::vector<std::string>(
              AttributionRequestHelper::kVerificationTokensPerTrigger,
              kTestBlindSignature)));
    }

    return response;
  }

  void RunBeginWith(net::URLRequest& request) {
    base::RunLoop run_loop;
    helper_->Begin(request, run_loop.QuitClosure());
    run_loop.Run();
  }

  void RunRedirectWith(net::URLRequest& request,
                       mojom::URLResponseHeadPtr response,
                       const ::net::RedirectInfo& redirect_info,
                       bool expect_trigger_verification) {
    base::RunLoop run_loop;
    auto expected_response_time = response->response_time;
    helper_->OnReceiveRedirect(
        request, std::move(response), redirect_info,
        base::BindLambdaForTesting([&](mojom::URLResponseHeadPtr response) {
          EXPECT_EQ(expected_response_time, response->response_time);
          CheckVerifications(response->trigger_verifications,
                             expect_trigger_verification);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void RunFinalizeWith(mojom::URLResponseHead& response,
                       bool expect_trigger_verification) {
    base::RunLoop run_loop;
    helper_->Finalize(response, run_loop.QuitClosure());
    run_loop.Run();

    CheckVerifications(response.trigger_verifications,
                       expect_trigger_verification);
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histograms_;
  std::unique_ptr<AttributionRequestHelper> helper_;
  GURL example_valid_request_url_ =
      GURL("https://reporting-origin.example/test/path/#123");
  GURL example_not_registered_url =
      GURL("https://not-registered-origin.example/path/123#foo");

 private:
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<TrustTokenKeyCommitments> trust_token_key_commitments_;
  net::TestDelegate delegate_;

  void CheckVerifications(const std::vector<TriggerVerification>& verifications,
                          bool expected_to_have_value) {
    if (expected_to_have_value) {
      EXPECT_EQ(verifications.size(),
                AttributionRequestHelper::kVerificationTokensPerTrigger);
      for (const auto& verification : verifications) {
        EXPECT_TRUE(FakeCryptographer::IsToken(verification.token(),
                                               kTestBlindSignature));
      }
    } else {
      ASSERT_THAT(verifications, IsEmpty());
    }
  }
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
      AttributionVerificationMediator::kReportVerificationHeader));

  std::string blind_messages_header;
  request->extra_request_headers().GetHeader(
      AttributionVerificationMediator::kReportVerificationHeader,
      &blind_messages_header);
  std::vector<const std::string> blinded_messages =
      DeserializeStructuredHeaderListOfStrings(blind_messages_header);
  std::string expected_origin = "https://origin.example";
  for (const auto& blinded_message : blinded_messages) {
    std::string message = FakeCryptographer::UnblindMessage(blinded_message);

    // Each generated message should be composed of:
    // a. The origin from which the request was made which corresponds to the
    //    attribution destination origin.
    // b. A generated uuid that represents the id of a future aggregatable
    // report.
    EXPECT_TRUE(base::EndsWith(message, expected_origin));
    std::string potential_id =
        message.substr(0, message.length() - expected_origin.length());
    EXPECT_TRUE(base::Uuid::ParseLowercase(potential_id).is_valid());
  }

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.DestinationOriginStatus",
      AttributionRequestHelper::DestinationOriginStatus::kValid,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionRequestHelperTest, Begin_NoDestinationOnTheRequest) {
  std::unique_ptr<net::URLRequest> request =
      CreateTestUrlRequest(/*to_url=*/example_valid_request_url_);

  RunBeginWith(*request);

  EXPECT_TRUE(request->extra_request_headers().IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.DestinationOriginStatus",
      AttributionRequestHelper::DestinationOriginStatus::kMissing,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionRequestHelperTest, Begin_NoSuitableDestinationOnTheRequest) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/example_valid_request_url_,
      /*from_url=*/GURL("http://origin.example/path/123#foo"));

  RunBeginWith(*request);

  EXPECT_TRUE(request->extra_request_headers().IsEmpty());

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.DestinationOriginStatus",
      AttributionRequestHelper::DestinationOriginStatus::kNonSuitable,
      /*expected_bucket_count=*/1);
}

TEST_F(AttributionRequestHelperTest, Begin_NoHeadersReturned) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/example_not_registered_url,
      /*from_url=*/GURL("https://origin.example/path/123#foo"));

  RunBeginWith(*request);

  EXPECT_TRUE(request->extra_request_headers().IsEmpty());
}

// Should handle multiple successful redirections were headers are added and
// responses parsed to return multiple trigger_verifications.
TEST_F(AttributionRequestHelperTest, Redirect_Headers_Headers_Headers) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/example_valid_request_url_,
      /*from_url=*/GURL("https://origin.example/path/123#foo"));

  RunBeginWith(*request);
  std::string first_request_header;
  request->extra_request_headers().GetHeader(
      AttributionVerificationMediator::kReportVerificationHeader,
      &first_request_header);

  RunRedirectWith(
      *request, CreateResponse(WithVerificationHeader::kYes),
      CreateRedirectInfo(*request, /*to_url=*/example_valid_request_url_),
      /*expect_trigger_verification=*/true);
  std::string second_request_header;
  request->extra_request_headers().GetHeader(
      AttributionVerificationMediator::kReportVerificationHeader,
      &second_request_header);
  EXPECT_NE(first_request_header, second_request_header);

  RunRedirectWith(
      *request, CreateResponse(WithVerificationHeader::kYes),
      CreateRedirectInfo(*request, /*to_url=*/example_valid_request_url_),
      /*expect_trigger_verification=*/true);
  std::string third_request_header;
  request->extra_request_headers().GetHeader(
      AttributionVerificationMediator::kReportVerificationHeader,
      &third_request_header);
  EXPECT_NE(second_request_header, third_request_header);

  RunFinalizeWith(*CreateResponse(WithVerificationHeader::kYes),
                  /*expect_trigger_verification=*/true);
}

// Should be able to have a redirect with verification following one to an
// origin that was not registered as an issuer.
TEST_F(AttributionRequestHelperTest, Redirect_Headers_NoHeaders_Headers) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/example_valid_request_url_,
      /*from_url=*/GURL("https://origin.example/path/123#foo"));

  RunBeginWith(*request);

  RunRedirectWith(*request,
                  /*response=*/CreateResponse(WithVerificationHeader::kYes),
                  /*redirect_info=*/
                  CreateRedirectInfo(*request, example_not_registered_url),
                  /*expect_trigger_verification=*/true);
  RunRedirectWith(*request,
                  /*response=*/CreateResponse(WithVerificationHeader::kNo),
                  /*redirect_info=*/
                  CreateRedirectInfo(*request, example_valid_request_url_),
                  /*expect_trigger_verification=*/false);

  RunFinalizeWith(*CreateResponse(WithVerificationHeader::kYes),
                  /*expect_trigger_verification=*/true);
}

// Should support verifying a redirection response even if the initial request
// did not need verification.
TEST_F(AttributionRequestHelperTest, Redirect_NoHeaders_Headers) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/example_not_registered_url,
      /*from_url=*/GURL("https://origin.example/path/123#foo"));

  RunBeginWith(*request);

  ASSERT_FALSE(request->extra_request_headers().HasHeader(
      AttributionVerificationMediator::kReportVerificationHeader));

  RunRedirectWith(*request, CreateResponse(WithVerificationHeader::kNo),
                  CreateRedirectInfo(*request, example_valid_request_url_),
                  /*expect_trigger_verification=*/false);

  // Should add the verification headers even if no headers were added on the
  // first request.
  ASSERT_TRUE(request->extra_request_headers().HasHeader(
      AttributionVerificationMediator::kReportVerificationHeader));

  RunFinalizeWith(*CreateResponse(WithVerificationHeader::kYes),
                  /*expect_trigger_verification=*/true);
}

// Should avoid leaking verification headers to redirection requests when an
// initial request needed verification but the redirection request does not.
TEST_F(AttributionRequestHelperTest, Redirect_Headers_NoHeaders) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/example_valid_request_url_,
      /*from_url=*/GURL("https://origin.example/path/123#foo"));

  RunBeginWith(*request);
  ASSERT_TRUE(request->extra_request_headers().HasHeader(
      AttributionVerificationMediator::kReportVerificationHeader));

  RunRedirectWith(*request,
                  /*response=*/CreateResponse(WithVerificationHeader::kYes),
                  /*redirect_info=*/
                  CreateRedirectInfo(*request, example_not_registered_url),
                  /*expect_trigger_verification=*/true);

  // Should have removed the headers added on the first request
  ASSERT_FALSE(request->extra_request_headers().HasHeader(
      AttributionVerificationMediator::kReportVerificationHeader));

  RunFinalizeWith(*CreateResponse(WithVerificationHeader::kNo),
                  /*expect_trigger_verification=*/false);
}

TEST_F(AttributionRequestHelperTest, Redirect_NoDestinationOnTheRequest) {
  std::unique_ptr<net::URLRequest> request =
      CreateTestUrlRequest(/*to_url=*/example_valid_request_url_);

  RunBeginWith(*request);

  RunRedirectWith(*request,
                  /*response=*/CreateResponse(WithVerificationHeader::kYes),
                  /*redirect_info=*/
                  CreateRedirectInfo(*request, example_not_registered_url),
                  /*expect_trigger_verification=*/false);

  EXPECT_TRUE(request->extra_request_headers().IsEmpty());
}

TEST_F(AttributionRequestHelperTest, Finalize_VerificationTokenAdded) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/example_valid_request_url_,
      /*from_url=*/GURL("https://origin.example/path/123#foo"));

  RunBeginWith(*request);

  RunFinalizeWith(*CreateResponse(WithVerificationHeader::kYes),
                  /*expect_trigger_verification=*/true);
}

TEST_F(AttributionRequestHelperTest, Finalize_NotBegun) {
  // We add verification header on the response, they should be ignored as the
  // operation had not been started.
  RunFinalizeWith(*CreateResponse(WithVerificationHeader::kYes),
                  /*expect_trigger_verification=*/false);
}

TEST_F(AttributionRequestHelperTest, CreateIfNeeded) {
  const struct {
    AttributionReportingEligibility eligibility;
    bool expect_instance_to_be_created;
  } kTestCases[] = {
      {AttributionReportingEligibility::kUnset, false},
      {AttributionReportingEligibility::kEmpty, false},
      {AttributionReportingEligibility::kEventSource, false},
      {AttributionReportingEligibility::kNavigationSource, false},
      {AttributionReportingEligibility::kTrigger, true},
      {AttributionReportingEligibility::kEventSourceOrTrigger, true},
  };

  auto key_commitment = CreateTestTrustTokenKeyCommitments(
      "dont-care", mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb,
      example_valid_request_url_);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.eligibility);

    // If the feature is disabled, the helper should never be created.
    EXPECT_FALSE(AttributionRequestHelper::CreateIfNeeded(
        test_case.eligibility, key_commitment.get()));

    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        network::features::kAttributionReportingReportVerification);

    auto instance = AttributionRequestHelper::CreateIfNeeded(
        test_case.eligibility, key_commitment.get());
    bool instance_created = !!instance;
    EXPECT_EQ(instance_created, test_case.expect_instance_to_be_created);
  }
}

TEST_F(AttributionRequestHelperTest, SetAttributionReportingHeaders) {
  {
    std::unique_ptr<net::URLRequest> request =
        CreateTestUrlRequest(/*to_url=*/example_valid_request_url_);

    ResourceRequest resource_request;
    resource_request.attribution_reporting_eligibility =
        AttributionReportingEligibility::kUnset;
    SetAttributionReportingHeaders(*request, resource_request);
    EXPECT_FALSE(request->extra_request_headers().HasHeader(
        kAttributionReportingEligible));
  }

  const struct {
    AttributionReportingEligibility eligibility;
    std::vector<std::string> required_keys;
    std::vector<std::string> prohibited_keys;
  } kTestCases[] = {
      {AttributionReportingEligibility::kEmpty,
       {},
       {"event-source", "navigation-source", "trigger"}},
      {AttributionReportingEligibility::kEventSource,
       {"event-source"},
       {"navigation-source", "trigger"}},
      {AttributionReportingEligibility::kNavigationSource,
       {"navigation-source"},
       {"event-source", "trigger"}},
      {AttributionReportingEligibility::kTrigger,
       {"trigger"},
       {"event-source", "navigation-source"}},
      {AttributionReportingEligibility::kEventSourceOrTrigger,
       {"event-source", "trigger"},
       {"navigation-source"}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.eligibility);

    std::unique_ptr<net::URLRequest> request =
        CreateTestUrlRequest(/*to_url=*/example_valid_request_url_);

    ResourceRequest resource_request;
    resource_request.attribution_reporting_eligibility = test_case.eligibility;
    SetAttributionReportingHeaders(*request, resource_request);

    std::string actual;
    request->extra_request_headers().GetHeader(kAttributionReportingEligible,
                                               &actual);

    auto dict = net::structured_headers::ParseDictionary(actual);
    EXPECT_TRUE(dict.has_value());

    for (const auto& key : test_case.required_keys) {
      EXPECT_TRUE(dict->contains(key)) << key;
    }

    for (const auto& key : test_case.prohibited_keys) {
      EXPECT_FALSE(dict->contains(key)) << key;
    }
  }
}

}  // namespace
}  // namespace network
