// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_features.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using BudgetDeniedBehavior = PrivateAggregationBudgeter::BudgetDeniedBehavior;

using testing::_;
using testing::Invoke;
using testing::Return;

using Checkpoint = testing::MockFunction<void(int step)>;

constexpr auto kExampleTime =
    base::Time::FromMillisecondsSinceUnixEpoch(1652984901234);

constexpr char kExampleOriginUrl[] = "https://origin.example";
constexpr char kExampleMainFrameUrl[] = "https://main_frame.example";
constexpr char kExampleCoordinatorUrl[] = "https://coordinator.example";

class PrivateAggregationManagerImplUnderTest
    : public PrivateAggregationManagerImpl {
 public:
  explicit PrivateAggregationManagerImplUnderTest(
      std::unique_ptr<PrivateAggregationBudgeter> budgeter,
      std::unique_ptr<PrivateAggregationHost> host,
      std::unique_ptr<AggregationService> aggregation_service)
      : PrivateAggregationManagerImpl(std::move(budgeter),
                                      std::move(host),
                                      /*storage_partition=*/nullptr),
        aggregation_service_(std::move(aggregation_service)) {}

  using PrivateAggregationManagerImpl::OnReportRequestDetailsReceivedFromHost;

  AggregationService* GetAggregationService() override {
    return aggregation_service_.get();
  }

 private:
  std::unique_ptr<AggregationService> aggregation_service_;
};

// Returns a generator and contributions vector. The generator returns a clone
// of `request` but must be passed the corresponding contributions vector.
// Used for manually triggering `OnReportRequestDetailsReceivedFromHost()`.
std::pair<PrivateAggregationHost::ReportRequestGenerator,
          std::vector<blink::mojom::AggregatableReportHistogramContribution>>
CloneAndSplitOutGenerator(const AggregatableReportRequest& request) {
  AggregatableReportRequest clone =
      aggregation_service::CloneReportRequest(request);

  PrivateAggregationHost::ReportRequestGenerator fake_generator =
      base::BindOnce(
          [](AggregatableReportRequest clone,
             std::vector<blink::mojom::AggregatableReportHistogramContribution>
                 contributions) {
            // Handle null reports
            if (contributions.empty()) {
              contributions.emplace_back(/*bucket=*/0, /*value=*/0,
                                         /*filtering_id=*/std::nullopt);
            }
            EXPECT_EQ(contributions, clone.payload_contents().contributions);
            return clone;
          },
          std::move(clone));
  return std::make_pair(std::move(fake_generator),
                        request.payload_contents().contributions);
}

constexpr char kBudgeterResultHistogram[] =
    "PrivacySandbox.PrivateAggregation.Budgeter.RequestResult3";

constexpr char kManagerResultHistogram[] =
    "PrivacySandbox.PrivateAggregation.Manager.RequestResult";

}  // namespace

class PrivateAggregationManagerImplTest : public testing::Test {
 public:
  PrivateAggregationManagerImplTest()
      : budgeter_(new testing::StrictMock<MockPrivateAggregationBudgeter>()),
        host_(new testing::StrictMock<MockPrivateAggregationHost>()),
        aggregation_service_(new testing::StrictMock<MockAggregationService>()),
        manager_(base::WrapUnique(budgeter_.get()),
                 base::WrapUnique(host_.get()),
                 base::WrapUnique(aggregation_service_.get())) {}

  ~PrivateAggregationManagerImplTest() override {
    budgeter_ = nullptr;
    host_ = nullptr;
    aggregation_service_ = nullptr;
  }

 protected:
  BrowserTaskEnvironment task_environment_;

  // Keep pointers around for EXPECT_CALL.
  raw_ptr<MockPrivateAggregationBudgeter> budgeter_;
  raw_ptr<MockPrivateAggregationHost> host_;
  raw_ptr<MockAggregationService> aggregation_service_;

  testing::StrictMock<PrivateAggregationManagerImplUnderTest> manager_;
};

TEST_F(PrivateAggregationManagerImplTest,
       BasicReportRequest_FerriedAppropriately) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  AggregatableReportRequest expected_request =
      aggregation_service::CreateExampleRequest();
  ASSERT_EQ(expected_request.payload_contents().contributions.size(), 1u);

  // As this report doesn't have debug mode enabled, we shouldn't get any debug
  // reports.
  EXPECT_EQ(expected_request.shared_info().debug_mode,
            AggregatableReportSharedInfo::DebugMode::kDisabled);
  EXPECT_CALL(*aggregation_service_, AssembleAndSendReport).Times(0);

  Checkpoint checkpoint;
  {
    testing::InSequence seq;
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(
        *budgeter_,
        ConsumeBudget(
            expected_request.payload_contents().contributions[0].value,
            example_key,
            expected_request.payload_contents().contributions[0].value, _))
        .WillOnce(Invoke(
            [&checkpoint](
                int, const PrivateAggregationBudgetKey&, int,
                base::OnceCallback<void(
                    PrivateAggregationBudgeter::RequestResult)> on_done) {
              checkpoint.Call(1);
              std::move(on_done).Run(
                  PrivateAggregationBudgeter::RequestResult::kApproved);
            }));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*aggregation_service_, ScheduleReport)
        .WillOnce(Invoke(
            [&expected_request](AggregatableReportRequest report_request) {
              EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
                  report_request, expected_request));
            }));
  }

  checkpoint.Call(0);

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(contributions), example_key,
      BudgetDeniedBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_F(PrivateAggregationManagerImplTest,
       ReportRequestWithMultipleContributions_CorrectBudgetRequested) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.contributions = {
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/100,
          /*filtering_id=*/std::nullopt),
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/5,
          /*filtering_id=*/std::nullopt),
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/456,
          /*value=*/20,
          /*filtering_id=*/std::nullopt)};

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*budgeter_, ConsumeBudget(/*budget=*/125, example_key,
                                          /*minimum_histogram_value=*/5, _))
        .WillOnce(Invoke(
            [&checkpoint](
                int, const PrivateAggregationBudgetKey&, int,
                base::OnceCallback<void(
                    PrivateAggregationBudgeter::RequestResult)> on_done) {
              checkpoint.Call(1);
              std::move(on_done).Run(
                  PrivateAggregationBudgeter::RequestResult::kApproved);
            }));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*aggregation_service_, ScheduleReport)
        .WillOnce(Invoke(
            [&expected_request](AggregatableReportRequest report_request) {
              EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
                  report_request, expected_request));
            }));
  }

  checkpoint.Call(0);

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(contributions), example_key,
      BudgetDeniedBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_F(PrivateAggregationManagerImplTest,
       BudgetRequestRejected_RequestNotScheduled) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  AggregatableReportRequest expected_request =
      aggregation_service::CreateExampleRequest();
  ASSERT_EQ(expected_request.payload_contents().contributions.size(), 1u);

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(
        *budgeter_,
        ConsumeBudget(
            expected_request.payload_contents().contributions[0].value,
            example_key,
            expected_request.payload_contents().contributions[0].value, _))
        .WillOnce(Invoke(
            [&checkpoint](
                int, const PrivateAggregationBudgetKey&, int,
                base::OnceCallback<void(
                    PrivateAggregationBudgeter::RequestResult)> on_done) {
              checkpoint.Call(1);
              std::move(on_done).Run(PrivateAggregationBudgeter::RequestResult::
                                         kInsufficientSmallerScopeBudget);
            }));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*aggregation_service_, ScheduleReport).Times(0);
  }

  checkpoint.Call(0);

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(contributions), example_key,
      BudgetDeniedBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(kBudgeterResultHistogram,
                               PrivateAggregationBudgeter::RequestResult::
                                   kInsufficientSmallerScopeBudget,
                               1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kNotSent, 1);
}

TEST_F(PrivateAggregationManagerImplTest,
       BudgetExceedsIntegerLimits_BudgetRejectedWithoutRequest) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.contributions = {
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/std::numeric_limits<int>::max(),
          /*filtering_id=*/std::nullopt),
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/456,
          /*value=*/1,
          /*filtering_id=*/std::nullopt)};

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  EXPECT_CALL(*budgeter_, ConsumeBudget).Times(0);
  EXPECT_CALL(*aggregation_service_, ScheduleReport).Times(0);

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(contributions), example_key,
      BudgetDeniedBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kRequestedMoreThanTotalBudget,
      1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kNotSent, 1);
}

TEST_F(PrivateAggregationManagerImplTest,
       DebugRequest_ImmediatelySentAfterBudgetRequest) {
  base::HistogramTester histogram;

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();
  shared_info.debug_mode = AggregatableReportSharedInfo::DebugMode::kEnabled;

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_request.shared_info().reporting_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  std::optional<AggregatableReportRequest> standard_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), shared_info.Clone(),
          AggregatableReportRequest::DelayType::ScheduledWithFullDelay,
          /*reporting_path=*/"/example-reporting-path");
  std::optional<AggregatableReportRequest> expected_debug_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(shared_info),
          AggregatableReportRequest::DelayType::Unscheduled,
          /*reporting_path=*/
          "/.well-known/private-aggregation/debug/report-protected-audience");
  ASSERT_TRUE(standard_request.has_value());
  ASSERT_TRUE(expected_debug_request.has_value());

  EXPECT_CALL(
      *budgeter_,
      ConsumeBudget(standard_request->payload_contents().contributions[0].value,
                    example_key,
                    standard_request->payload_contents().contributions[0].value,
                    _))
      .WillOnce(Invoke(
          [](int, const PrivateAggregationBudgetKey&, int,
             base::OnceCallback<void(PrivateAggregationBudgeter::RequestResult)>
                 on_done) {
            std::move(on_done).Run(
                PrivateAggregationBudgeter::RequestResult::kApproved);
          }));
  EXPECT_CALL(*aggregation_service_, AssembleAndSendReport)
      .WillOnce(Invoke([&](AggregatableReportRequest report_request) {
        EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
            report_request, expected_debug_request.value()));
      }));

  // Still triggers the standard (non-debug) report.
  EXPECT_CALL(*aggregation_service_, ScheduleReport)
      .WillOnce(
          Invoke([&standard_request](AggregatableReportRequest report_request) {
            EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
                report_request, standard_request.value()));
          }));

  auto [generator, contributions] =
      CloneAndSplitOutGenerator(standard_request.value());
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(contributions), example_key,
      BudgetDeniedBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_F(PrivateAggregationManagerImplTest,
       DebugRequestWithContextId_ImmediatelySentAfterBudgetRequest) {
  base::HistogramTester histogram;

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();
  shared_info.debug_mode = AggregatableReportSharedInfo::DebugMode::kEnabled;

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_request.shared_info().reporting_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  std::optional<AggregatableReportRequest> standard_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), shared_info.Clone(),
          AggregatableReportRequest::DelayType::ScheduledWithFullDelay,
          /*reporting_path=*/"/example-reporting-path",
          /*debug_key=*/std::nullopt,
          /*additional_fields=*/{{"context_id", "example_context_id"}});
  std::optional<AggregatableReportRequest> expected_debug_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(shared_info),
          AggregatableReportRequest::DelayType::Unscheduled,
          /*reporting_path=*/
          "/.well-known/private-aggregation/debug/report-protected-audience",
          /*debug_key=*/std::nullopt,
          /*additional_fields=*/{{"context_id", "example_context_id"}});
  ASSERT_TRUE(standard_request.has_value());
  ASSERT_TRUE(expected_debug_request.has_value());

  EXPECT_CALL(
      *budgeter_,
      ConsumeBudget(standard_request->payload_contents().contributions[0].value,
                    example_key,
                    standard_request->payload_contents().contributions[0].value,
                    _))
      .WillOnce(base::test::RunOnceCallback<3>(
          PrivateAggregationBudgeter::RequestResult::kApproved));
  EXPECT_CALL(*aggregation_service_, AssembleAndSendReport)
      .WillOnce(Invoke([&](AggregatableReportRequest report_request) {
        EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
            report_request, expected_debug_request.value()));
      }));

  // Still triggers the standard (non-debug) report.
  EXPECT_CALL(*aggregation_service_, ScheduleReport)
      .WillOnce(
          Invoke([&standard_request](AggregatableReportRequest report_request) {
            EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
                report_request, standard_request.value()));
          }));

  auto [generator, contributions] =
      CloneAndSplitOutGenerator(standard_request.value());
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(contributions), example_key,
      BudgetDeniedBehavior::kSendNullReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_F(PrivateAggregationManagerImplTest, DebugReportingPath) {
  base::HistogramTester histogram;

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();
  shared_info.debug_mode = AggregatableReportSharedInfo::DebugMode::kEnabled;

  std::optional<AggregatableReportRequest> standard_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), shared_info.Clone(),
          AggregatableReportRequest::DelayType::ScheduledWithFullDelay,
          /*reporting_path=*/"/example-reporting-path");
  ASSERT_TRUE(standard_request.has_value());

  PrivateAggregationBudgetKey protected_audience_key =
      PrivateAggregationBudgetKey::Create(
          example_request.shared_info().reporting_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();
  PrivateAggregationBudgetKey shared_storage_key =
      PrivateAggregationBudgetKey::Create(
          example_request.shared_info().reporting_origin, kExampleTime,
          PrivateAggregationCallerApi::kSharedStorage)
          .value();

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    EXPECT_CALL(*budgeter_, ConsumeBudget(_, protected_audience_key, _, _))
        .WillOnce(
            Invoke([](int, const PrivateAggregationBudgetKey&, int,
                      base::OnceCallback<void(
                          PrivateAggregationBudgeter::RequestResult)> on_done) {
              std::move(on_done).Run(
                  PrivateAggregationBudgeter::RequestResult::kApproved);
            }));
    EXPECT_CALL(*aggregation_service_, AssembleAndSendReport)
        .WillOnce(Invoke([&](AggregatableReportRequest report_request) {
          EXPECT_EQ(report_request.shared_info().reporting_origin,
                    example_request.shared_info().reporting_origin);
          EXPECT_EQ(report_request.reporting_path(),
                    "/.well-known/private-aggregation/debug/"
                    "report-protected-audience");
        }));
    // Still triggers the standard (non-debug) report.
    EXPECT_CALL(*aggregation_service_, ScheduleReport);

    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(*budgeter_, ConsumeBudget(_, shared_storage_key, _, _))
        .WillOnce(
            Invoke([](int, const PrivateAggregationBudgetKey&, int,
                      base::OnceCallback<void(
                          PrivateAggregationBudgeter::RequestResult)> on_done) {
              std::move(on_done).Run(
                  PrivateAggregationBudgeter::RequestResult::kApproved);
            }));
    EXPECT_CALL(*aggregation_service_, AssembleAndSendReport)
        .WillOnce(Invoke([&](AggregatableReportRequest report_request) {
          EXPECT_EQ(report_request.shared_info().reporting_origin,
                    example_request.shared_info().reporting_origin);
          EXPECT_EQ(
              report_request.reporting_path(),
              "/.well-known/private-aggregation/debug/report-shared-storage");
        }));
    // Still triggers the standard (non-debug) report.
    EXPECT_CALL(*aggregation_service_, ScheduleReport);
  }

  {
    auto [generator, contributions] =
        CloneAndSplitOutGenerator(standard_request.value());
    manager_.OnReportRequestDetailsReceivedFromHost(
        std::move(generator), std::move(contributions), protected_audience_key,
        BudgetDeniedBehavior::kDontSendReport);
  }
  checkpoint.Call(1);
  {
    auto [generator, contributions] =
        CloneAndSplitOutGenerator(standard_request.value());
    manager_.OnReportRequestDetailsReceivedFromHost(
        std::move(generator), std::move(contributions), shared_storage_key,
        BudgetDeniedBehavior::kDontSendReport);
  }

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 2);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 2);
}

TEST_F(PrivateAggregationManagerImplTest,
       BudgetDeniedWithDontSendReportBehavior_DebugRequestNotAssembledOrSent) {
  base::HistogramTester histogram;

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();
  shared_info.debug_mode = AggregatableReportSharedInfo::DebugMode::kEnabled;

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_request.shared_info().reporting_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  std::optional<AggregatableReportRequest> standard_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), shared_info.Clone(),
          AggregatableReportRequest::DelayType::ScheduledWithFullDelay,
          /*reporting_path=*/"/example-reporting-path");
  ASSERT_TRUE(standard_request.has_value());

  EXPECT_CALL(
      *budgeter_,
      ConsumeBudget(standard_request->payload_contents().contributions[0].value,
                    example_key,
                    standard_request->payload_contents().contributions[0].value,
                    _))
      .WillOnce(Invoke(
          [](int, const PrivateAggregationBudgetKey&, int,
             base::OnceCallback<void(PrivateAggregationBudgeter::RequestResult)>
                 on_done) {
            std::move(on_done).Run(
                PrivateAggregationBudgeter::RequestResult::kBadValuesOnDisk);
          }));
  EXPECT_CALL(*aggregation_service_, AssembleAndSendReport).Times(0);
  EXPECT_CALL(*aggregation_service_, ScheduleReport).Times(0);

  auto [generator, contributions] =
      CloneAndSplitOutGenerator(standard_request.value());
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(contributions), example_key,
      BudgetDeniedBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kBadValuesOnDisk, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kNotSent, 1);
}

TEST_F(PrivateAggregationManagerImplTest,
       BudgetDeniedWithSendNullReportBehavior_RequestSent) {
  base::HistogramTester histogram;

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();
  shared_info.debug_mode = AggregatableReportSharedInfo::DebugMode::kEnabled;

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_request.shared_info().reporting_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  AggregationServicePayloadContents null_payload =
      example_request.payload_contents();
  null_payload.contributions = {
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/0,
          /*value=*/0,
          /*filtering_id=*/std::nullopt)};

  std::optional<AggregatableReportRequest> null_request =
      AggregatableReportRequest::Create(
          null_payload, shared_info.Clone(),
          AggregatableReportRequest::DelayType::ScheduledWithFullDelay,
          /*reporting_path=*/"/example-reporting-path");
  std::optional<AggregatableReportRequest> expected_null_debug_request =
      AggregatableReportRequest::Create(
          null_payload, std::move(shared_info),
          AggregatableReportRequest::DelayType::Unscheduled,
          /*reporting_path=*/
          "/.well-known/private-aggregation/debug/report-protected-audience");
  ASSERT_TRUE(null_request.has_value());
  ASSERT_TRUE(expected_null_debug_request.has_value());

  EXPECT_CALL(*budgeter_,
              ConsumeBudget(
                  example_request.payload_contents().contributions[0].value,
                  example_key,
                  example_request.payload_contents().contributions[0].value, _))
      .WillOnce(Invoke(
          [](int, const PrivateAggregationBudgetKey&, int,
             base::OnceCallback<void(PrivateAggregationBudgeter::RequestResult)>
                 on_done) {
            std::move(on_done).Run(PrivateAggregationBudgeter::RequestResult::
                                       kInsufficientLargerScopeBudget);
          }));

  // Triggers the debug report
  EXPECT_CALL(*aggregation_service_, AssembleAndSendReport)
      .WillOnce(Invoke([&expected_null_debug_request](
                           AggregatableReportRequest report_request) {
        EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
            expected_null_debug_request.value(), report_request));
      }));

  // Triggers the standard (non-debug) report.
  EXPECT_CALL(*aggregation_service_, ScheduleReport)
      .WillOnce(
          Invoke([&null_request](AggregatableReportRequest report_request) {
            EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
                null_request.value(), report_request));
          }));

  auto [generator, null_contributions] =
      CloneAndSplitOutGenerator(null_request.value());
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), example_request.payload_contents().contributions,
      example_key, BudgetDeniedBehavior::kSendNullReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kInsufficientLargerScopeBudget,
      1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::
          kSentButContributionsClearedDueToBudgetDenial,
      1);
}

TEST_F(PrivateAggregationManagerImplTest,
       NoContributions_BudgetNotCheckedButNullReportSent) {
  base::HistogramTester histogram;

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();
  shared_info.debug_mode = AggregatableReportSharedInfo::DebugMode::kEnabled;

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_request.shared_info().reporting_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  AggregationServicePayloadContents null_payload =
      example_request.payload_contents();
  null_payload.contributions = {
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/0,
          /*value=*/0,
          /*filtering_id=*/std::nullopt)};

  std::optional<AggregatableReportRequest> null_request =
      AggregatableReportRequest::Create(
          null_payload, shared_info.Clone(),
          AggregatableReportRequest::DelayType::ScheduledWithFullDelay,
          /*reporting_path=*/"/example-reporting-path");
  std::optional<AggregatableReportRequest> expected_null_debug_request =
      AggregatableReportRequest::Create(
          null_payload, std::move(shared_info),
          AggregatableReportRequest::DelayType::Unscheduled,
          /*reporting_path=*/
          "/.well-known/private-aggregation/debug/report-protected-audience");
  ASSERT_TRUE(null_request.has_value());
  ASSERT_TRUE(expected_null_debug_request.has_value());

  EXPECT_CALL(*budgeter_, ConsumeBudget).Times(0);

  // Triggers the debug report
  EXPECT_CALL(*aggregation_service_, AssembleAndSendReport)
      .WillOnce(Invoke([&expected_null_debug_request](
                           AggregatableReportRequest report_request) {
        EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
            expected_null_debug_request.value(), report_request));
      }));

  // Triggers the standard (non-debug) report.
  EXPECT_CALL(*aggregation_service_, ScheduleReport)
      .WillOnce(
          Invoke([&null_request](AggregatableReportRequest report_request) {
            EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
                null_request.value(), report_request));
          }));

  auto [generator, null_contributions] =
      CloneAndSplitOutGenerator(null_request.value());
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), /*contributions=*/{}, example_key,
      BudgetDeniedBehavior::kSendNullReport);

  histogram.ExpectTotalCount(kBudgeterResultHistogram, 0);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithoutContributions,
      1);
}

TEST_F(PrivateAggregationManagerImplTest,
       BindNewReceiver_InvokesHostMethodIdentically) {
  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));
  const url::Origin example_main_frame_origin =
      url::Origin::Create(GURL(kExampleMainFrameUrl));
  const url::Origin example_coordinator_origin =
      url::Origin::Create(GURL(kExampleCoordinatorUrl));

  EXPECT_CALL(*host_, BindNewReceiver(
                          example_origin, example_main_frame_origin,
                          PrivateAggregationCallerApi::kProtectedAudience,
                          testing::Eq(std::nullopt), testing::Eq(std::nullopt),
                          testing::Eq(std::nullopt), 1, _))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      /*filtering_id_max_bytes=*/1,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));

  EXPECT_CALL(*host_, BindNewReceiver(
                          example_origin, example_main_frame_origin,
                          PrivateAggregationCallerApi::kSharedStorage,
                          testing::Eq(std::nullopt), testing::Eq(std::nullopt),
                          testing::Eq(std::nullopt), 1, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationCallerApi::kSharedStorage,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      /*filtering_id_max_bytes=*/1,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));

  EXPECT_CALL(*host_,
              BindNewReceiver(example_origin, example_main_frame_origin,
                              PrivateAggregationCallerApi::kProtectedAudience,
                              testing::Eq("example_context_id"),
                              testing::Eq(std::nullopt),
                              testing::Eq(std::nullopt), 1, _))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationCallerApi::kProtectedAudience, "example_context_id",
      /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      /*filtering_id_max_bytes=*/1,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));

  EXPECT_CALL(*host_,
              BindNewReceiver(example_origin, example_main_frame_origin,
                              PrivateAggregationCallerApi::kSharedStorage,
                              testing::Eq("example_context_id"),
                              testing::Eq(base::Seconds(5)),
                              testing::Eq(std::nullopt), 1, _))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Seconds(5),
      /*aggregation_coordinator_origin=*/std::nullopt,
      /*filtering_id_max_bytes=*/1,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));

  EXPECT_CALL(*host_, BindNewReceiver(
                          example_origin, example_main_frame_origin,
                          PrivateAggregationCallerApi::kProtectedAudience,
                          testing::Eq(std::nullopt), testing::Eq(std::nullopt),
                          testing::Eq(example_coordinator_origin), 1, _))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      example_coordinator_origin, /*filtering_id_max_bytes=*/1,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));

  EXPECT_CALL(*host_, BindNewReceiver(
                          example_origin, example_main_frame_origin,
                          PrivateAggregationCallerApi::kProtectedAudience,
                          testing::Eq(std::nullopt), testing::Eq(std::nullopt),
                          testing::Eq(std::nullopt), 8, _))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      /*filtering_id_max_bytes=*/8,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));
}

TEST_F(PrivateAggregationManagerImplTest,
       ClearBudgetingData_InvokesClearDataIdentically) {
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*budgeter_,
                ClearData(kExampleTime, kExampleTime + base::Days(1), _, _))
        .WillOnce(Invoke([](base::Time delete_begin, base::Time delete_end,
                            StoragePartition::StorageKeyMatcherFunction filter,
                            base::OnceClosure done) {
          EXPECT_TRUE(filter.is_null());
          std::move(done).Run();
        }));
    manager_.ClearBudgetData(kExampleTime, kExampleTime + base::Days(1),
                             StoragePartition::StorageKeyMatcherFunction(),
                             run_loop.QuitClosure());
    run_loop.Run();
  }

  StoragePartition::StorageKeyMatcherFunction example_filter;
  example_filter =
      base::BindLambdaForTesting([](const blink::StorageKey& storage_key) {
        return storage_key.origin() ==
               url::Origin::Create(GURL("https://example.com"));
      });

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*budgeter_,
                ClearData(kExampleTime - base::Days(10), kExampleTime, _, _))
        .WillOnce(Invoke([&example_filter](
                             base::Time delete_begin, base::Time delete_end,
                             StoragePartition::StorageKeyMatcherFunction filter,
                             base::OnceClosure done) {
          EXPECT_EQ(filter, example_filter);
          std::move(done).Run();
        }));
    manager_.ClearBudgetData(kExampleTime - base::Days(10), kExampleTime,
                             example_filter, run_loop.QuitClosure());
    run_loop.Run();
  }
}

TEST_F(PrivateAggregationManagerImplTest,
       BrowsingDataModel_CallbacksProperlyCalled) {
  AggregatableReportRequest expected_request =
      aggregation_service::CreateExampleRequest();

  std::vector<PrivateAggregationDataModel::DataKey> expected = {
      PrivateAggregationDataModel::DataKey(
          url::Origin::Create(GURL("https://example.com"))),
      PrivateAggregationDataModel::DataKey(
          url::Origin::Create(GURL("https://example2.com")))};

  {
    base::RunLoop run_loop;
    std::set<PrivateAggregationDataModel::DataKey> data_keys;
    auto cb = base::BindLambdaForTesting(
        [&](std::set<PrivateAggregationDataModel::DataKey> returned_keys) {
          data_keys = std::move(returned_keys);
        });

    EXPECT_CALL(*budgeter_, GetAllDataKeys)
        .WillOnce(base::test::RunOnceCallback<0>(
            std::set<PrivateAggregationDataModel::DataKey>{expected[0]}));
    EXPECT_CALL(*aggregation_service_, GetPendingReportReportingOrigins)
        .WillOnce(testing::DoAll(
            base::test::RunOnceClosure(run_loop.QuitClosure()),
            base::test::RunOnceCallback<0>(
                std::set<url::Origin>{expected[1].reporting_origin()})));

    manager_.GetAllDataKeys(cb);
    run_loop.Run();

    EXPECT_THAT(data_keys,
                testing::UnorderedElementsAre(expected[0], expected[1]));
  }

  {
    base::RunLoop run_loop;

    PrivateAggregationDataModel::DataKey data_key(
        expected_request.shared_info().reporting_origin);

    EXPECT_CALL(*budgeter_, DeleteByDataKey)
        .WillOnce(base::test::RunOnceCallback<1>());
    EXPECT_CALL(*aggregation_service_, ClearData)
        .WillOnce(base::test::RunOnceCallback<3>());

    manager_.RemovePendingDataKey(data_key, run_loop.QuitClosure());

    run_loop.Run();
  }
}

}  // namespace content
