// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_host.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using testing::_;
using testing::Invoke;
using testing::Property;

constexpr char kSendHistogramReportResultHistogram[] =
    "PrivacySandbox.PrivateAggregation.Host.SendHistogramReportResult";

class PrivateAggregationHostTest : public testing::Test {
 public:
  PrivateAggregationHostTest() = default;

  void SetUp() override {
    host_ = std::make_unique<PrivateAggregationHost>(
        /*on_report_request_received=*/mock_callback_.Get(),
        /*browser_context=*/&test_browser_context_);
  }

  void TearDown() override { host_.reset(); }

 protected:
  base::MockRepeatingCallback<void(AggregatableReportRequest,
                                   PrivateAggregationBudgetKey)>
      mock_callback_;
  std::unique_ptr<PrivateAggregationHost> host_;

 private:
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestBrowserContext test_browser_context_;
};

TEST_F(PrivateAggregationHostTest,
       SendHistogramReport_ReportRequestHasCorrectMembers) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     /*context_id=*/absl::nullopt,
                                     remote.BindNewPipeAndPassReceiver()));

  absl::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_,
              Run(_, Property(&PrivateAggregationBudgetKey::api,
                              PrivateAggregationBudgetKey::Api::kFledge)))
      .WillOnce(MoveArg<0>(&validated_request));

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456));
  remote->SendHistogramReport(std::move(contributions),
                              blink::mojom::AggregationServiceMode::kDefault,
                              blink::mojom::DebugModeDetails::New());

  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());

  ASSERT_TRUE(validated_request);

  // We only do some basic validation for the scheduled report time and report
  // ID as they are not deterministic and will be copied to `expected_request`.
  // We're using `MOCK_TIME` so we can be sure no time has advanced.
  base::Time now = base::Time::Now();
  EXPECT_GE(validated_request->shared_info().scheduled_report_time,
            now + base::Minutes(10));
  EXPECT_LE(validated_request->shared_info().scheduled_report_time,
            now + base::Hours(1));
  EXPECT_TRUE(validated_request->shared_info().report_id.is_valid());

  absl::optional<AggregatableReportRequest> expected_request =
      AggregatableReportRequest::Create(
          AggregationServicePayloadContents(
              AggregationServicePayloadContents::Operation::kHistogram,
              {blink::mojom::AggregatableReportHistogramContribution(
                  /*bucket=*/123, /*value=*/456)},
              blink::mojom::AggregationServiceMode::kDefault,
              ::aggregation_service::mojom::AggregationCoordinator::kDefault),
          AggregatableReportSharedInfo(
              validated_request->shared_info().scheduled_report_time,
              validated_request->shared_info().report_id,
              /*reporting_origin=*/kExampleOrigin,
              AggregatableReportSharedInfo::DebugMode::kDisabled,
              /*additional_fields=*/base::Value::Dict(),
              /*api_version=*/"0.1",
              /*api_identifier=*/"fledge"),
          /*reporting_path=*/"/.well-known/private-aggregation/report-fledge");
  ASSERT_TRUE(expected_request);

  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      validated_request.value(), expected_request.value()));

  histogram.ExpectUniqueSample(
      kSendHistogramReportResultHistogram,
      PrivateAggregationHost::SendHistogramReportResult::kSuccess, 1);
}

TEST_F(PrivateAggregationHostTest, ApiDiffers_RequestUpdatesCorrectly) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  const PrivateAggregationBudgetKey::Api apis[] = {
      PrivateAggregationBudgetKey::Api::kFledge,
      PrivateAggregationBudgetKey::Api::kSharedStorage};

  std::vector<mojo::Remote<blink::mojom::PrivateAggregationHost>> remotes{
      /*n=*/2};
  std::vector<absl::optional<AggregatableReportRequest>> validated_requests{
      /*n=*/2};

  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin, apis[i], /*context_id=*/absl::nullopt,
        remotes[i].BindNewPipeAndPassReceiver()));
    EXPECT_CALL(mock_callback_,
                Run(_, Property(&PrivateAggregationBudgetKey::api, apis[i])))
        .WillOnce(MoveArg<0>(&validated_requests[i]));

    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/456));
    remotes[i]->SendHistogramReport(
        std::move(contributions),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New());

    remotes[i].FlushForTesting();
    EXPECT_TRUE(remotes[i].is_connected());

    ASSERT_TRUE(validated_requests[i]);
  }

  EXPECT_EQ(validated_requests[0]->reporting_path(),
            "/.well-known/private-aggregation/report-fledge");
  EXPECT_EQ(validated_requests[1]->reporting_path(),
            "/.well-known/private-aggregation/report-shared-storage");

  EXPECT_EQ(validated_requests[0]->shared_info().api_identifier, "fledge");
  EXPECT_EQ(validated_requests[1]->shared_info().api_identifier,
            "shared-storage");

  histogram.ExpectUniqueSample(
      kSendHistogramReportResultHistogram,
      PrivateAggregationHost::SendHistogramReportResult::kSuccess, 2);
}

TEST_F(PrivateAggregationHostTest, DebugModeDetails_ReflectedInReport) {
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

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     /*context_id=*/absl::nullopt,
                                     remote.BindNewPipeAndPassReceiver()));

  std::vector<absl::optional<AggregatableReportRequest>> validated_requests{
      /*n=*/3};
  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(MoveArg<0>(&validated_requests[0]))
      .WillOnce(MoveArg<0>(&validated_requests[1]))
      .WillOnce(MoveArg<0>(&validated_requests[2]));

  for (auto& debug_mode_details_arg : debug_mode_details_args) {
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/456));
    remote->SendHistogramReport(std::move(contributions),
                                blink::mojom::AggregationServiceMode::kDefault,
                                debug_mode_details_arg->Clone());
  }

  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());

  ASSERT_TRUE(validated_requests[0].has_value());
  ASSERT_TRUE(validated_requests[1].has_value());
  ASSERT_TRUE(validated_requests[2].has_value());

  EXPECT_EQ(validated_requests[0]->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kDisabled);
  EXPECT_EQ(validated_requests[1]->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kEnabled);
  EXPECT_EQ(validated_requests[2]->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kEnabled);

  EXPECT_EQ(validated_requests[0]->debug_key(), absl::nullopt);
  EXPECT_EQ(validated_requests[1]->debug_key(), absl::nullopt);
  EXPECT_EQ(validated_requests[2]->debug_key(), 1234u);

  histogram.ExpectUniqueSample(
      kSendHistogramReportResultHistogram,
      PrivateAggregationHost::SendHistogramReportResult::kSuccess, 3);
}

TEST_F(PrivateAggregationHostTest,
       MultipleReceievers_SendHistogramReportCallsRoutedCorrectly) {
  base::HistogramTester histogram;

  const url::Origin kExampleOriginA =
      url::Origin::Create(GURL("https://a.example"));
  const url::Origin kExampleOriginB =
      url::Origin::Create(GURL("https://b.example"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  std::vector<mojo::Remote<blink::mojom::PrivateAggregationHost>> remotes(
      /*n=*/4);

  EXPECT_TRUE(host_->BindNewReceiver(kExampleOriginA, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     /*context_id=*/absl::nullopt,
                                     remotes[0].BindNewPipeAndPassReceiver()));
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOriginB, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     /*context_id=*/absl::nullopt,
                                     remotes[1].BindNewPipeAndPassReceiver()));
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOriginA, kMainFrameOrigin,
      PrivateAggregationBudgetKey::Api::kSharedStorage,
      /*context_id=*/absl::nullopt, remotes[2].BindNewPipeAndPassReceiver()));
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOriginB, kMainFrameOrigin,
      PrivateAggregationBudgetKey::Api::kSharedStorage,
      /*context_id=*/absl::nullopt, remotes[3].BindNewPipeAndPassReceiver()));

  // Use the bucket as a sentinel to ensure that calls were routed correctly.
  EXPECT_CALL(mock_callback_,
              Run(_, Property(&PrivateAggregationBudgetKey::api,
                              PrivateAggregationBudgetKey::Api::kFledge)))
      .WillOnce(
          Invoke([&kExampleOriginB](AggregatableReportRequest request,
                                    PrivateAggregationBudgetKey budget_key) {
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.shared_info().reporting_origin, kExampleOriginB);
            EXPECT_EQ(budget_key.origin(), kExampleOriginB);
          }));

  EXPECT_CALL(
      mock_callback_,
      Run(_, Property(&PrivateAggregationBudgetKey::api,
                      PrivateAggregationBudgetKey::Api::kSharedStorage)))
      .WillOnce(
          Invoke([&kExampleOriginA](AggregatableReportRequest request,
                                    PrivateAggregationBudgetKey budget_key) {
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, kExampleOriginA);
            EXPECT_EQ(budget_key.origin(), kExampleOriginA);
          }));

  {
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/1, /*value=*/123));
    remotes[1]->SendHistogramReport(
        std::move(contributions),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New());
  }

  {
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/2, /*value=*/123));
    remotes[2]->SendHistogramReport(
        std::move(contributions),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New());
  }

  for (auto& remote : remotes) {
    remote.FlushForTesting();
    EXPECT_TRUE(remote.is_connected());
  }

  histogram.ExpectUniqueSample(
      kSendHistogramReportResultHistogram,
      PrivateAggregationHost::SendHistogramReportResult::kSuccess, 2);
}

TEST_F(PrivateAggregationHostTest, BindUntrustworthyOriginReceiver_Fails) {
  base::HistogramTester histogram;

  const url::Origin kInsecureOrigin =
      url::Origin::Create(GURL("http://example.com"));
  const url::Origin kOpaqueOrigin;
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote_1;
  EXPECT_FALSE(host_->BindNewReceiver(kInsecureOrigin, kMainFrameOrigin,
                                      PrivateAggregationBudgetKey::Api::kFledge,
                                      /*context_id=*/absl::nullopt,
                                      remote_1.BindNewPipeAndPassReceiver()));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote_2;
  EXPECT_FALSE(host_->BindNewReceiver(kOpaqueOrigin, kMainFrameOrigin,
                                      PrivateAggregationBudgetKey::Api::kFledge,
                                      /*context_id=*/absl::nullopt,
                                      remote_2.BindNewPipeAndPassReceiver()));

  // Attempt to send a message to an unconnected remote. The request should
  // not be processed.
  EXPECT_CALL(mock_callback_, Run(_, _)).Times(0);
  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456));
  remote_1->SendHistogramReport(std::move(contributions),
                                blink::mojom::AggregationServiceMode::kDefault,
                                blink::mojom::DebugModeDetails::New());

  // Flush to ensure disconnection and the SendHistogramReport call have had
  // time to be processed.
  remote_1.FlushForTesting();
  remote_2.FlushForTesting();
  EXPECT_FALSE(remote_1.is_connected());
  EXPECT_FALSE(remote_2.is_connected());

  histogram.ExpectTotalCount(kSendHistogramReportResultHistogram, 0);
}

TEST_F(PrivateAggregationHostTest, BindReceiverWithTooLongContextid_Fails) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  const std::string kTooLongContextId =
      "this_is_an_example_of_a_context_id_that_is_too_long_to_be_allowed";

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_FALSE(host_->BindNewReceiver(kExampleOrigin, kMainFrameOrigin,
                                      PrivateAggregationBudgetKey::Api::kFledge,
                                      kTooLongContextId,
                                      remote.BindNewPipeAndPassReceiver()));

  // Attempt to send a message to an unconnected remote. The request should
  // not be processed.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456));
  remote->SendHistogramReport(std::move(contributions),
                              blink::mojom::AggregationServiceMode::kDefault,
                              blink::mojom::DebugModeDetails::New());

  // Flush to ensure disconnection and the SendHistogramReport call have had
  // time to be processed.
  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());

  histogram.ExpectTotalCount(kSendHistogramReportResultHistogram, 0);
}

TEST_F(PrivateAggregationHostTest, InvalidRequest_Rejected) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     /*context_id=*/absl::nullopt,
                                     remote.BindNewPipeAndPassReceiver()));

  // Negative values are invalid
  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      negative_contributions;
  negative_contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/-1));

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      valid_contributions;
  valid_contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456));

  EXPECT_CALL(mock_callback_, Run(_, _)).Times(0);

  {
    base::HistogramTester histogram;
    remote->SendHistogramReport(std::move(negative_contributions),
                                blink::mojom::AggregationServiceMode::kDefault,
                                blink::mojom::DebugModeDetails::New());
    remote.FlushForTesting();
    histogram.ExpectUniqueSample(
        kSendHistogramReportResultHistogram,
        PrivateAggregationHost::SendHistogramReportResult::kNegativeValue, 1);
  }
  {
    base::HistogramTester histogram;

    remote->SendHistogramReport(
        std::move(valid_contributions),
        blink::mojom::AggregationServiceMode::kDefault,
        // Debug mode must be enabled for a debug key to be set.
        blink::mojom::DebugModeDetails::New(
            /*is_enabled=*/false,
            /*debug_key=*/blink::mojom::DebugKey::New(1234u)));
    remote.FlushForTesting();
    histogram.ExpectUniqueSample(
        kSendHistogramReportResultHistogram,
        PrivateAggregationHost::SendHistogramReportResult::
            kDebugKeyPresentWithoutDebugMode,
        1);
  }
}

TEST_F(PrivateAggregationHostTest, TooManyContributions_Truncated) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     /*context_id=*/absl::nullopt,
                                     remote.BindNewPipeAndPassReceiver()));
  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      too_many_contributions;
  for (int i = 0; i < PrivateAggregationHost::kMaxNumberOfContributions + 1;
       ++i) {
    too_many_contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/1));
  }

  base::HistogramTester histogram;

  absl::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_, Run).WillOnce(MoveArg<0>(&validated_request));

  remote->SendHistogramReport(std::move(too_many_contributions),
                              blink::mojom::AggregationServiceMode::kDefault,
                              blink::mojom::DebugModeDetails::New());
  remote.FlushForTesting();
  histogram.ExpectUniqueSample(
      kSendHistogramReportResultHistogram,
      PrivateAggregationHost::SendHistogramReportResult::
          kSuccessButTruncatedDueToTooManyContributions,
      1);

  ASSERT_TRUE(validated_request);
  EXPECT_EQ(
      validated_request->payload_contents().contributions.size(),
      static_cast<size_t>(PrivateAggregationHost::kMaxNumberOfContributions));
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
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     /*context_id=*/absl::nullopt,
                                     remote.BindNewPipeAndPassReceiver()));

  // If the API is enabled, the call should succeed.
  EXPECT_CALL(browser_client,
              IsPrivateAggregationAllowed(_, kMainFrameOrigin, kExampleOrigin))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_callback_, Run);

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456));
  remote->SendHistogramReport(std::move(contributions),
                              blink::mojom::AggregationServiceMode::kDefault,
                              blink::mojom::DebugModeDetails::New());

  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());

  histogram.ExpectUniqueSample(
      kSendHistogramReportResultHistogram,
      PrivateAggregationHost::SendHistogramReportResult::kSuccess, 1);
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
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     /*context_id=*/absl::nullopt,
                                     remote.BindNewPipeAndPassReceiver()));

  // If the API is enabled, the call should succeed.
  EXPECT_CALL(browser_client,
              IsPrivateAggregationAllowed(_, kMainFrameOrigin, kExampleOrigin))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(mock_callback_, Run).Times(0);

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456));
  remote->SendHistogramReport(std::move(contributions),
                              blink::mojom::AggregationServiceMode::kDefault,
                              blink::mojom::DebugModeDetails::New());

  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());

  histogram.ExpectUniqueSample(
      kSendHistogramReportResultHistogram,
      PrivateAggregationHost::SendHistogramReportResult::kApiDisabledInSettings,
      1);
}

TEST_F(PrivateAggregationHostTest, ContextIdSet_ReflectedInSingleReport) {
  base::HistogramTester histogram;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     "example_context_id",
                                     remote.BindNewPipeAndPassReceiver()));

  absl::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_, Run).WillOnce(MoveArg<0>(&validated_request));

  // Setting the debug details has no effect if a standard report is sent.
  remote->SetDebugModeDetailsOnNullReport(blink::mojom::DebugModeDetails::New(
      /*is_enabled=*/true,
      /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/1234u)));
  {
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/456));
    remote->SendHistogramReport(std::move(contributions),
                                blink::mojom::AggregationServiceMode::kDefault,
                                blink::mojom::DebugModeDetails::New());
  }

  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());

  ASSERT_TRUE(validated_request.has_value());

  EXPECT_THAT(
      validated_request->additional_fields(),
      testing::ElementsAre(testing::Pair("context_id", "example_context_id")));

  histogram.ExpectUniqueSample(
      kSendHistogramReportResultHistogram,
      PrivateAggregationHost::SendHistogramReportResult::kSuccess, 1);

  // Reusing the pipe is not allowed
  EXPECT_CALL(mock_callback_, Run).Times(0);
  {
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/456));
    remote->SendHistogramReport(std::move(contributions),
                                blink::mojom::AggregationServiceMode::kDefault,
                                blink::mojom::DebugModeDetails::New());
  }
  remote.FlushForTesting();

  // Expect just this one additional histogram.
  histogram.ExpectBucketCount(
      kSendHistogramReportResultHistogram,
      PrivateAggregationHost::SendHistogramReportResult::
          kPipeWithContextIdReused,
      1u);
  histogram.ExpectTotalCount(kSendHistogramReportResultHistogram, 2u);
}

TEST_F(PrivateAggregationHostTest,
       ContextIdSetPipeNotUsed_NullReportSentWithSetDebugModeDetails) {
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

  std::vector<absl::optional<AggregatableReportRequest>> validated_requests{
      /*n=*/4};
  EXPECT_CALL(mock_callback_, Run)
      .WillOnce(MoveArg<0>(&validated_requests[0]))
      .WillOnce(MoveArg<0>(&validated_requests[1]))
      .WillOnce(MoveArg<0>(&validated_requests[2]))
      .WillOnce(MoveArg<0>(&validated_requests[3]));
  for (auto& debug_mode_details_arg : debug_mode_details_args) {
    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationBudgetKey::Api::kFledge, "example_context_id",
        remote.BindNewPipeAndPassReceiver()));

    remote->SetDebugModeDetailsOnNullReport(std::move(debug_mode_details_arg));

    EXPECT_TRUE(remote.is_connected());
    remote.reset();
    host_->FlushReceiverSetForTesting();
  }

  {
    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationBudgetKey::Api::kFledge, "example_context_id",
        remote.BindNewPipeAndPassReceiver()));

    // While it is expected that SetDebugModeDetailsOnNullReport() be called, a
    // null report should still be sent if it isn't.

    EXPECT_TRUE(remote.is_connected());
    remote.reset();
    host_->FlushReceiverSetForTesting();
  }

  for (absl::optional<AggregatableReportRequest>& validated_request :
       validated_requests) {
    ASSERT_TRUE(validated_request.has_value());
    EXPECT_THAT(validated_request->additional_fields(),
                testing::ElementsAre(
                    testing::Pair("context_id", "example_context_id")));
    ASSERT_EQ(validated_request->payload_contents().contributions.size(), 1u);
    EXPECT_EQ(validated_request->payload_contents().contributions[0].bucket,
              0u);
    EXPECT_EQ(validated_request->payload_contents().contributions[0].value, 0);
  }

  EXPECT_EQ(validated_requests[0]->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kDisabled);
  EXPECT_EQ(validated_requests[1]->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kEnabled);
  EXPECT_EQ(validated_requests[2]->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kEnabled);
  EXPECT_EQ(validated_requests[3]->shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kDisabled);

  EXPECT_EQ(validated_requests[0]->debug_key(), absl::nullopt);
  EXPECT_EQ(validated_requests[1]->debug_key(), absl::nullopt);
  EXPECT_EQ(validated_requests[2]->debug_key(), 1234u);
  EXPECT_EQ(validated_requests[3]->debug_key(), absl::nullopt);
}

TEST_F(PrivateAggregationHostTest, ContextIdNotSet_NoNullReportSent) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  EXPECT_CALL(mock_callback_, Run).Times(0);

  {
    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationBudgetKey::Api::kFledge,
        /*context_id=*/absl::nullopt, remote.BindNewPipeAndPassReceiver()));

    EXPECT_TRUE(remote.is_connected());
    remote.reset();
    host_->FlushReceiverSetForTesting();
  }

  {
    mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, kMainFrameOrigin,
        PrivateAggregationBudgetKey::Api::kFledge,
        /*context_id=*/absl::nullopt, remote.BindNewPipeAndPassReceiver()));

    // Setting the debug details has no effect.
    remote->SetDebugModeDetailsOnNullReport(blink::mojom::DebugModeDetails::New(
        /*is_enabled=*/true,
        /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/1234u)));

    EXPECT_TRUE(remote.is_connected());
    remote.reset();
    host_->FlushReceiverSetForTesting();
  }
}

TEST_F(PrivateAggregationHostTest,
       MultipleSetDebugModeDetailsOnNullReportCalls_OnlyFirstHasEffect) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     "example_context_id",
                                     remote.BindNewPipeAndPassReceiver()));

  absl::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_, Run).WillOnce(MoveArg<0>(&validated_request));

  remote->SetDebugModeDetailsOnNullReport(blink::mojom::DebugModeDetails::New(
      /*is_enabled=*/true,
      /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/1234u)));
  remote->SetDebugModeDetailsOnNullReport(blink::mojom::DebugModeDetails::New(
      /*is_enabled=*/true,
      /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/2345u)));

  EXPECT_TRUE(remote.is_connected());
  remote.reset();
  host_->FlushReceiverSetForTesting();

  ASSERT_TRUE(validated_request.has_value());

  EXPECT_THAT(
      validated_request->additional_fields(),
      testing::ElementsAre(testing::Pair("context_id", "example_context_id")));

  // Only the first call should have had an effect.
  EXPECT_EQ(validated_request->debug_key(), 1234u);
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
       SendHistogramReport_ScheduledReportTimeIsNotDelayed) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main_frame.com"));

  mojo::Remote<blink::mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin, kMainFrameOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     /*context_id=*/absl::nullopt,
                                     remote.BindNewPipeAndPassReceiver()));

  absl::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_,
              Run(_, Property(&PrivateAggregationBudgetKey::api,
                              PrivateAggregationBudgetKey::Api::kFledge)))
      .WillOnce(MoveArg<0>(&validated_request));

  std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      blink::mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/456));
  remote->SendHistogramReport(std::move(contributions),
                              blink::mojom::AggregationServiceMode::kDefault,
                              blink::mojom::DebugModeDetails::New());

  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());

  ASSERT_TRUE(validated_request);

  // We're using `MOCK_TIME` so we can be sure no time has advanced.
  EXPECT_EQ(validated_request->shared_info().scheduled_report_time,
            base::Time::Now());
}

}  // namespace

}  // namespace content
