// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_host.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using testing::_;
using testing::Invoke;
using testing::Property;

class PrivateAggregationHostTest : public testing::Test {
 public:
  PrivateAggregationHostTest() = default;

  void SetUp() override {
    host_ = std::make_unique<PrivateAggregationHost>(
        /*on_report_request_received=*/mock_callback_.Get());
  }

  void TearDown() override { host_.reset(); }

 protected:
  base::MockRepeatingCallback<void(AggregatableReportRequest,
                                   PrivateAggregationBudgetKey)>
      mock_callback_;
  std::unique_ptr<PrivateAggregationHost> host_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

}  // namespace

TEST_F(PrivateAggregationHostTest,
       SendHistogramReport_ReportRequestHasCorrectMembers) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));

  mojo::Remote<mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     remote.BindNewPipeAndPassReceiver()));

  absl::optional<AggregatableReportRequest> validated_request;
  EXPECT_CALL(mock_callback_,
              Run(_, Property(&PrivateAggregationBudgetKey::api,
                              PrivateAggregationBudgetKey::Api::kFledge)))
      .WillOnce(MoveArg<0>(&validated_request));

  std::vector<mojom::AggregatableReportHistogramContributionPtr> contributions;
  contributions.push_back(mojom::AggregatableReportHistogramContribution::New(
      /*bucket=*/123, /*value=*/456));
  remote->SendHistogramReport(std::move(contributions),
                              mojom::AggregationServiceMode::kDefault);

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
              {mojom::AggregatableReportHistogramContribution(
                  /*bucket=*/123, /*value=*/456)},
              mojom::AggregationServiceMode::kDefault),
          AggregatableReportSharedInfo(
              validated_request->shared_info().scheduled_report_time,
              validated_request->shared_info().report_id,
              /*reporting_origin=*/kExampleOrigin,
              AggregatableReportSharedInfo::DebugMode::kDisabled,
              /*additional_fields=*/base::Value::Dict(),
              /*api_version=*/"0.1",
              /*api_identifier=*/"private-aggregation"),
          /*reporting_path=*/"/.well-known/private-aggregation/report-fledge");
  ASSERT_TRUE(expected_request);

  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      validated_request.value(), expected_request.value()));
}

TEST_F(PrivateAggregationHostTest, ReportingPath) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));

  const PrivateAggregationBudgetKey::Api apis[] = {
      PrivateAggregationBudgetKey::Api::kFledge,
      PrivateAggregationBudgetKey::Api::kSharedStorage};

  std::vector<mojo::Remote<mojom::PrivateAggregationHost>> remotes{/*n=*/2};
  std::vector<absl::optional<AggregatableReportRequest>> validated_requests{
      /*n=*/2};

  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(host_->BindNewReceiver(
        kExampleOrigin, apis[i], remotes[i].BindNewPipeAndPassReceiver()));
    EXPECT_CALL(mock_callback_,
                Run(_, Property(&PrivateAggregationBudgetKey::api, apis[i])))
        .WillOnce(MoveArg<0>(&validated_requests[i]));

    std::vector<mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(mojom::AggregatableReportHistogramContribution::New(
        /*bucket=*/123, /*value=*/456));
    remotes[i]->SendHistogramReport(std::move(contributions),
                                    mojom::AggregationServiceMode::kDefault);

    remotes[i].FlushForTesting();
    EXPECT_TRUE(remotes[i].is_connected());

    ASSERT_TRUE(validated_requests[i]);
  }

  EXPECT_EQ(validated_requests[0]->reporting_path(),
            "/.well-known/private-aggregation/report-fledge");
  EXPECT_EQ(validated_requests[1]->reporting_path(),
            "/.well-known/private-aggregation/report-shared-storage");
}

TEST_F(PrivateAggregationHostTest,
       MultipleReceievers_SendHistogramReportCallsRoutedCorrectly) {
  const url::Origin kExampleOriginA =
      url::Origin::Create(GURL("https://a.example"));
  const url::Origin kExampleOriginB =
      url::Origin::Create(GURL("https://b.example"));

  std::vector<mojo::Remote<mojom::PrivateAggregationHost>> remotes(/*n=*/4);

  EXPECT_TRUE(host_->BindNewReceiver(kExampleOriginA,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     remotes[0].BindNewPipeAndPassReceiver()));
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOriginB,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     remotes[1].BindNewPipeAndPassReceiver()));
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOriginA, PrivateAggregationBudgetKey::Api::kSharedStorage,
      remotes[2].BindNewPipeAndPassReceiver()));
  EXPECT_TRUE(host_->BindNewReceiver(
      kExampleOriginB, PrivateAggregationBudgetKey::Api::kSharedStorage,
      remotes[3].BindNewPipeAndPassReceiver()));

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
    std::vector<mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(mojom::AggregatableReportHistogramContribution::New(
        /*bucket=*/1, /*value=*/123));
    remotes[1]->SendHistogramReport(std::move(contributions),
                                    mojom::AggregationServiceMode::kDefault);
  }

  {
    std::vector<mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(mojom::AggregatableReportHistogramContribution::New(
        /*bucket=*/2, /*value=*/123));
    remotes[2]->SendHistogramReport(std::move(contributions),
                                    mojom::AggregationServiceMode::kDefault);
  }

  for (auto& remote : remotes) {
    remote.FlushForTesting();
    EXPECT_TRUE(remote.is_connected());
  }
}

TEST_F(PrivateAggregationHostTest, BindUntrustworthyOriginReceiver_Fails) {
  const url::Origin kInsecureOrigin =
      url::Origin::Create(GURL("http://example.com"));
  const url::Origin kOpaqueOrigin;

  mojo::Remote<mojom::PrivateAggregationHost> remote_1;
  EXPECT_FALSE(host_->BindNewReceiver(kInsecureOrigin,
                                      PrivateAggregationBudgetKey::Api::kFledge,
                                      remote_1.BindNewPipeAndPassReceiver()));

  mojo::Remote<mojom::PrivateAggregationHost> remote_2;
  EXPECT_FALSE(host_->BindNewReceiver(kOpaqueOrigin,
                                      PrivateAggregationBudgetKey::Api::kFledge,
                                      remote_2.BindNewPipeAndPassReceiver()));

  // Attempt to send a message to an unconnected remote. The request should not
  // be processed.
  EXPECT_CALL(mock_callback_, Run(_, _)).Times(0);
  std::vector<mojom::AggregatableReportHistogramContributionPtr> contributions;
  contributions.push_back(mojom::AggregatableReportHistogramContribution::New(
      /*bucket=*/123, /*value=*/456));
  remote_1->SendHistogramReport(std::move(contributions),
                                mojom::AggregationServiceMode::kDefault);

  // Flush to ensure disconnection and the SendHistogramReport call have had
  // time to be processed.
  remote_1.FlushForTesting();
  remote_2.FlushForTesting();
  EXPECT_FALSE(remote_1.is_connected());
  EXPECT_FALSE(remote_2.is_connected());
}

TEST_F(PrivateAggregationHostTest, InvalidRequest_Rejected) {
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));

  mojo::Remote<mojom::PrivateAggregationHost> remote;
  EXPECT_TRUE(host_->BindNewReceiver(kExampleOrigin,
                                     PrivateAggregationBudgetKey::Api::kFledge,
                                     remote.BindNewPipeAndPassReceiver()));

  // Negative values are invalid
  std::vector<mojom::AggregatableReportHistogramContributionPtr>
      negative_contributions;
  negative_contributions.push_back(
      mojom::AggregatableReportHistogramContribution::New(
          /*bucket=*/123, /*value=*/-1));

  std::vector<mojom::AggregatableReportHistogramContributionPtr>
      too_many_contributions;
  for (int i = 0; i < PrivateAggregationHost::kMaxNumberOfContributions + 1;
       ++i) {
    too_many_contributions.push_back(
        mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/123, /*value=*/1));
  }

  EXPECT_CALL(mock_callback_, Run(_, _)).Times(0);
  remote->SendHistogramReport(std::move(negative_contributions),
                              mojom::AggregationServiceMode::kDefault);
  remote->SendHistogramReport(std::move(too_many_contributions),
                              mojom::AggregationServiceMode::kDefault);
  remote.FlushForTesting();
}

}  // namespace content
