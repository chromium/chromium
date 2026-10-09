// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/model/web_payments_metrics_recorder.h"

#import <Foundation/Foundation.h>

#import <cstdint>
#import <string>
#import <string_view>
#import <vector>

#import "base/base64url.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/strcat.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/payments/core/features.h"
#import "components/payments/core/web_payments_telemetry.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

namespace {

using ChallengeRequestUkm =
    ukm::builders::Payments_ThreeDSecure_ChallengeRequest;
using ChallengeResponseUkm =
    ukm::builders::Payments_ThreeDSecure_ChallengeResponse;
using UkmEntries =
    std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>;

constexpr char kChallengeRequestHistogram[] =
    "Payments.ThreeDSecure.ChallengeRequest";
constexpr char kChallengeResponseHistogram[] =
    "Payments.ThreeDSecure.ChallengeResponse";

// Returns form data with a field named `key` holding `message_json` encoded
// like a 3DS message.
std::string CreateFormData(std::string_view key,
                           std::string_view message_json) {
  std::string encoded_message;
  base::Base64UrlEncode(message_json, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_message);
  return base::StrCat({key, "=", encoded_message});
}

class WebPaymentsMetricsRecorderTest : public PlatformTest {
 protected:
  WebPaymentsMetricsRecorderTest() {
    ukm::InitializeSourceUrlRecorderForWebState(&web_state_);
    WebPaymentsMetricsRecorder::CreateForWebState(&web_state_);

    // Commit a main frame document so that UKM can be attributed to it.
    web::FakeNavigationContext main_frame_navigation;
    main_frame_navigation.SetUrl(GURL("https://merchant.test/checkout"));
    main_frame_navigation.SetHasCommitted(true);
    web_state_.OnNavigationStarted(&main_frame_navigation);
    web_state_.OnNavigationFinished(&main_frame_navigation);
    main_frame_source_id_ = ukm::GetSourceIdForWebStateDocument(&web_state_);
  }

  // Asks the policy deciders of `web_state_` whether to allow a subframe
  // navigation request with `form_data` as its body, and returns the decision.
  web::WebStatePolicyDecider::PolicyDecision RequestNavigation(
      std::string_view form_data,
      NSString* http_method = @"POST",
      ui::PageTransition transition = ui::PAGE_TRANSITION_FORM_SUBMIT) {
    NSMutableURLRequest* request = [NSMutableURLRequest
        requestWithURL:[NSURL URLWithString:@"https://acs.test/challenge"]];
    request.HTTPMethod = http_method;
    request.HTTPBody = [NSData dataWithBytes:form_data.data()
                                      length:form_data.size()];
    const web::WebStatePolicyDecider::RequestInfo request_info(
        transition,
        /*target_frame_is_main=*/false,
        /*target_frame_is_cross_origin=*/true,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
    base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
        policy_decision;
    web_state_.ShouldAllowRequest(request, request_info,
                                  policy_decision.GetCallback());
    return policy_decision.Take();
  }

  // Returns the UKM entries named `entry_name`, after checking that they are
  // all attributed to the main frame document of `web_state_`.
  UkmEntries GetUkmEntries(std::string_view entry_name) {
    UkmEntries entries = ukm_recorder_.GetEntriesByName(entry_name);
    for (const ukm::mojom::UkmEntry* entry : entries) {
      EXPECT_EQ(entry->source_id, main_frame_source_id_);
    }
    return entries;
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      payments::features::kThreeDSecureTelemetryForIos};
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  web::FakeWebState web_state_;
  ukm::SourceId main_frame_source_id_ = ukm::kInvalidSourceId;
};

// Tests that no telemetry is recorded when the feature is disabled, and that
// navigations are still allowed.
TEST_F(WebPaymentsMetricsRecorderTest, FeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      payments::features::kThreeDSecureTelemetryForIos);

  EXPECT_TRUE(RequestNavigation(CreateFormData("creq", R"({"acsTransID":"a"})"))
                  .ShouldAllowNavigation());
  histogram_tester_.ExpectTotalCount(kChallengeRequestHistogram, 0);
  EXPECT_TRUE(GetUkmEntries(ChallengeRequestUkm::kEntryName).empty());
}

// Tests that a Challenge Request is recorded against the main frame document
// and its navigation is allowed.
TEST_F(WebPaymentsMetricsRecorderTest, RecordsChallengeRequest) {
  ASSERT_NE(main_frame_source_id_, ukm::kInvalidSourceId);

  EXPECT_TRUE(RequestNavigation(CreateFormData("creq", R"({"acsTransID":"a"})"))
                  .ShouldAllowNavigation());
  histogram_tester_.ExpectUniqueSample(kChallengeRequestHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kChallengeResponseHistogram, 0);

  auto entries = GetUkmEntries(ChallengeRequestUkm::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0], ChallengeRequestUkm::kChallengeRequestName, true);
  EXPECT_TRUE(GetUkmEntries(ChallengeResponseUkm::kEntryName).empty());
}

// Tests that a Challenge Response is recorded against the main frame document
// and its navigation is allowed.
TEST_F(WebPaymentsMetricsRecorderTest, RecordsChallengeResponse) {
  ASSERT_NE(main_frame_source_id_, ukm::kInvalidSourceId);

  EXPECT_TRUE(
      RequestNavigation(
          CreateFormData("cres", R"({"acsTransID":"a","transStatus":"Y"})"))
          .ShouldAllowNavigation());
  histogram_tester_.ExpectUniqueSample(
      kChallengeResponseHistogram,
      payments::ThreeDSecureTransactionStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kChallengeRequestHistogram, 0);

  auto entries = GetUkmEntries(ChallengeResponseUkm::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0], ChallengeResponseUkm::kChallengeResponseName,
      static_cast<int64_t>(payments::ThreeDSecureTransactionStatus::kSuccess));
  EXPECT_TRUE(GetUkmEntries(ChallengeRequestUkm::kEntryName).empty());
}

// Tests that requests which are not HTTP POST requests are ignored.
TEST_F(WebPaymentsMetricsRecorderTest, NotPostRequest) {
  RequestNavigation(CreateFormData("creq", R"({"acsTransID":"a"})"),
                    /*http_method=*/@"GET");
  histogram_tester_.ExpectTotalCount(kChallengeRequestHistogram, 0);
  EXPECT_TRUE(GetUkmEntries(ChallengeRequestUkm::kEntryName).empty());
}

// Tests that requests which are not form submissions are ignored.
TEST_F(WebPaymentsMetricsRecorderTest, NotFormSubmission) {
  RequestNavigation(CreateFormData("creq", R"({"acsTransID":"a"})"),
                    /*http_method=*/@"POST",
                    /*transition=*/ui::PAGE_TRANSITION_LINK);
  histogram_tester_.ExpectTotalCount(kChallengeRequestHistogram, 0);
  EXPECT_TRUE(GetUkmEntries(ChallengeRequestUkm::kEntryName).empty());
}

// Tests that form submissions without a body are allowed and ignored.
TEST_F(WebPaymentsMetricsRecorderTest, EmptyBody) {
  EXPECT_TRUE(RequestNavigation("").ShouldAllowNavigation());
  histogram_tester_.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester_.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(GetUkmEntries(ChallengeRequestUkm::kEntryName).empty());
  EXPECT_TRUE(GetUkmEntries(ChallengeResponseUkm::kEntryName).empty());
}

// Tests that a body reported again, as WebKit does for HTTP 307 and 308
// redirects, is only recorded once.
TEST_F(WebPaymentsMetricsRecorderTest, DeduplicatesRepeatedMessage) {
  std::string form_data = CreateFormData("creq", R"({"acsTransID":"a"})");
  RequestNavigation(form_data);
  RequestNavigation(form_data);
  histogram_tester_.ExpectUniqueSample(kChallengeRequestHistogram, true, 1);
  EXPECT_EQ(GetUkmEntries(ChallengeRequestUkm::kEntryName).size(), 1u);
}

// Tests that Challenge Requests from different transactions are all recorded.
TEST_F(WebPaymentsMetricsRecorderTest, RecordsDifferentTransactions) {
  RequestNavigation(CreateFormData("creq", R"({"acsTransID":"a"})"));
  RequestNavigation(CreateFormData("creq", R"({"acsTransID":"b"})"));
  histogram_tester_.ExpectUniqueSample(kChallengeRequestHistogram, true, 2);
  EXPECT_EQ(GetUkmEntries(ChallengeRequestUkm::kEntryName).size(), 2u);
}

// Tests that the Challenge Request and Challenge Response of one transaction
// are both recorded.
TEST_F(WebPaymentsMetricsRecorderTest,
       RecordsChallengeRequestAndResponseOfTransaction) {
  RequestNavigation(CreateFormData("creq", R"({"acsTransID":"a"})"));
  RequestNavigation(
      CreateFormData("cres", R"({"acsTransID":"a","transStatus":"Y"})"));
  histogram_tester_.ExpectUniqueSample(kChallengeRequestHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(
      kChallengeResponseHistogram,
      payments::ThreeDSecureTransactionStatus::kSuccess, 1);
  EXPECT_EQ(GetUkmEntries(ChallengeRequestUkm::kEntryName).size(), 1u);
  EXPECT_EQ(GetUkmEntries(ChallengeResponseUkm::kEntryName).size(), 1u);
}

// Tests that a repeated encrypted Challenge Response is only recorded once,
// even though its contents cannot be read.
TEST_F(WebPaymentsMetricsRecorderTest, DeduplicatesEncryptedChallengeResponse) {
  RequestNavigation("cres=header.key.iv.ciphertext.tag");
  RequestNavigation("cres=header.key.iv.ciphertext.tag");
  histogram_tester_.ExpectUniqueSample(
      kChallengeResponseHistogram,
      payments::ThreeDSecureTransactionStatus::kJSONEncrypted, 1);
  EXPECT_EQ(GetUkmEntries(ChallengeResponseUkm::kEntryName).size(), 1u);
}

}  // namespace
