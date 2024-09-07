// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_host.h"

#include <stddef.h>

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/aggregation_service/features.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_features.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using BudgetDeniedBehavior = PrivateAggregationBudgeter::BudgetDeniedBehavior;

using testing::_;
using testing::Invoke;
using testing::Property;

auto GenerateAndSaveReportRequest(
    std::optional<AggregatableReportRequest>* out) {
  return
      [out](PrivateAggregationHost::ReportRequestGenerator generator,
            std::vector<blink::mojom::AggregatableReportHistogramContribution>
                contributions,
            auto&&...) {
        *out = std::move(generator).Run(std::move(contributions));
      };
}

constexpr std::string_view kPipeResultHistogram =
    "PrivacySandbox.PrivateAggregation.Host.PipeResult";

constexpr std::string_view kTimeoutResultHistogram =
    "PrivacySandbox.PrivateAggregation.Host.TimeoutResult";

constexpr std::string_view kTimeToGenerateReportRequestWithContextIdHistogram =
    "PrivacySandbox.PrivateAggregation.Host."
    "TimeToGenerateReportRequestWithContextId";

constexpr std::string_view kFilteringIdStatusHistogram =
    "PrivacySandbox.PrivateAggregation.Host.FilteringIdStatus";

void ExpectNumberOfContributionMergeKeysHistogram(
    const base::HistogramTester& tester,
    size_t value,
    PrivateAggregationCallerApi api,
    bool is_reduced_delay) {
  constexpr std::string_view kBaseHistogram =
      "PrivacySandbox.PrivateAggregation.Host.NumContributionMergeKeysInPipe";

  tester.ExpectUniqueSample(kBaseHistogram, value, /*expected_bucket_count=*/1);

  tester.ExpectUniqueSample(
      base::StrCat({kBaseHistogram, ".ProtectedAudience"}), value,
      /*expected_bucket_count=*/
      (api == PrivateAggregationCallerApi::kProtectedAudience) ? 1 : 0);
  tester.ExpectUniqueSample(
      base::StrCat({kBaseHistogram, ".SharedStorage"}), value,
      /*expected_bucket_count=*/
      (api == PrivateAggregationCallerApi::kSharedStorage) ? 1 : 0);

  if (is_reduced_delay) {
    CHECK_EQ(api, PrivateAggregationCallerApi::kSharedStorage);
  }

  tester.ExpectUniqueSample(
      base::StrCat({kBaseHistogram, ".SharedStorage.ReducedDelay"}), value,
      /*expected_bucket_count=*/is_reduced_delay ? 1 : 0);
  tester.ExpectUniqueSample(
      base::StrCat({kBaseHistogram, ".SharedStorage.FullDelay"}), value,
      /*expected_bucket_count=*/
      (api == PrivateAggregationCallerApi::kSharedStorage && !is_reduced_delay)
          ? 1
          : 0);
}

class PrivateAggregationHostTest : public testing::Test {
 public:
  PrivateAggregationHostTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        kPrivateAggregationApiDebugModeRequires3pcEligibility);
    host_ = std::make_unique<PrivateAggregationHost>(
        /*on_report_request_received=*/mock_callback_.Get(),
        /*browser_context=*/&test_browser_context_);
  }

  void TearDown() override { host_.reset(); }

 protected:
  base::MockRepeatingCallback<void(
      PrivateAggregationHost::ReportRequestGenerator,
      std::vector<blink::mojom::AggregatableReportHistogramContribution>,
      PrivateAggregationBudgetKey,
      BudgetDeniedBehavior)>
      mock_callback_;
  std::unique_ptr<PrivateAggregationHost> host_;
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  TestBrowserContext test_browser_context_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivateAggregationHostTest,
       ContributeToHistogram_ReportRequestHasCorrectMembers) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  std::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_,
              Run(_, _,
                  Property(&PrivateAggregationBudgetKey::api,
                           PrivateAggregationCallerApi::kProtectedAudience),
                  BudgetDeniedBehavior::kDontSendReport))
      .WillOnce(GenerateAndSaveReportRequest(&validated_request));

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
  remote->ContributeToHistogram(std::move(contributions));

  // Should not get a request until after the remote is disconnected.
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
  EXPECT_FALSE(validated_request);

  remote.reset();
  host_->FlushReceiverSetForTesting();
  ASSERT_TRUE(validated_request);

  // We only do some basic validation for the scheduled report time and report
  // ID as they are not deterministic and will be copied to `expected_request`.
  // We're using `MOCK_TIME` so we can be sure no time has advanced.
  base::Time now = base::Time::Now();
  EXPECT_GE(validated_request->shared_info().scheduled_report_time,
            now + base::Minutes(10) +
                PrivateAggregationHost::kTimeForLocalProcessing);
  EXPECT_LE(
      validated_request->shared_info().scheduled_report_time,
      now + base::Hours(1) + PrivateAggregationHost::kTimeForLocalProcessing);
  EXPECT_TRUE(validated_request->shared_info().report_id.is_valid());

  // We only made one contribution, and padding would be added later on by
  // `AggregatableReport::Provider::CreateFromRequestAndPublicKeys()`.
  EXPECT_EQ(validated_request->payload_contents().contributions.size(), 1u);

  std::optional<AggregatableReportRequest> expected_request =
      AggregatableReportRequest::Create(
          AggregationServicePayloadContents(
              AggregationServicePayloadContents::Operation::kHistogram,
              {blink::mojom::AggregatableReportHistogramContribution(
                  /*bucket=*/123, /*value=*/456,
                  /*filtering_id=*/std::nullopt)},
              blink::mojom::AggregationServiceMode::kDefault,
              /*aggregation_coordinator_origin=*/std::nullopt,
              /*max_contributions_allowed=*/20u,
              /*filtering_id_max_bytes=*/std::nullopt),
          AggregatableReportSharedInfo(
              validated_request->shared_info().scheduled_report_time,
              validated_request->shared_info().report_id,
              /*reporting_origin=*/kExampleOrigin,
              AggregatableReportSharedInfo::DebugMode::kDisabled,
              /*additional_fields=*/base::Value::Dict(),
              /*api_version=*/"1.0",
              /*api_identifier=*/"protected-audience"),
          AggregatableReportRequest::DelayType::ScheduledWithFullDelay,
          /*reporting_path=*/
          "/.well-known/private-aggregation/report-protected-audience");
  ASSERT_TRUE(expected_request);

  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      validated_request.value(), expected_request.value()));

  histogram.ExpectUniqueSample(
      kPipeResultHistogram, PrivateAggregationHost::PipeResult::kReportSuccess,
      1);
}

TEST_F(PrivateAggregationHostTest, ApiDiffers_RequestUpdatesCorrectly) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  const auto apis = std::to_array<const PrivateAggregationCallerApi>({
      PrivateAggregationCallerApi::kProtectedAudience,
      PrivateAggregationCallerApi::kSharedStorage,
  });

  std::vector<mojo::Remote<blink::mojom::PrivateAggregationHost>> remotes{
      /*n=*/2};
  std::vector<std::optional<AggregatableReportRequest>> validated_requests{
      /*n=*/2};

  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin, apis[i], /*context_id=*/std::nullopt,
        /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remotes[i].BindNewPipeAndPassReceiver()));
    EXPECT_CALL(mock_callback_,
                Run(_, _, Property(&PrivateAggregationBudgetKey::api, apis[i]),
                    BudgetDeniedBehavior::kDontSendReport))
        .WillOnce(GenerateAndSaveReportRequest(&validated_requests[i]));

    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
    remotes[i]->ContributeToHistogram(std::move(contributions));

    remotes[i].FlushForTesting();
    EXPECT_TRUE(remotes[i].is_connected());
    ASSERT_FALSE(validated_requests[i]);

    remotes[i].reset();
    host_->FlushReceiverSetForTesting();
    ASSERT_TRUE(validated_requests[i]);
  }

  EXPECT_EQ(validated_requests[0]->reporting_path(),
            "/.well-known/private-aggregation/report-protected-audience");
  EXPECT_EQ(validated_requests[1]->reporting_path(),
            "/.well-known/private-aggregation/report-shared-storage");

  EXPECT_EQ(validated_requests[0]->shared_info().api_identifier,
            "protected-audience");
  EXPECT_EQ(validated_requests[1]->shared_info().api_identifier,
            "shared-storage");

  histogram.ExpectUniqueSample(
      kPipeResultHistogram, PrivateAggregationHost::PipeResult::kReportSuccess,
      2);
}

TEST_F(PrivateAggregationHostTest, EnableDebugMode_ReflectedInReport) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  std::vector<blink::mojom::DebugModeDetailsPtr> debug_mode_details_args;
  debug_mode_details_args.push_back(blink::mojom::DebugModeDetails::New());
  debug_mode_details_args.push_back(blink::mojom::DebugModeDetails::New(
      /*is_enabled=*/true, /*debug_key=*/nullptr));
  debug_mode_details_args.push_back(blink::mojom::DebugModeDetails::New(
      /*is_enabled=*/true,
      /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/1234u)));

  std::vector<mojo::Remote<blink::mojom::PrivateAggregationHost>> remotes{
      /*n=*/3};
  std::vector<std::optional<AggregatableReportRequest>> validated_requests{
      /*n=*/3};
  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(GenerateAndSaveReportRequest(&validated_requests[0]))
      .WillOnce(GenerateAndSaveReportRequest(&validated_requests[1]))
      .WillOnce(GenerateAndSaveReportRequest(&validated_requests[2]));

  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remotes[i].BindNewPipeAndPassReceiver()));

    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
    remotes[i]->ContributeToHistogram(std::move(contributions));
    if (debug_mode_details_args[i]->is_enabled) {
      remotes[i]->EnableDebugMode(
          std::move(debug_mode_details_args[i]->debug_key));
    }

    remotes[i].reset();
  }
  host_->FlushReceiverSetForTesting();

  ASSERT_TRUE(validated_requests[0].has_value());
  ASSERT_TRUE(validated_requests[1].has_value());
  ASSERT_TRUE(validated_requests[2].has_value());

  EXPECT_EQ(validated_requests[0]->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kDisabled);
  EXPECT_EQ(validated_requests[1]->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kEnabled);
  EXPECT_EQ(validated_requests[2]->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kEnabled);

  EXPECT_EQ(validated_requests[0]->debug_key(), std::nullopt);
  EXPECT_EQ(validated_requests[1]->debug_key(), std::nullopt);
  EXPECT_EQ(validated_requests[2]->debug_key(), 1234u);

  histogram.ExpectUniqueSample(
      kPipeResultHistogram, PrivateAggregationHost::PipeResult::kReportSuccess,
      3);
}

TEST_F(PrivateAggregationHostTest,
       MultipleReceievers_ContributeToHistogramCallsRoutedCorrectly) {
  base::HistogramTester histogram;

  const url::Origin kExampleOriginA =
      url::Origin::Create(GURL("https://a.example"));
  const url::Origin kExampleOriginB =
      url::Origin::Create(GURL("https://b.example"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  std::vector<mojo::Remote<blink::mojom::PrivateAggregationHost>> remotes(
      /*n=*/4);

  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOriginA, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remotes[0].BindNewPipeAndPassReceiver()));
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOriginB, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remotes[1].BindNewPipeAndPassReceiver()));
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOriginA, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remotes[2].BindNewPipeAndPassReceiver()));
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOriginB, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remotes[3].BindNewPipeAndPassReceiver()));

  // Use the bucket as a sentinel to ensure that calls were routed correctly.
  EXPECT_CALL(mock_callback_,
              Run(_, _,
                  Property(&PrivateAggregationBudgetKey::api,
                           PrivateAggregationCallerApi::kProtectedAudience),
                  BudgetDeniedBehavior::kDontSendReport))
      .WillOnce(Invoke(
          [&kExampleOriginB](
              PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior) {
            ASSERT_EQ(contributions.size(), 1u);
            EXPECT_EQ(contributions[0].bucket, 1);
            EXPECT_EQ(budget_key.origin(), kExampleOriginB);
            AggregatableReportRequest request =
                std::move(generator).Run(std::move(contributions));
            EXPECT_EQ(request.shared_info().reporting_origin, kExampleOriginB);
          }));

  EXPECT_CALL(mock_callback_,
              Run(_, _,
                  Property(&PrivateAggregationBudgetKey::api,
                           PrivateAggregationCallerApi::kSharedStorage),
                  BudgetDeniedBehavior::kDontSendReport))
      .WillOnce(Invoke(
          [&kExampleOriginA](
              PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior) {
            ASSERT_EQ(contributions.size(), 1u);
            EXPECT_EQ(contributions[0].bucket, 2);
            AggregatableReportRequest request =
                std::move(generator).Run(std::move(contributions));
            EXPECT_EQ(request.shared_info().reporting_origin, kExampleOriginA);
            EXPECT_EQ(budget_key.origin(), kExampleOriginA);
          }));

  {
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/1, /*value=*/123, /*filtering_id=*/std::nullopt));
    remotes[1]->ContributeToHistogram(std::move(contributions));
  }

  {
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/2, /*value=*/123, /*filtering_id=*/std::nullopt));
    remotes[2]->ContributeToHistogram(std::move(contributions));
  }

  for (auto& remote : remotes) {
    remote.reset();
  }
  host_->FlushReceiverSetForTesting();

  histogram.ExpectTotalCount(kPipeResultHistogram, 4);
  histogram.ExpectBucketCount(
      kPipeResultHistogram, PrivateAggregationHost::PipeResult::kReportSuccess,
      2);
  histogram.ExpectBucketCount(
      kPipeResultHistogram,
      PrivateAggregationHost::PipeResult::kNoReportButNoError, 2);
}

TEST_F(PrivateAggregationHostTest, BindUntrustworthyOriginReceiver_Fails) {
  base::HistogramTester histogram;

  const url::Origin kInsecureOrigin =
      url::Origin::Create(GURL("http://example.com"));
  const url::Origin kOpaqueOrigin;
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote_1;
  EXPECT_FALSE(host_->BindNewReceiver(
      kInsecureOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote_1.BindNewPipeAndPassReceiver()));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote_2;
  EXPECT_FALSE(host_->BindNewReceiver(
      kOpaqueOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote_2.BindNewPipeAndPassReceiver()));

  // Attempt to send a message to an unconnected remote. The request should
  // not be processed.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
  remote_1->ContributeToHistogram(std::move(contributions));

  // Reset then flush to ensure disconnection and the ContributeToHistogram call
  // have had time to be processed.
  remote_1.reset();
  remote_2.reset();
  host_->FlushReceiverSetForTesting();

  histogram.ExpectTotalCount(kPipeResultHistogram, 0);
}

TEST_F(PrivateAggregationHostTest, BindReceiverWithTooLongContextId_Fails) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  const std::string kTooLongContextId =
      "this_is_an_example_of_a_context_id_that_is_too_long_to_be_allowed";

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_FALSE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience, kTooLongContextId,
      /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  // Attempt to send a message to an unconnected remote. The request should
  // not be processed.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
  remote->ContributeToHistogram(std::move(contributions));

  // Reset then flush to ensure disconnection and the ContributeToHistogram call
  // have had time to be processed.
  remote.reset();
  host_->FlushReceiverSetForTesting();

  histogram.ExpectTotalCount(kPipeResultHistogram, 0);
}

TEST_F(PrivateAggregationHostTest, TimeoutSetWithoutDeterministicReport_Fails) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_FALSE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt,
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));
}

TEST_F(PrivateAggregationHostTest, TimeoutSetWithContextId_Succeeds) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/"example_context_id",
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));
}

TEST_F(PrivateAggregationHostTest,
       TimeoutSetWithNonDefaultFilteringIdMaxBytes_Succeeds) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kPrivateAggregationApiFilteringIds,
                            kPrivacySandboxAggregationServiceFilteringIds},
      /*disabled_features=*/{});

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt,
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      /*filtering_id_max_bytes=*/3, remote.BindNewPipeAndPassReceiver()));
}

TEST_F(PrivateAggregationHostTest, InvalidRequest_Rejected) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  // Negative values are invalid
  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      negative_contributions;
  negative_contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/-1, /*filtering_id=*/std::nullopt));

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      valid_contributions;
  valid_contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));

  EXPECT_CALL(mock_callback_, Run).Times(0);

  {
    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver()));

    base::HistogramTester histogram;
    remote->ContributeToHistogram(std::move(negative_contributions));
    remote.reset();
    host_->FlushReceiverSetForTesting();
    histogram.ExpectUniqueSample(
        kPipeResultHistogram,
        PrivateAggregationHost::PipeResult::kNegativeValue, 1);
  }
  {
    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver()));

    base::HistogramTester histogram;

    remote->ContributeToHistogram(std::move(valid_contributions));
    remote->EnableDebugMode(
        /*debug_key=*/nullptr);
    remote->EnableDebugMode(
        /*debug_key=*/blink::mojom::DebugKey::New(1234u));
    remote.reset();
    host_->FlushReceiverSetForTesting();
    histogram.ExpectUniqueSample(
        kPipeResultHistogram,
        PrivateAggregationHost::PipeResult::kEnableDebugModeCalledMultipleTimes,
        1);
  }
}

constexpr struct {
  std::string_view label;
  bool should_enable_per_calling_api_sizing;
  PrivateAggregationCallerApi caller_api;
  size_t expected_num_contributions;
} kMaxNumContributionsTestCases[]{
    {
        "Shared Storage gets legacy number of contributions when "
        "per-caller-API report sizing is disabled",
        /*should_enable_per_calling_api_sizing=*/false,
        /*caller_api=*/PrivateAggregationCallerApi::kSharedStorage,
        /*expected_num_contributions=*/20,
    },
    {
        "Protected Audience gets legacy number of contributions when "
        "per-caller-API report sizing is disabled",
        /*should_enable_per_calling_api_sizing=*/false,
        /*caller_api=*/PrivateAggregationCallerApi::kProtectedAudience,
        /*expected_num_contributions=*/20,
    },
    {
        "Shared Storage is unaffected when per-caller-API report sizing is "
        "enabled",
        /*should_enable_per_calling_api_sizing=*/true,
        /*caller_api=*/PrivateAggregationCallerApi::kSharedStorage,
        /*expected_num_contributions=*/20,
    },
    {
        "Protected Audience gets more contributions when per-caller-API "
        "report sizing is enabled",
        /*should_enable_per_calling_api_sizing=*/true,
        /*caller_api=*/PrivateAggregationCallerApi::kProtectedAudience,
        /*expected_num_contributions=*/100,
    }};

TEST_F(PrivateAggregationHostTest,
       TooManyContributionsWithMergingDisabled_Truncated) {
  for (const auto& test_case : kMaxNumContributionsTestCases) {
    SCOPED_TRACE(testing::Message() << test_case.label);

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features{
        kPrivateAggregationApiContributionMerging};

    if (test_case.should_enable_per_calling_api_sizing) {
      enabled_features.push_back(
          content::kPrivateAggregationApi100ContributionsForProtectedAudience);
    } else {
      disabled_features.push_back(
          content::kPrivateAggregationApi100ContributionsForProtectedAudience);
    }

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);

    const url::Origin kExampleOrigin =
        url::Origin::Create(GURL("https://example.com"));
    const url::Origin kMainFrameOrigin =
        url::Origin::Create(GURL("https://main_frame.com"));

    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin, test_case.caller_api,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver()));
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        too_many_contributions;
    for (size_t i = 0; i < test_case.expected_num_contributions + 1; ++i) {
      too_many_contributions.push_back(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123 + i, /*value=*/1, /*filtering_id=*/std::nullopt));
    }
    base::HistogramTester histogram;

    std::optional<AggregatableReportRequest> validated_request;
    EXPECT_CALL(mock_callback_, Run)
        .WillOnce(GenerateAndSaveReportRequest(&validated_request));

    remote->ContributeToHistogram(std::move(too_many_contributions));
    remote.reset();
    host_->FlushReceiverSetForTesting();
    histogram.ExpectUniqueSample(
        kPipeResultHistogram,
        PrivateAggregationHost::PipeResult::
            kReportSuccessButTruncatedDueToTooManyContributions,
        1);

    ASSERT_TRUE(validated_request);
    EXPECT_EQ(validated_request->payload_contents().contributions.size(),
              test_case.expected_num_contributions);
  }
}

TEST_F(PrivateAggregationHostTest,
       TooManyContributionsWithMergingEnabled_Truncated) {
  for (const auto& test_case : kMaxNumContributionsTestCases) {
    SCOPED_TRACE(testing::Message() << test_case.label);

    std::vector<base::test::FeatureRef> enabled_features{
        kPrivateAggregationApiContributionMerging};
    std::vector<base::test::FeatureRef> disabled_features;

    if (test_case.should_enable_per_calling_api_sizing) {
      enabled_features.push_back(
          content::kPrivateAggregationApi100ContributionsForProtectedAudience);
    } else {
      disabled_features.push_back(
          content::kPrivateAggregationApi100ContributionsForProtectedAudience);
    }

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);

    const url::Origin kExampleOrigin =
        url::Origin::Create(GURL("https://example.com"));
    const url::Origin kMainFrameOrigin =
        url::Origin::Create(GURL("https://main_frame.com"));

    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin, test_case.caller_api,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver()));
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        too_many_contributions;
    for (size_t i = 0; i < test_case.expected_num_contributions + 1; ++i) {
      too_many_contributions.push_back(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123 + i, /*value=*/1, /*filtering_id=*/std::nullopt));
    }

    base::HistogramTester histogram;

    std::optional<AggregatableReportRequest> validated_request;
    EXPECT_CALL(mock_callback_, Run)
        .WillOnce(GenerateAndSaveReportRequest(&validated_request));

    remote->ContributeToHistogram(std::move(too_many_contributions));
    remote.reset();
    host_->FlushReceiverSetForTesting();
    histogram.ExpectUniqueSample(
        kPipeResultHistogram,
        PrivateAggregationHost::PipeResult::
            kReportSuccessButTruncatedDueToTooManyContributions,
        1);

    ASSERT_TRUE(validated_request);
    EXPECT_EQ(validated_request->payload_contents().contributions.size(),
              test_case.expected_num_contributions);
  }
}

TEST_F(PrivateAggregationHostTest,
       ContributionsMergedIffSameBucketAndFilteringId) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kPrivateAggregationApiFilteringIds,
                            kPrivateAggregationApiContributionMerging,
                            kPrivacySandboxAggregationServiceFilteringIds},
      /*disabled_features=*/{});

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));
  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/1, /*filtering_id=*/std::nullopt));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/0, /*filtering_id=*/std::nullopt));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/1, /*filtering_id=*/0));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/2, /*filtering_id=*/0));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/1, /*filtering_id=*/1));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/124, /*value=*/1, /*filtering_id=*/std::nullopt));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/125, /*value=*/0, /*filtering_id=*/1));

  base::HistogramTester histogram;

  std::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(GenerateAndSaveReportRequest(&validated_request));

  remote->ContributeToHistogram(std::move(contributions));
  remote.reset();
  host_->FlushReceiverSetForTesting();
  histogram.ExpectUniqueSample(
      kPipeResultHistogram, PrivateAggregationHost::PipeResult::kReportSuccess,
      1);

  ASSERT_TRUE(validated_request);
  EXPECT_THAT(
      validated_request->payload_contents().contributions,
      testing::UnorderedElementsAre(
          testing::Eq(blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123, /*value=*/4, /*filtering_id=*/std::nullopt)),
          testing::Eq(blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123, /*value=*/1, /*filtering_id=*/1)),
          testing::Eq(blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/124, /*value=*/1, /*filtering_id=*/std::nullopt))));

  ExpectNumberOfContributionMergeKeysHistogram(
      histogram, 3, PrivateAggregationCallerApi::kProtectedAudience,
      /*is_reduced_delay=*/false);
}

TEST_F(PrivateAggregationHostTest, ContributionsNotMergedIfFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kPrivateAggregationApiFilteringIds,
                            kPrivacySandboxAggregationServiceFilteringIds},
      /*disabled_features=*/{kPrivateAggregationApiContributionMerging});

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));
  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/1, /*filtering_id=*/std::nullopt));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/0, /*filtering_id=*/std::nullopt));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/1, /*filtering_id=*/0));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/2, /*filtering_id=*/0));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/1, /*filtering_id=*/1));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/124, /*value=*/1, /*filtering_id=*/std::nullopt));
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/125, /*value=*/0, /*filtering_id=*/1));

  base::HistogramTester histogram;

  std::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(GenerateAndSaveReportRequest(&validated_request));

  remote->ContributeToHistogram(std::move(contributions));
  remote.reset();
  host_->FlushReceiverSetForTesting();
  histogram.ExpectUniqueSample(
      kPipeResultHistogram, PrivateAggregationHost::PipeResult::kReportSuccess,
      1);

  ASSERT_TRUE(validated_request);
  EXPECT_THAT(
      validated_request->payload_contents().contributions,
      testing::UnorderedElementsAre(
          testing::Eq(blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123, /*value=*/1, /*filtering_id=*/std::nullopt)),
          testing::Eq(blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123, /*value=*/0, /*filtering_id=*/std::nullopt)),
          testing::Eq(blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123, /*value=*/1, /*filtering_id=*/0)),
          testing::Eq(blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123, /*value=*/2, /*filtering_id=*/0)),
          testing::Eq(blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123, /*value=*/1, /*filtering_id=*/1)),
          testing::Eq(blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/124, /*value=*/1, /*filtering_id=*/std::nullopt)),
          testing::Eq(blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/125, /*value=*/0, /*filtering_id=*/1))));

  constexpr std::string_view kBaseHistogram =
      "PrivacySandbox.PrivateAggregation.Host.NumContributionMergeKeysInPipe";
  constexpr std::string_view kSuffixesToTest[] = {
      "",
      ".ProtectedAudience",
      ".SharedStorage",
      ".SharedStorage.ReducedDelay",
      ".SharedStorage.FullDelay",
  };

  for (std::string_view suffix : kSuffixesToTest) {
    histogram.ExpectTotalCount(base::StrCat({kBaseHistogram, suffix}), 0);
  }
}

TEST_F(PrivateAggregationHostTest,
       MergeableContributions_NotTruncatedUnnecessarily) {
  for (const auto& test_case : kMaxNumContributionsTestCases) {
    SCOPED_TRACE(testing::Message() << test_case.label);

    std::vector<base::test::FeatureRef> enabled_features{
        kPrivateAggregationApiContributionMerging};
    std::vector<base::test::FeatureRef> disabled_features;

    if (test_case.should_enable_per_calling_api_sizing) {
      enabled_features.push_back(
          content::kPrivateAggregationApi100ContributionsForProtectedAudience);
    } else {
      disabled_features.push_back(
          content::kPrivateAggregationApi100ContributionsForProtectedAudience);
    }

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);

    const url::Origin kExampleOrigin =
        url::Origin::Create(GURL("https://example.com"));
    const url::Origin kMainFrameOrigin =
        url::Origin::Create(GURL("https://main_frame.com"));

    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin, test_case.caller_api,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver()));
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        too_many_contributions_unless_merged;
    for (size_t i = 0; i < test_case.expected_num_contributions + 1; ++i) {
      too_many_contributions_unless_merged.push_back(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123, /*value=*/1, /*filtering_id=*/std::nullopt));
    }

    base::HistogramTester histogram;

    std::optional<AggregatableReportRequest> validated_request;
    EXPECT_CALL(mock_callback_, Run)
        .WillOnce(GenerateAndSaveReportRequest(&validated_request));

    remote->ContributeToHistogram(
        std::move(too_many_contributions_unless_merged));
    remote.reset();
    host_->FlushReceiverSetForTesting();
    histogram.ExpectUniqueSample(
        kPipeResultHistogram,
        PrivateAggregationHost::PipeResult::kReportSuccess, 1);

    ASSERT_TRUE(validated_request);
    ASSERT_EQ(validated_request->payload_contents().contributions.size(), 1u);
    EXPECT_EQ(validated_request->payload_contents().contributions.at(0).bucket,
              123);
    EXPECT_EQ(
        base::checked_cast<size_t>(
            validated_request->payload_contents().contributions.at(0).value),
        test_case.expected_num_contributions + 1);
    EXPECT_EQ(
        validated_request->payload_contents().contributions.at(0).filtering_id,
        std::nullopt);

    ExpectNumberOfContributionMergeKeysHistogram(histogram, 1,
                                                 test_case.caller_api,
                                                 /*is_reduced_delay=*/false);
  }
}

TEST_F(PrivateAggregationHostTest,
       ZeroValueContributions_DroppedAndTruncationHistogramNotTriggered) {
  for (const auto& test_case : kMaxNumContributionsTestCases) {
    SCOPED_TRACE(testing::Message() << test_case.label);

    std::vector<base::test::FeatureRef> enabled_features{
        kPrivateAggregationApiContributionMerging};
    std::vector<base::test::FeatureRef> disabled_features;

    if (test_case.should_enable_per_calling_api_sizing) {
      enabled_features.push_back(
          content::kPrivateAggregationApi100ContributionsForProtectedAudience);
    } else {
      disabled_features.push_back(
          content::kPrivateAggregationApi100ContributionsForProtectedAudience);
    }

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);

    const url::Origin kExampleOrigin =
        url::Origin::Create(GURL("https://example.com"));
    const url::Origin kMainFrameOrigin =
        url::Origin::Create(GURL("https://main_frame.com"));

    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin, test_case.caller_api,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver()));
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    for (size_t i = 0; i < test_case.expected_num_contributions; ++i) {
      contributions.push_back(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123 + i, /*value=*/1, /*filtering_id=*/std::nullopt));
    }

    // This contribution would put us over the limit, but it will be dropped due
    // to its zero value.
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123 + test_case.expected_num_contributions,
            /*value=*/0, /*filtering_id=*/std::nullopt));

    base::HistogramTester histogram;

    std::optional<AggregatableReportRequest> validated_request;
    EXPECT_CALL(mock_callback_, Run)
        .WillOnce(GenerateAndSaveReportRequest(&validated_request));

    remote->ContributeToHistogram(std::move(contributions));
    remote.reset();
    host_->FlushReceiverSetForTesting();

    // Histogram does *not* indicate truncation as only zero value contributions
    // were lost.
    histogram.ExpectUniqueSample(
        kPipeResultHistogram,
        PrivateAggregationHost::PipeResult::kReportSuccess, 1);

    ASSERT_TRUE(validated_request);
    EXPECT_EQ(validated_request->payload_contents().contributions.size(),
              test_case.expected_num_contributions);

    ExpectNumberOfContributionMergeKeysHistogram(
        histogram, test_case.expected_num_contributions, test_case.caller_api,
        /*is_reduced_delay=*/false);
  }
}

TEST_F(PrivateAggregationHostTest,
       NumberOfContributionMergeKeysHistograms_RecordsCorrectSubMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kPrivateAggregationApiFilteringIds,
                            kPrivateAggregationApiContributionMerging,
                            kPrivacySandboxAggregationServiceFilteringIds},
      /*disabled_features=*/{});

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      example_contributions;
  example_contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/1, /*filtering_id=*/std::nullopt));
  example_contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/1, /*filtering_id=*/0));
  example_contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/2, /*filtering_id=*/0));
  example_contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/1, /*filtering_id=*/1));
  example_contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/124, /*value=*/1, /*filtering_id=*/std::nullopt));

  constexpr size_t kExpectedNumberMergeKeys = 3;

  const struct {
    const std::string_view description;
    PrivateAggregationCallerApi api;
    std::optional<std::string> context_id;
    size_t filtering_id_max_bytes;
    std::optional<base::TimeDelta> timeout;
  } kTestCases[] = {
      {
          "Protected Audience",
          PrivateAggregationCallerApi::kProtectedAudience,
          /*context_id=*/std::nullopt,
          PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
          /*timeout=*/std::nullopt,
      },
      {
          "Shared Storage full delay",
          PrivateAggregationCallerApi::kSharedStorage,
          /*context_id=*/std::nullopt,
          PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
          /*timeout=*/std::nullopt,
      },
      {
          "Shared Storage reduced delay due to context ID",
          PrivateAggregationCallerApi::kSharedStorage,
          /*context_id=*/"example_context_id",
          PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
          /*timeout=*/base::Seconds(5),
      },
      {
          "Shared Storage reduced delay due to filtering ID max bytes",
          PrivateAggregationCallerApi::kSharedStorage,
          /*context_id=*/std::nullopt,
          /*filtering_id_max_bytes=*/3,
          /*timeout=*/base::Seconds(5),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    base::HistogramTester histogram;

    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    bool bind_result = host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin, test_case.api,
        /*context_id=*/test_case.context_id, /*timeout=*/test_case.timeout,
        /*aggregation_coordinator_origin=*/std::nullopt,
        /*filtering_id_max_bytes=*/test_case.filtering_id_max_bytes,
        remote.BindNewPipeAndPassReceiver());
    EXPECT_TRUE(bind_result);

    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    for (const auto& contribution : example_contributions) {
      contributions.push_back(contribution->Clone());
    }

    remote->ContributeToHistogram(std::move(contributions));

    remote.reset();
    host_->FlushReceiverSetForTesting();

    ExpectNumberOfContributionMergeKeysHistogram(
        histogram, kExpectedNumberMergeKeys, test_case.api,
        /*is_reduced_delay=*/test_case.timeout.has_value());
  }
}

TEST_F(PrivateAggregationHostTest, PrivateAggregationAllowed_RequestSucceeds) {
  base::HistogramTester histogram;

  MockPrivateAggregationContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  // If the API is enabled, the call should succeed.
  EXPECT_CALL(browser_client, IsPrivateAggregationAllowed(_, kMainFrameOrigin,
                                                          kExampleOrigin, _))
      .Times(2)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_callback_, Run);

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
  remote->ContributeToHistogram(std::move(contributions));

  remote.reset();
  host_->FlushReceiverSetForTesting();

  histogram.ExpectUniqueSample(
      kPipeResultHistogram, PrivateAggregationHost::PipeResult::kReportSuccess,
      1);
}

TEST_F(PrivateAggregationHostTest, PrivateAggregationDisallowed_RequestFails) {
  base::HistogramTester histogram;

  MockPrivateAggregationContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  // If the API is enabled, the call should succeed.
  EXPECT_CALL(browser_client, IsPrivateAggregationAllowed(_, kMainFrameOrigin,
                                                          kExampleOrigin, _))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(mock_callback_, Run).Times(0);

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
  remote->ContributeToHistogram(std::move(contributions));

  remote.reset();
  host_->FlushReceiverSetForTesting();

  histogram.ExpectUniqueSample(
      kPipeResultHistogram,
      PrivateAggregationHost::PipeResult::kApiDisabledInSettings, 1);
}

TEST_F(PrivateAggregationHostTest, ContextIdSet_ReflectedInSingleReport) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience, "example_context_id",
      /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  constexpr base::TimeDelta kTimeToGenerateReportRequest =
      base::Milliseconds(123);

  std::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_,
              Run(_, _, _, BudgetDeniedBehavior::kSendNullReport))

      .WillOnce(testing::DoAll(
          [&] {
            task_environment_.FastForwardBy(kTimeToGenerateReportRequest);
          },
          GenerateAndSaveReportRequest(&validated_request)));

  {
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
    remote->ContributeToHistogram(std::move(contributions));
  }

  remote.reset();
  host_->FlushReceiverSetForTesting();

  ASSERT_TRUE(validated_request.has_value());

  EXPECT_THAT(
      validated_request->additional_fields(),
      testing::ElementsAre(testing::Pair("context_id", "example_context_id")));

  histogram.ExpectUniqueSample(
      kPipeResultHistogram, PrivateAggregationHost::PipeResult::kReportSuccess,
      1);

  histogram.ExpectUniqueTimeSample(
      kTimeToGenerateReportRequestWithContextIdHistogram,
      kTimeToGenerateReportRequest, 1);
}

TEST_F(PrivateAggregationHostTest,
       ContextIdSetNoContributions_NullReportSentWithoutDebugModeEnabled) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  std::vector<blink::mojom::DebugModeDetailsPtr> debug_mode_details_args;
  debug_mode_details_args.push_back(blink::mojom::DebugModeDetails::New());
  debug_mode_details_args.push_back(blink::mojom::DebugModeDetails::New(
      /*is_enabled=*/true, /*debug_key=*/nullptr));
  debug_mode_details_args.push_back(blink::mojom::DebugModeDetails::New(
      /*is_enabled=*/true,
      /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/1234u)));

  std::vector<std::optional<AggregatableReportRequest>> validated_requests{
      /*n=*/3};
  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(GenerateAndSaveReportRequest(&validated_requests[0]))
      .WillOnce(GenerateAndSaveReportRequest(&validated_requests[1]))
      .WillOnce(GenerateAndSaveReportRequest(&validated_requests[2]));
  for (auto& debug_mode_details_arg : debug_mode_details_args) {
    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience, "example_context_id",
        /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver()));

    if (debug_mode_details_arg->is_enabled) {
      remote->EnableDebugMode(std::move(debug_mode_details_arg->debug_key));
    }

    EXPECT_TRUE(remote.is_connected());
    remote.reset();
  }

  host_->FlushReceiverSetForTesting();

  for (std::optional<AggregatableReportRequest>& validated_request :
       validated_requests) {
    ASSERT_TRUE(validated_request.has_value());
    EXPECT_THAT(validated_request->additional_fields(),
                testing::ElementsAre(
                    testing::Pair("context_id", "example_context_id")));
    ASSERT_TRUE(validated_request->payload_contents().contributions.empty());

    // Null reports never have debug mode set according to the current spec.
    EXPECT_EQ(validated_request->shared_info().debug_mode,
              AggregatableReportSharedInfo::DebugMode::kDisabled);
    EXPECT_EQ(validated_request->debug_key(), std::nullopt);
  }
}

TEST_F(PrivateAggregationHostTest, ContextIdNotSet_NoNullReportSent) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  EXPECT_CALL(mock_callback_, Run).Times(0);

  {
    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver()));

    EXPECT_TRUE(remote.is_connected());
    remote.reset();
    host_->FlushReceiverSetForTesting();
  }

  {
    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver()));

    // Enabling the debug mode has no effect.
    remote->EnableDebugMode(
        /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/1234u));

    EXPECT_TRUE(remote.is_connected());
    remote.reset();
    host_->FlushReceiverSetForTesting();
  }

  // This histogram should only be recorded when there is a context ID.
  histogram.ExpectTotalCount(kTimeToGenerateReportRequestWithContextIdHistogram,
                             0);
}

TEST_F(PrivateAggregationHostTest, AggregationCoordinatorOrigin) {
  ::aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting
      scoped_coordinator_allowlist(
          {url::Origin::Create(GURL("https://a.test"))});

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  const url::Origin kValidCoordinatorOrigin =
      url::Origin::Create(GURL("https://a.test"));
  const url::Origin kInvalidCoordinatorOrigin =
      url::Origin::Create(GURL("https://b.test"));

  const struct {
    const char* description;
    const std::optional<url::Origin> aggregation_coordinator_origin;
    bool expected_bind_result;
  } kTestCases[] = {
      {
          "aggregation_coordinator_origin_empty",
          std::nullopt,
          true,
      },
      {
          "aggregation_coordinator_origin_valid",
          kValidCoordinatorOrigin,
          true,
      },
      {
          "aggregation_coordinator_origin_invalid",
          kInvalidCoordinatorOrigin,
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histogram;

    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    bool bind_result = host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        test_case.aggregation_coordinator_origin,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver());

    EXPECT_EQ(bind_result, test_case.expected_bind_result);
    if (!bind_result) {
      continue;
    }

    std::optional<AggregatableReportRequest> validated_request;
    EXPECT_CALL(mock_callback_, Run)
        .WillOnce(GenerateAndSaveReportRequest(&validated_request));

    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
    remote->ContributeToHistogram(std::move(contributions));

    remote.reset();
    host_->FlushReceiverSetForTesting();

    histogram.ExpectUniqueSample(
        kPipeResultHistogram,
        PrivateAggregationHost::PipeResult::kReportSuccess, 1);

    ASSERT_TRUE(validated_request);
    EXPECT_EQ(
        validated_request->payload_contents().aggregation_coordinator_origin,
        test_case.aggregation_coordinator_origin)
        << test_case.description;
  }
}

TEST_F(PrivateAggregationHostTest, FilteringIdMaxBytesValidated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kPrivateAggregationApiFilteringIds,
                            kPrivacySandboxAggregationServiceFilteringIds},
      /*disabled_features=*/{});

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  const struct {
    const char* description;
    const size_t filtering_id_max_bytes;
    bool expected_bind_result;
  } kTestCases[] = {
      {
          "filtering_id_max_bytes zero",
          0,
          false,
      },
      {
          "filtering_id_max_bytes minimum valid value",
          1,
          true,
      },
      {
          "filtering_id_max_bytes maximum valid value",
          AggregationServicePayloadContents::kMaximumFilteringIdMaxBytes,
          true,
      },
      {
          "filtering_id_max_bytes too large",
          AggregationServicePayloadContents::kMaximumFilteringIdMaxBytes + 1,
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    bool bind_result = host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        /*filtering_id_max_bytes=*/test_case.filtering_id_max_bytes,
        remote.BindNewPipeAndPassReceiver());

    EXPECT_EQ(bind_result, test_case.expected_bind_result);
  }
}

TEST_F(PrivateAggregationHostTest, FilteringIdValidatedToFitInMaxBytes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kPrivateAggregationApiFilteringIds,
                            kPrivacySandboxAggregationServiceFilteringIds},
      /*disabled_features=*/{});

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  const struct {
    const char* description;
    const size_t filtering_id_max_bytes;
    const std::optional<uint64_t> filtering_id;
    bool expected_to_be_valid;
    std::optional<PrivateAggregationHost::FilteringIdStatus>
        expected_filtering_id_histogram;
  } kTestCases[] = {
      {
          "filtering_id null with default maxBytes",
          1,
          std::nullopt,
          true,
          PrivateAggregationHost::FilteringIdStatus::
              kNoFilteringIdWithDefaultMaxBytes,
      },
      {
          "filtering_id 0",
          1,
          0,
          true,
          PrivateAggregationHost::FilteringIdStatus::
              kFilteringIdProvidedWithDefaultMaxBytes,
      },
      {
          "filtering_id max for one byte",
          1,
          255,
          true,
          PrivateAggregationHost::FilteringIdStatus::
              kFilteringIdProvidedWithDefaultMaxBytes,
      },
      {
          "filtering_id too big",
          1,
          256,
          false,
          std::nullopt,
      },
      {
          "filtering_id null with custom maxBytes",
          3,
          std::nullopt,
          true,
          PrivateAggregationHost::FilteringIdStatus::
              kNoFilteringIdWithCustomMaxBytes,
      },
      {
          "filtering_id max value for max maxBytes",
          8,
          std::numeric_limits<uint64_t>::max(),
          true,
          PrivateAggregationHost::FilteringIdStatus::
              kFilteringIdProvidedWithCustomMaxBytes,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    base::HistogramTester histogram;

    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    bool bind_result = host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        /*filtering_id_max_bytes=*/test_case.filtering_id_max_bytes,
        remote.BindNewPipeAndPassReceiver());

    ASSERT_TRUE(bind_result);

    std::optional<AggregatableReportRequest> validated_request;
    if (test_case.expected_to_be_valid) {
      EXPECT_CALL(mock_callback_, Run)
          .WillOnce(GenerateAndSaveReportRequest(&validated_request));
    } else {
      EXPECT_CALL(mock_callback_, Run).Times(0);
    }

    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/456, test_case.filtering_id));
    remote->ContributeToHistogram(std::move(contributions));

    // The pipe should've been closed in case of a validation error.
    remote.FlushForTesting();
    EXPECT_EQ(remote.is_connected(), test_case.expected_to_be_valid);

    remote.reset();
    host_->FlushReceiverSetForTesting();

    histogram.ExpectUniqueSample(
        kPipeResultHistogram,
        test_case.expected_to_be_valid
            ? PrivateAggregationHost::PipeResult::kReportSuccess
            : PrivateAggregationHost::PipeResult::kFilteringIdInvalid,
        1);

    if (test_case.expected_filtering_id_histogram.has_value()) {
      histogram.ExpectUniqueSample(
          kFilteringIdStatusHistogram,
          test_case.expected_filtering_id_histogram.value(), 1);
    } else {
      histogram.ExpectTotalCount(kFilteringIdStatusHistogram, 0);
    }

    EXPECT_EQ(validated_request.has_value(), test_case.expected_to_be_valid);
    if (!validated_request.has_value()) {
      continue;
    }
    ASSERT_EQ(validated_request->payload_contents().contributions.size(), 1u);
    EXPECT_EQ(validated_request->payload_contents().filtering_id_max_bytes,
              test_case.filtering_id_max_bytes);
    EXPECT_EQ(
        validated_request->payload_contents().contributions[0].filtering_id,
        test_case.filtering_id);
  }
}

TEST_F(PrivateAggregationHostTest,
       FilteringIdMaxBytesNotValidatedIfFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kPrivateAggregationApiFilteringIds);

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  const struct {
    const char* description;
    const size_t filtering_id_max_bytes;
  } kTestCases[] = {
      {
          "filtering_id_max_bytes zero",
          0,
      },
      {
          "filtering_id_max_bytes minimum valid value",
          1,
      },
      {
          "filtering_id_max_bytes maximum valid value",
          AggregationServicePayloadContents::kMaximumFilteringIdMaxBytes,
      },
      {
          "filtering_id_max_bytes too large",
          AggregationServicePayloadContents::kMaximumFilteringIdMaxBytes + 1,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    bool bind_result = host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        /*filtering_id_max_bytes=*/test_case.filtering_id_max_bytes,
        remote.BindNewPipeAndPassReceiver());

    EXPECT_TRUE(bind_result);
  }
}

TEST_F(PrivateAggregationHostTest,
       FilteringIdNotValidatedToFitInMaxBytesIfFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kPrivateAggregationApiFilteringIds);

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  const struct {
    const char* description;
    const size_t filtering_id_max_bytes;
    const std::optional<uint64_t> filtering_id;
  } kTestCases[] = {
      {
          "filtering_id null with default max bytes",
          1,
          std::nullopt,
      },
      {
          "filtering_id 0",
          1,
          0,
      },
      {
          "filtering_id max for one byte",
          1,
          255,
      },
      {
          "filtering_id too big",
          1,
          256,
      },
      {
          "filtering_id null with custom maxBytes",
          3,
          std::nullopt,
      },
      {
          "filtering_id max value for max maxBytes",
          8,
          std::numeric_limits<uint64_t>::max(),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    for (bool should_enable_debug_mode : {false, true}) {
      base::HistogramTester histogram;

      mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
      bool bind_result = host_->BindNewReceiver(
          kExampleOrigin, kMainFrameOrigin,
          PrivateAggregationCallerApi::kProtectedAudience,
          /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
          /*aggregation_coordinator_origin=*/std::nullopt,
          /*filtering_id_max_bytes=*/test_case.filtering_id_max_bytes,
          remote.BindNewPipeAndPassReceiver());

      ASSERT_TRUE(bind_result);

      // We shouldn't have any validation errors if the feature is disabled.
      std::optional<AggregatableReportRequest> validated_request;
      EXPECT_CALL(mock_callback_, Run)
          .WillOnce(GenerateAndSaveReportRequest(&validated_request));
      if (should_enable_debug_mode) {
        remote->EnableDebugMode(/*debug_key=*/nullptr);
      }

      std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
          contributions;
      contributions.push_back(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123, /*value=*/456, test_case.filtering_id));
      remote->ContributeToHistogram(std::move(contributions));

      // The pipe should've been closed in case of a validation error.
      remote.FlushForTesting();
      EXPECT_TRUE(remote.is_connected());

      remote.reset();
      host_->FlushReceiverSetForTesting();

      histogram.ExpectUniqueSample(
          kPipeResultHistogram,
          PrivateAggregationHost::PipeResult::kReportSuccess, 1);

      histogram.ExpectTotalCount(kFilteringIdStatusHistogram, 0);

      ASSERT_TRUE(validated_request.has_value());
      ASSERT_EQ(validated_request->payload_contents().contributions.size(), 1u);

      // The filtering IDs should always be default if the feature is disabled.
      EXPECT_FALSE(validated_request->payload_contents()
                       .filtering_id_max_bytes.has_value());
      EXPECT_FALSE(validated_request->payload_contents()
                       .contributions[0]
                       .filtering_id.has_value());
    }
  }
}

TEST_F(PrivateAggregationHostTest,
       DebugModeFeatureParamsAndSettingsCheckAppliedCorrectly) {
  struct {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    bool expected_debug_mode_settings_check;

    // No effect if `expected_debug_mode_settings_check` is false.
    bool approve_debug_mode_settings_check = false;
    bool call_enable_debug_mode = true;
    AggregatableReportSharedInfo::DebugMode expected_debug_mode;
  } kTestCases[] = {
      {
          .enabled_features = {{blink::features::kPrivateAggregationApi,
                                {{"debug_mode_enabled_at_all", "false"}}}},
          .expected_debug_mode_settings_check = false,
          .expected_debug_mode =
              AggregatableReportSharedInfo::DebugMode::kDisabled,
      },
      {
          .enabled_features =
              {{kPrivateAggregationApiDebugModeRequires3pcEligibility, {}}},
          .expected_debug_mode_settings_check = true,
          .approve_debug_mode_settings_check = false,
          .expected_debug_mode =
              AggregatableReportSharedInfo::DebugMode::kDisabled,
      },
      {
          .enabled_features =
              {{kPrivateAggregationApiDebugModeRequires3pcEligibility, {}}},
          .expected_debug_mode_settings_check = true,
          .approve_debug_mode_settings_check = true,
          .expected_debug_mode =
              AggregatableReportSharedInfo::DebugMode::kEnabled,
      },
      {
          .enabled_features =
              {{kPrivateAggregationApiDebugModeRequires3pcEligibility, {}}},
          .expected_debug_mode_settings_check = false,
          .call_enable_debug_mode = false,
          .expected_debug_mode =
              AggregatableReportSharedInfo::DebugMode::kDisabled,
      }};

  for (auto& test_case : kTestCases) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        test_case.enabled_features, /*disabled_features=*/{});

    base::HistogramTester histogram;
    MockPrivateAggregationContentBrowserClient browser_client;
    ScopedContentBrowserClientSetting setting(&browser_client);

    const url::Origin kExampleOrigin =
        url::Origin::Create(GURL("https://example.com"));
    const url::Origin kMainFrameOrigin =
        url::Origin::Create(GURL("https://main_frame.com"));

    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote.BindNewPipeAndPassReceiver()));

    std::optional<AggregatableReportRequest> validated_request;
    EXPECT_CALL(mock_callback_,
                Run(_, _,
                    Property(&PrivateAggregationBudgetKey::api,
                             PrivateAggregationCallerApi::kProtectedAudience),
                    BudgetDeniedBehavior::kDontSendReport))
        .WillOnce(GenerateAndSaveReportRequest(&validated_request));

    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
    remote->ContributeToHistogram(std::move(contributions));
    if (test_case.call_enable_debug_mode) {
      remote->EnableDebugMode(/*debug_key=*/nullptr);
    }

    EXPECT_CALL(browser_client, IsPrivateAggregationAllowed(_, kMainFrameOrigin,
                                                            kExampleOrigin, _))
        .WillRepeatedly(testing::Return(true));

    if (test_case.expected_debug_mode_settings_check) {
      EXPECT_CALL(browser_client, IsPrivateAggregationDebugModeAllowed(
                                      _, kMainFrameOrigin, kExampleOrigin))
          .WillOnce(
              testing::Return(test_case.approve_debug_mode_settings_check));
    } else {
      EXPECT_CALL(browser_client, IsPrivateAggregationDebugModeAllowed(
                                      _, kMainFrameOrigin, kExampleOrigin))
          .Times(0);
    }

    remote.reset();
    host_->FlushReceiverSetForTesting();
    ASSERT_TRUE(validated_request);
    EXPECT_EQ(validated_request->shared_info().debug_mode,
              test_case.expected_debug_mode);

    histogram.ExpectUniqueSample(
        kPipeResultHistogram,
        PrivateAggregationHost::PipeResult::kReportSuccess, 1);
  }
}

TEST_F(PrivateAggregationHostTest, PipeClosedBeforeShutdown_NoHistogram) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  EXPECT_CALL(mock_callback_, Run).Times(0);

  base::HistogramTester histogram;

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  EXPECT_TRUE(remote.is_connected());
  remote.reset();
  host_->FlushReceiverSetForTesting();
  host_.reset();

  histogram.ExpectTotalCount(
      "PrivacySandbox.PrivateAggregation.Host.PipeOpenDurationOnShutdown", 0);
}

TEST_F(PrivateAggregationHostTest, PipeStillOpenAtShutdown_Histogram) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  EXPECT_CALL(mock_callback_, Run).Times(0);

  base::HistogramTester histogram;

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  task_environment_.FastForwardBy(base::Minutes(10));

  EXPECT_TRUE(remote.is_connected());
  host_.reset();

  histogram.ExpectUniqueTimeSample(
      "PrivacySandbox.PrivateAggregation.Host.PipeOpenDurationOnShutdown",
      base::Minutes(10), 1);
}

TEST_F(PrivateAggregationHostTest, TimeoutBeforeDisconnect) {
  // Set the start time to be "on the minute".
  base::Time on_the_minute_start_time =
      base::Time() +
      base::Time::Now().since_origin().CeilToMultiple(base::Minutes(1));
  task_environment_.FastForwardBy(on_the_minute_start_time - base::Time::Now());

  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  bool received_request = false;
  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior
                  budget_denied_behavior) {
            AggregatableReportRequest request =
                std::move(generator).Run(contributions);
            received_request = true;

            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(
                            testing::Pair("context_id", "example_context_id")));
            EXPECT_TRUE(request.payload_contents().contributions.empty());
            EXPECT_EQ(request.debug_key(), std::nullopt);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kDisabled);
            EXPECT_EQ(request.shared_info().scheduled_report_time,
                      on_the_minute_start_time + base::Minutes(1) +
                          PrivateAggregationHost::kTimeForLocalProcessing);
            EXPECT_EQ(budget_key.time_window().start_time(),
                      on_the_minute_start_time + base::Minutes(1));
          }));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  remote->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/1234u));

  task_environment_.FastForwardBy(base::Seconds(59));
  ASSERT_FALSE(received_request);
  EXPECT_TRUE(remote.is_connected());

  // At the timeout time, the reports are sent and the pipe is closed.
  task_environment_.FastForwardBy(base::Seconds(1));
  ASSERT_TRUE(received_request);
  EXPECT_FALSE(remote.is_connected());

  remote.reset();
  host_->FlushReceiverSetForTesting();

  histogram.ExpectUniqueSample(
      kTimeoutResultHistogram,
      PrivateAggregationHost::TimeoutResult::kOccurredBeforeRemoteDisconnection,
      1);
}

TEST_F(PrivateAggregationHostTest, TimeoutAfterDisconnect) {
  // Set the start time to be "on the minute".
  base::Time on_the_minute_start_time =
      base::Time() +
      base::Time::Now().since_origin().CeilToMultiple(base::Minutes(1));
  task_environment_.FastForwardBy(on_the_minute_start_time - base::Time::Now());

  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  bool received_request = false;
  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior
                  budget_denied_behavior) {
            AggregatableReportRequest request =
                std::move(generator).Run(contributions);
            received_request = true;

            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(
                            testing::Pair("context_id", "example_context_id")));
            EXPECT_TRUE(request.payload_contents().contributions.empty());
            EXPECT_EQ(request.debug_key(), std::nullopt);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kDisabled);

            // `request` should have report scheduled 1s from now.
            CHECK_EQ(base::Time::Now() + base::Seconds(1),
                     on_the_minute_start_time + base::Minutes(1));
            EXPECT_EQ(request.shared_info().scheduled_report_time,
                      on_the_minute_start_time + base::Minutes(1) +
                          PrivateAggregationHost::kTimeForLocalProcessing);

            // The start time for budgeting should be based off the current
            // time, instead of the desired timeout time.
            EXPECT_EQ(budget_key.time_window().start_time(),
                      on_the_minute_start_time);
          }));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  remote->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/1234u));

  task_environment_.FastForwardBy(base::Seconds(59));
  ASSERT_FALSE(received_request);
  EXPECT_TRUE(remote.is_connected());

  remote.reset();
  host_->FlushReceiverSetForTesting();

  ASSERT_TRUE(received_request);

  histogram.ExpectUniqueSample(
      kTimeoutResultHistogram,
      PrivateAggregationHost::TimeoutResult::kOccurredAfterRemoteDisconnection,
      1);
}

// Test the scenario that the disconnect happens before the timer fires, but we
// find the remaining time is negative. This can happen if enough time passes
// between the disconnect and the point when we compute the remaining time that
// the current time exceeds the original timer deadline. Viewed as a timeline:
//
//        T1                    T2                    T3
//    ----|---------------------|---------------------|--------------->
//    Disconnect           Timer deadline      Compute remaining time
//
TEST_F(PrivateAggregationHostTest,
       TimeoutAfterDisconnectTimeRemainingNegative) {
  // Set the start time to be "on the minute".
  base::Time on_the_minute_start_time =
      base::Time() +
      base::Time::Now().since_origin().CeilToMultiple(base::Minutes(1));
  task_environment_.FastForwardBy(on_the_minute_start_time - base::Time::Now());

  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  bool received_request = false;
  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              std::vector<blink::mojom::AggregatableReportHistogramContribution>
                  contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationBudgeter::BudgetDeniedBehavior
                  budget_denied_behavior) {
            AggregatableReportRequest request =
                std::move(generator).Run(contributions);
            received_request = true;

            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(
                            testing::Pair("context_id", "example_context_id")));
            EXPECT_TRUE(request.payload_contents().contributions.empty());
            EXPECT_EQ(request.debug_key(), std::nullopt);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kDisabled);

            // `request` should have report scheduled now (with some additional
            // buffer for local processing).
            CHECK_EQ(base::Time::Now(),
                     on_the_minute_start_time + base::Seconds(61));
            EXPECT_EQ(request.shared_info().scheduled_report_time,
                      on_the_minute_start_time + base::Seconds(61) +
                          PrivateAggregationHost::kTimeForLocalProcessing);

            // The start time for budgeting should be based off the current
            // time, instead of the desired timeout time.
            EXPECT_EQ(budget_key.time_window().start_time(),
                      base::Time::Now() - base::Seconds(1));
          }));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  CHECK_EQ(base::Time::Now(), on_the_minute_start_time);

  // Run pending tasks by fast-forwarding. The timer will not fire because its
  // desired run time will still be in the future.
  task_environment_.FastForwardBy(base::Seconds(59));

  // Without giving the timer a chance to fire, advance the clock past its
  // desired run time.
  task_environment_.AdvanceClock(base::Seconds(2));

  ASSERT_FALSE(received_request);
  EXPECT_TRUE(remote.is_connected());

  remote.reset();
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_request);

  CHECK_EQ(base::Time::Now(), on_the_minute_start_time + base::Seconds(61));

  histogram.ExpectUniqueSample(
      kTimeoutResultHistogram,
      PrivateAggregationHost::TimeoutResult::kOccurredAfterRemoteDisconnection,
      1);
}

TEST_F(PrivateAggregationHostTest, TimeoutBeforeDisconnectForTwoHosts) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote1;
  mojo::Remote<blink::mojom::PrivateAggregationHost> remote2;
  std::optional<AggregatableReportRequest> validated_request1;
  std::optional<AggregatableReportRequest> validated_request2;

  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(GenerateAndSaveReportRequest(&validated_request1))
      .WillOnce(GenerateAndSaveReportRequest(&validated_request2));

  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote1.BindNewPipeAndPassReceiver()));

  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Seconds(61),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote2.BindNewPipeAndPassReceiver()));

  remote1->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/1234u));

  task_environment_.FastForwardBy(base::Seconds(59));
  ASSERT_FALSE(validated_request1.has_value());
  ASSERT_FALSE(validated_request2.has_value());

  remote2->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/1234u));

  // Timeout reached for remote1.
  task_environment_.FastForwardBy(base::Seconds(1));
  ASSERT_TRUE(validated_request1.has_value());
  ASSERT_FALSE(validated_request2.has_value());
  EXPECT_FALSE(remote1.is_connected());
  EXPECT_TRUE(remote2.is_connected());

  EXPECT_THAT(
      validated_request1->additional_fields(),
      testing::ElementsAre(testing::Pair("context_id", "example_context_id")));
  EXPECT_TRUE(validated_request1->payload_contents().contributions.empty());
  EXPECT_EQ(validated_request1->debug_key(), std::nullopt);
  EXPECT_EQ(validated_request1->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kDisabled);
  EXPECT_EQ(
      validated_request1->shared_info().scheduled_report_time,
      base::Time::Now() + PrivateAggregationHost::kTimeForLocalProcessing);

  // Timeout reached for remote2.
  task_environment_.FastForwardBy(base::Seconds(1));
  ASSERT_TRUE(validated_request2.has_value());
  EXPECT_FALSE(remote2.is_connected());

  EXPECT_THAT(
      validated_request2->additional_fields(),
      testing::ElementsAre(testing::Pair("context_id", "example_context_id")));
  EXPECT_TRUE(validated_request2->payload_contents().contributions.empty());
  EXPECT_EQ(validated_request2->debug_key(), std::nullopt);
  EXPECT_EQ(validated_request2->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kDisabled);
  EXPECT_EQ(
      validated_request2->shared_info().scheduled_report_time,
      base::Time::Now() + PrivateAggregationHost::kTimeForLocalProcessing);

  remote1.reset();
  remote2.reset();
  host_->FlushReceiverSetForTesting();

  histogram.ExpectUniqueSample(
      kTimeoutResultHistogram,
      PrivateAggregationHost::TimeoutResult::kOccurredBeforeRemoteDisconnection,
      2);
}

TEST_F(PrivateAggregationHostTest, TimeoutAfterDisconnectForTwoHosts) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote1;
  mojo::Remote<blink::mojom::PrivateAggregationHost> remote2;
  std::optional<AggregatableReportRequest> validated_request1;
  std::optional<AggregatableReportRequest> validated_request2;

  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(GenerateAndSaveReportRequest(&validated_request1))
      .WillOnce(GenerateAndSaveReportRequest(&validated_request2));

  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote1.BindNewPipeAndPassReceiver()));

  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Seconds(61),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote2.BindNewPipeAndPassReceiver()));

  remote1->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/1234u));
  remote2->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/1234u));

  task_environment_.FastForwardBy(base::Seconds(59));
  ASSERT_FALSE(validated_request1.has_value());
  ASSERT_FALSE(validated_request2.has_value());

  remote1.reset();
  remote2.reset();
  host_->FlushReceiverSetForTesting();

  ASSERT_TRUE(validated_request1.has_value());
  ASSERT_TRUE(validated_request2.has_value());

  // `validated_request1` should have report scheduled 1s from now.
  EXPECT_THAT(
      validated_request1->additional_fields(),
      testing::ElementsAre(testing::Pair("context_id", "example_context_id")));
  EXPECT_TRUE(validated_request1->payload_contents().contributions.empty());
  EXPECT_EQ(validated_request1->debug_key(), std::nullopt);
  EXPECT_EQ(validated_request1->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kDisabled);
  EXPECT_EQ(validated_request1->shared_info().scheduled_report_time,
            base::Time::Now() + base::Seconds(1) +
                PrivateAggregationHost::kTimeForLocalProcessing);

  // `validated_request2` should have report scheduled 2s from now.
  EXPECT_THAT(
      validated_request2->additional_fields(),
      testing::ElementsAre(testing::Pair("context_id", "example_context_id")));
  EXPECT_TRUE(validated_request2->payload_contents().contributions.empty());
  EXPECT_EQ(validated_request2->debug_key(), std::nullopt);
  EXPECT_EQ(validated_request2->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kDisabled);
  EXPECT_EQ(validated_request2->shared_info().scheduled_report_time,
            base::Time::Now() + base::Seconds(2) +
                PrivateAggregationHost::kTimeForLocalProcessing);

  histogram.ExpectUniqueSample(
      kTimeoutResultHistogram,
      PrivateAggregationHost::TimeoutResult::kOccurredAfterRemoteDisconnection,
      2);
}

TEST_F(PrivateAggregationHostTest, TimeoutCanceledDueToError) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  remote->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/1234u));

  // This second call to EnableDebugMode() will fail the mojom validation.
  remote->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/5678u));

  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());

  host_.reset();

  histogram.ExpectUniqueSample(
      kTimeoutResultHistogram,
      PrivateAggregationHost::TimeoutResult::kCanceledDueToError, 1);
}

TEST_F(PrivateAggregationHostTest,
       TimeoutStillScheduledOnShutdownWithPipeOpen) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  remote->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/1234u));

  host_->FlushReceiverSetForTesting();
  EXPECT_TRUE(remote.is_connected());

  host_.reset();

  histogram.ExpectUniqueSample(
      kTimeoutResultHistogram,
      PrivateAggregationHost::TimeoutResult::kStillScheduledOnShutdown, 1);
}

TEST_F(PrivateAggregationHostTest,
       TimeoutStillScheduledOnShutdownWithPipeOpenForTwoHosts) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote1;
  mojo::Remote<blink::mojom::PrivateAggregationHost> remote2;

  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote1.BindNewPipeAndPassReceiver()));

  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Minutes(1),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote2.BindNewPipeAndPassReceiver()));

  remote1->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/1234u));
  remote2->EnableDebugMode(blink::mojom::DebugKey::New(/*value=*/1234u));

  task_environment_.FastForwardBy(base::Seconds(59));

  EXPECT_TRUE(remote1.is_connected());
  EXPECT_TRUE(remote2.is_connected());

  host_.reset();

  histogram.ExpectUniqueSample(
      kTimeoutResultHistogram,
      PrivateAggregationHost::TimeoutResult::kStillScheduledOnShutdown, 2);
}

class PrivateAggregationHostDeveloperModeTest
    : public PrivateAggregationHostTest {
 public:
  PrivateAggregationHostDeveloperModeTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kPrivateAggregationDeveloperMode);
  }
};

TEST_F(PrivateAggregationHostDeveloperModeTest,
       ContributeToHistogram_ScheduledReportTimeIsNotDelayed) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  std::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_,
              Run(_, _,
                  Property(&PrivateAggregationBudgetKey::api,
                           PrivateAggregationCallerApi::kProtectedAudience),
                  BudgetDeniedBehavior::kDontSendReport))
      .WillOnce(GenerateAndSaveReportRequest(&validated_request));

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
  remote->ContributeToHistogram(std::move(contributions));
  remote.reset();
  host_->FlushReceiverSetForTesting();

  ASSERT_TRUE(validated_request);

  // We're using `MOCK_TIME` so we can be sure no time has advanced.
  EXPECT_EQ(
      validated_request->shared_info().scheduled_report_time,
      base::Time::Now() + PrivateAggregationHost::kTimeForLocalProcessing);
}

TEST_F(PrivateAggregationHostDeveloperModeTest,
       TimeoutSet_ScheduledReportTimeIsAtTimeout) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOrigin, kMainFrameOrigin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/"example_context_id", /*timeout=*/base::Seconds(30),
      /*aggregation_coordinator_origin=*/std::nullopt,
      PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
      remote.BindNewPipeAndPassReceiver()));

  std::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_,
              Run(_, _,
                  Property(&PrivateAggregationBudgetKey::api,
                           PrivateAggregationCallerApi::kProtectedAudience),
                  BudgetDeniedBehavior::kSendNullReport))
      .WillOnce(GenerateAndSaveReportRequest(&validated_request));

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456, /*filtering_id=*/std::nullopt));
  remote->ContributeToHistogram(std::move(contributions));
  remote.reset();
  host_->FlushReceiverSetForTesting();

  ASSERT_TRUE(validated_request);

  // We're using `MOCK_TIME` so we can be sure no time has advanced.
  EXPECT_EQ(validated_request->shared_info().scheduled_report_time,
            base::Time::Now() + base::Seconds(30) +
                PrivateAggregationHost::kTimeForLocalProcessing);
}

}  // namespace

}  // namespace content
