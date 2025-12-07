// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
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
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_pending_contributions.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using NullReportBehavior = PrivateAggregationHost::NullReportBehavior;

using InspectBudgetCallResult =
    PrivateAggregationBudgeter::InspectBudgetCallResult;
using BudgetQueryResult = PrivateAggregationBudgeter::BudgetQueryResult;
using RequestResult = PrivateAggregationBudgeter::RequestResult;
using ResultForContribution = PrivateAggregationBudgeter::ResultForContribution;
using PendingReportLimitResult =
    PrivateAggregationBudgeter::PendingReportLimitResult;

using aggregation_service::ReportRequestIs;

using testing::_;
using testing::Return;

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

PrivateAggregationPendingContributions::Wrapper ConvertToWrapper(
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions) {
  std::optional<PrivateAggregationPendingContributions::Wrapper> wrapper;
  if (base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiErrorReporting)) {
    wrapper = PrivateAggregationPendingContributions::Wrapper(
        PrivateAggregationPendingContributions(20u, {}));
    wrapper->GetPendingContributions().AddUnconditionalContributions(
        std::move(contributions));
    wrapper->GetPendingContributions().MarkContributionsFinalized(
        PrivateAggregationPendingContributions::TimeoutOrDisconnect::
            kDisconnect);
  } else {
    wrapper = PrivateAggregationPendingContributions::Wrapper(
        std::move(contributions));
  }
  return *std::move(wrapper);
}

// Returns a generator and pending contributions object. The generator returns a
// clone of `request` but must be passed the corresponding contributions object.
// Used for manually triggering `OnReportRequestDetailsReceivedFromHost()`.
std::pair<PrivateAggregationHost::ReportRequestGenerator,
          PrivateAggregationPendingContributions::Wrapper>
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

  return std::make_pair(
      std::move(fake_generator),
      ConvertToWrapper(request.payload_contents().contributions));
}

constexpr char kBudgeterResultHistogram[] =
    "PrivacySandbox.PrivateAggregation.Budgeter.RequestResult3";

constexpr char kManagerResultHistogram[] =
    "PrivacySandbox.PrivateAggregation.Manager.RequestResult";

}  // namespace

class PrivateAggregationManagerImplTestBase : public testing::Test {
 public:
  explicit PrivateAggregationManagerImplTestBase(
      bool enable_error_reporting_feature)
      : budgeter_(new testing::StrictMock<MockPrivateAggregationBudgeter>()),
        host_(new testing::StrictMock<MockPrivateAggregationHost>()),
        aggregation_service_(new testing::StrictMock<MockAggregationService>()),
        manager_(base::WrapUnique(budgeter_.get()),
                 base::WrapUnique(host_.get()),
                 base::WrapUnique(aggregation_service_.get())) {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kPrivateAggregationApiErrorReporting,
        enable_error_reporting_feature);
  }

  ~PrivateAggregationManagerImplTestBase() override {
    budgeter_ = nullptr;
    host_ = nullptr;
    aggregation_service_ = nullptr;
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // Keep pointers around for EXPECT_CALL.
  raw_ptr<MockPrivateAggregationBudgeter> budgeter_;
  raw_ptr<MockPrivateAggregationHost> host_;
  raw_ptr<MockAggregationService> aggregation_service_;

  testing::StrictMock<PrivateAggregationManagerImplUnderTest> manager_;
};

class PrivateAggregationManagerImplTest
    : public PrivateAggregationManagerImplTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PrivateAggregationManagerImplTest()
      : PrivateAggregationManagerImplTestBase(GetErrorReportingEnabledParam()) {
  }

  bool GetErrorReportingEnabledParam() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         PrivateAggregationManagerImplTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "ErrorReportingEnabled"
                                             : "ErrorReportingDisabled";
                         });

class PrivateAggregationManagerImplErrorReportingEnabledTest
    : public PrivateAggregationManagerImplTestBase {
 public:
  PrivateAggregationManagerImplErrorReportingEnabledTest()
      : PrivateAggregationManagerImplTestBase(
            /*enable_error_reporting_feature=*/true) {}
};

TEST_P(PrivateAggregationManagerImplTest,
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

  {
    testing::InSequence seq;

    if (GetErrorReportingEnabledParam()) {
      EXPECT_CALL(*budgeter_,
                  InspectBudgetAndLock(
                      expected_request.payload_contents().contributions,
                      example_key, _))
          .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
              BudgetQueryResult(RequestResult::kApproved,
                                {ResultForContribution::kApproved}),
              PrivateAggregationBudgeter::Lock::CreateForTesting(),
              PendingReportLimitResult::kNotAtLimit)));
      EXPECT_CALL(
          *budgeter_,
          ConsumeBudget(_, expected_request.payload_contents().contributions,
                        example_key, _))
          .WillOnce(base::test::RunOnceCallback<3>(BudgetQueryResult(
              RequestResult::kApproved, {ResultForContribution::kApproved})));

    } else {
      EXPECT_CALL(
          *budgeter_,
          ConsumeBudget(
              expected_request.payload_contents().contributions[0].value,
              example_key,
              expected_request.payload_contents().contributions[0].value, _))
          .WillOnce(base::test::RunOnceCallback<3>(
              PrivateAggregationBudgeter::RequestResult::kApproved));
    }

    EXPECT_CALL(*aggregation_service_,
                ScheduleReport(ReportRequestIs(expected_request)));
  }

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator),
      PrivateAggregationPendingContributions::Wrapper(std::move(contributions)),
      example_key, NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_P(PrivateAggregationManagerImplTest,
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
          /*filtering_id=*/1u),
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/456,
          /*value=*/20,
          /*filtering_id=*/std::nullopt)};

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  {
    testing::InSequence seq;

    if (GetErrorReportingEnabledParam()) {
      EXPECT_CALL(*budgeter_,
                  InspectBudgetAndLock(
                      expected_request.payload_contents().contributions,
                      example_key, _))
          .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
              BudgetQueryResult(RequestResult::kApproved,
                                std::vector<ResultForContribution>(
                                    3, ResultForContribution::kApproved)),
              PrivateAggregationBudgeter::Lock::CreateForTesting(),
              PendingReportLimitResult::kNotAtLimit)));
      EXPECT_CALL(
          *budgeter_,
          ConsumeBudget(_, expected_request.payload_contents().contributions,
                        example_key, _))
          .WillOnce(base::test::RunOnceCallback<3>(
              BudgetQueryResult(RequestResult::kApproved,
                                std::vector<ResultForContribution>(
                                    3, ResultForContribution::kApproved))));
    } else {
      EXPECT_CALL(*budgeter_, ConsumeBudget(/*budget=*/125, example_key,
                                            /*minimum_value_for_metrics=*/5, _))
          .WillOnce(base::test::RunOnceCallback<3>(
              PrivateAggregationBudgeter::RequestResult::kApproved));
    }

    EXPECT_CALL(*aggregation_service_,
                ScheduleReport(ReportRequestIs(expected_request)));
  }

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator),
      PrivateAggregationPendingContributions::Wrapper(std::move(contributions)),
      example_key, NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_P(PrivateAggregationManagerImplTest,
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

  {
    testing::InSequence seq;

    if (GetErrorReportingEnabledParam()) {
      EXPECT_CALL(*budgeter_,
                  InspectBudgetAndLock(
                      expected_request.payload_contents().contributions,
                      example_key, _))
          .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
              BudgetQueryResult(RequestResult::kInsufficientSmallerScopeBudget,
                                {ResultForContribution::kDenied}),
              PrivateAggregationBudgeter::Lock::CreateForTesting(),
              PendingReportLimitResult::kNotAtLimit)));
      EXPECT_CALL(*budgeter_,
                  ConsumeBudget(_, testing::IsEmpty(), example_key, _))
          .WillOnce(base::test::RunOnceCallback<3>(
              BudgetQueryResult(RequestResult::kApproved, {})));
    } else {
      EXPECT_CALL(
          *budgeter_,
          ConsumeBudget(
              expected_request.payload_contents().contributions[0].value,
              example_key,
              expected_request.payload_contents().contributions[0].value, _))
          .WillOnce(base::test::RunOnceCallback<3>(
              PrivateAggregationBudgeter::RequestResult::
                  kInsufficientSmallerScopeBudget));
    }

    EXPECT_CALL(*aggregation_service_, ScheduleReport).Times(0);
  }

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator),
      PrivateAggregationPendingContributions::Wrapper(std::move(contributions)),
      example_key, NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(kBudgeterResultHistogram,
                               PrivateAggregationBudgeter::RequestResult::
                                   kInsufficientSmallerScopeBudget,
                               1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kNotSent, 1);
}

TEST_P(PrivateAggregationManagerImplTest,
       BudgetExceedsIntegerLimits_BudgetRejected) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  blink::mojom::AggregatableReportHistogramContribution example_contribution(
      /*bucket=*/456,
      /*value=*/1,
      /*filtering_id=*/std::nullopt);

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.contributions = {
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/std::numeric_limits<int>::max(),
          /*filtering_id=*/std::nullopt),
      example_contribution};

  AggregatableReportRequest large_budget_request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  // Only expected when the error reporting feature is enabled.
  AggregationServicePayloadContents expected_payload_contents =
      payload_contents;
  expected_payload_contents.contributions = {example_contribution};
  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(expected_payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  // When the feature is disabled, the query is rejected without a request. When
  // enabled, per-contribution budgeting occurs.
  if (GetErrorReportingEnabledParam()) {
    testing::InSequence seq;

    EXPECT_CALL(*budgeter_,
                InspectBudgetAndLock(
                    large_budget_request.payload_contents().contributions,
                    example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kRequestedMoreThanTotalBudget,
                              {ResultForContribution::kDenied,
                               ResultForContribution::kApproved}),
            PrivateAggregationBudgeter::Lock::CreateForTesting(),
            PendingReportLimitResult::kNotAtLimit)));
    EXPECT_CALL(*budgeter_,
                ConsumeBudget(_, testing::ElementsAre(example_contribution),
                              example_key, _))
        .WillOnce(base::test::RunOnceCallback<3>(BudgetQueryResult(
            RequestResult::kApproved, {ResultForContribution::kApproved})));
    EXPECT_CALL(*aggregation_service_,
                ScheduleReport(ReportRequestIs(expected_request)));
  } else {
    EXPECT_CALL(*budgeter_, ConsumeBudget(testing::An<int>(), _, _, _))
        .Times(0);
    EXPECT_CALL(*aggregation_service_, ScheduleReport).Times(0);
  }

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator),
      ConvertToWrapper(large_budget_request.payload_contents().contributions),
      example_key, NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kRequestedMoreThanTotalBudget,
      1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      GetErrorReportingEnabledParam()
          ? PrivateAggregationManagerImpl::RequestResult::kSentWithContributions
          : PrivateAggregationManagerImpl::RequestResult::kNotSent,
      1);
}

TEST_P(PrivateAggregationManagerImplTest,
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
  ASSERT_FALSE(standard_request->payload_contents().contributions.empty());

  testing::Expectation consume_budget_call;

  if (GetErrorReportingEnabledParam()) {
    testing::InSequence seq;

    EXPECT_CALL(
        *budgeter_,
        InspectBudgetAndLock(standard_request->payload_contents().contributions,
                             example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kApproved,
                              {ResultForContribution::kApproved}),
            PrivateAggregationBudgeter::Lock::CreateForTesting(),
            PendingReportLimitResult::kNotAtLimit)));
    consume_budget_call =
        EXPECT_CALL(
            *budgeter_,
            ConsumeBudget(_, standard_request->payload_contents().contributions,
                          example_key, _))
            .WillOnce(base::test::RunOnceCallback<3>(BudgetQueryResult(
                RequestResult::kApproved, {ResultForContribution::kApproved})));
  } else {
    consume_budget_call =
        EXPECT_CALL(
            *budgeter_,
            ConsumeBudget(
                standard_request->payload_contents().contributions[0].value,
                example_key,
                standard_request->payload_contents().contributions[0].value, _))
            .WillOnce(base::test::RunOnceCallback<3>(
                PrivateAggregationBudgeter::RequestResult::kApproved));
  }

  EXPECT_CALL(*aggregation_service_,
              AssembleAndSendReport(ReportRequestIs(*expected_debug_request)))
      .After(consume_budget_call);

  // Still triggers the standard (non-debug) report.
  EXPECT_CALL(*aggregation_service_,
              ScheduleReport(ReportRequestIs(*standard_request)))
      .After(consume_budget_call);

  auto [generator, contributions] =
      CloneAndSplitOutGenerator(standard_request.value());
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator),
      PrivateAggregationPendingContributions::Wrapper(std::move(contributions)),
      example_key, NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_P(PrivateAggregationManagerImplTest,
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
  ASSERT_FALSE(standard_request->payload_contents().contributions.empty());

  testing::Expectation consume_budget_call;

  if (GetErrorReportingEnabledParam()) {
    testing::InSequence seq;

    EXPECT_CALL(
        *budgeter_,
        InspectBudgetAndLock(standard_request->payload_contents().contributions,
                             example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kApproved,
                              {ResultForContribution::kApproved}),
            PrivateAggregationBudgeter::Lock::CreateForTesting(),
            PendingReportLimitResult::kNotAtLimit)));
    consume_budget_call =
        EXPECT_CALL(
            *budgeter_,
            ConsumeBudget(_, standard_request->payload_contents().contributions,
                          example_key, _))
            .WillOnce(base::test::RunOnceCallback<3>(BudgetQueryResult(
                RequestResult::kApproved, {ResultForContribution::kApproved})));
  } else {
    consume_budget_call =
        EXPECT_CALL(
            *budgeter_,
            ConsumeBudget(
                standard_request->payload_contents().contributions[0].value,
                example_key,
                standard_request->payload_contents().contributions[0].value, _))
            .WillOnce(base::test::RunOnceCallback<3>(
                PrivateAggregationBudgeter::RequestResult::kApproved));
  }

  EXPECT_CALL(*aggregation_service_,
              AssembleAndSendReport(ReportRequestIs(*expected_debug_request)))
      .After(consume_budget_call);

  // Still triggers the standard (non-debug) report.
  EXPECT_CALL(*aggregation_service_,
              ScheduleReport(ReportRequestIs(*standard_request)))
      .After(consume_budget_call);

  auto [generator, contributions] =
      CloneAndSplitOutGenerator(standard_request.value());
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator),
      PrivateAggregationPendingContributions::Wrapper(std::move(contributions)),
      example_key, NullReportBehavior::kSendNullReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_P(PrivateAggregationManagerImplTest, DebugReportingPath) {
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

  // Like `ReportRequestIs`, but only tests the reporting origin and path.
  auto ReportRequestHasDestination = [](url::Origin reporting_origin,
                                        std::string_view reporting_path) {
    return testing::AllOf(
        testing::Property(
            &AggregatableReportRequest::shared_info,
            testing::Field(&AggregatableReportSharedInfo::reporting_origin,
                           reporting_origin)),
        testing::Property(&AggregatableReportRequest::reporting_path,
                          reporting_path));
  };

  testing::MockFunction<void(int step)> checkpoint;
  {
    testing::InSequence seq;

    if (GetErrorReportingEnabledParam()) {
      EXPECT_CALL(*budgeter_,
                  InspectBudgetAndLock(_, protected_audience_key, _))
          .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
              BudgetQueryResult(RequestResult::kApproved,
                                {ResultForContribution::kApproved}),
              PrivateAggregationBudgeter::Lock::CreateForTesting(),
              PendingReportLimitResult::kNotAtLimit)));
      EXPECT_CALL(*budgeter_, ConsumeBudget(_, _, protected_audience_key, _))
          .WillOnce(base::test::RunOnceCallback<3>(BudgetQueryResult(
              RequestResult::kApproved, {ResultForContribution::kApproved})));
    } else {
      EXPECT_CALL(*budgeter_, ConsumeBudget(_, protected_audience_key, _, _))
          .WillOnce(base::test::RunOnceCallback<3>(
              PrivateAggregationBudgeter::RequestResult::kApproved));
    }
    EXPECT_CALL(*aggregation_service_,
                AssembleAndSendReport(ReportRequestHasDestination(
                    example_request.shared_info().reporting_origin,
                    "/.well-known/private-aggregation/debug/"
                    "report-protected-audience")));
    // Still triggers the standard (non-debug) report.
    EXPECT_CALL(*aggregation_service_, ScheduleReport);

    EXPECT_CALL(checkpoint, Call(1));

    if (GetErrorReportingEnabledParam()) {
      EXPECT_CALL(*budgeter_, InspectBudgetAndLock(_, shared_storage_key, _))
          .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
              BudgetQueryResult(RequestResult::kApproved,
                                {ResultForContribution::kApproved}),
              PrivateAggregationBudgeter::Lock::CreateForTesting(),
              PendingReportLimitResult::kNotAtLimit)));
      EXPECT_CALL(*budgeter_, ConsumeBudget(_, _, shared_storage_key, _))
          .WillOnce(base::test::RunOnceCallback<3>(BudgetQueryResult(
              RequestResult::kApproved, {ResultForContribution::kApproved})));
    } else {
      EXPECT_CALL(*budgeter_, ConsumeBudget(_, shared_storage_key, _, _))
          .WillOnce(base::test::RunOnceCallback<3>(
              PrivateAggregationBudgeter::RequestResult::kApproved));
    }
    EXPECT_CALL(
        *aggregation_service_,
        AssembleAndSendReport(ReportRequestHasDestination(
            example_request.shared_info().reporting_origin,
            "/.well-known/private-aggregation/debug/report-shared-storage")));
    // Still triggers the standard (non-debug) report.
    EXPECT_CALL(*aggregation_service_, ScheduleReport);
  }

  {
    auto [generator, contributions] =
        CloneAndSplitOutGenerator(standard_request.value());
    manager_.OnReportRequestDetailsReceivedFromHost(
        std::move(generator),
        PrivateAggregationPendingContributions::Wrapper(
            std::move(contributions)),
        protected_audience_key, NullReportBehavior::kDontSendReport);
  }
  checkpoint.Call(1);
  {
    auto [generator, contributions] =
        CloneAndSplitOutGenerator(standard_request.value());
    manager_.OnReportRequestDetailsReceivedFromHost(
        std::move(generator),
        PrivateAggregationPendingContributions::Wrapper(
            std::move(contributions)),
        shared_storage_key, NullReportBehavior::kDontSendReport);
  }

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 2);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 2);
}

TEST_P(PrivateAggregationManagerImplTest,
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
  ASSERT_FALSE(standard_request->payload_contents().contributions.empty());

  if (GetErrorReportingEnabledParam()) {
    EXPECT_CALL(
        *budgeter_,
        InspectBudgetAndLock(standard_request->payload_contents().contributions,
                             example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kBadValuesOnDisk,
                              {ResultForContribution::kDenied}),
            /*lock=*/std::nullopt, PendingReportLimitResult::kNotAtLimit)));
    EXPECT_CALL(
        *budgeter_,
        ConsumeBudget(testing::A<PrivateAggregationBudgeter::Lock>(), _, _, _))
        .Times(0);
  } else {
    EXPECT_CALL(
        *budgeter_,
        ConsumeBudget(
            standard_request->payload_contents().contributions[0].value,
            example_key,
            standard_request->payload_contents().contributions[0].value, _))
        .WillOnce(base::test::RunOnceCallback<3>(
            PrivateAggregationBudgeter::RequestResult::kBadValuesOnDisk));
  }
  EXPECT_CALL(*aggregation_service_, AssembleAndSendReport).Times(0);
  EXPECT_CALL(*aggregation_service_, ScheduleReport).Times(0);

  auto [generator, contributions] =
      CloneAndSplitOutGenerator(standard_request.value());
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator),
      PrivateAggregationPendingContributions::Wrapper(std::move(contributions)),
      example_key, NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kBadValuesOnDisk, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kNotSent, 1);
}

TEST_P(PrivateAggregationManagerImplTest,
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
  ASSERT_FALSE(example_request.payload_contents().contributions.empty());

  if (GetErrorReportingEnabledParam()) {
    EXPECT_CALL(
        *budgeter_,
        InspectBudgetAndLock(example_request.payload_contents().contributions,
                             example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kInsufficientLargerScopeBudget,
                              {ResultForContribution::kDenied}),
            /*lock=*/std::nullopt, PendingReportLimitResult::kNotAtLimit)));
    EXPECT_CALL(
        *budgeter_,
        ConsumeBudget(testing::A<PrivateAggregationBudgeter::Lock>(), _, _, _))
        .Times(0);
  } else {
    EXPECT_CALL(
        *budgeter_,
        ConsumeBudget(example_request.payload_contents().contributions[0].value,
                      example_key,
                      example_request.payload_contents().contributions[0].value,
                      _))
        .WillOnce(base::test::RunOnceCallback<3>(
            PrivateAggregationBudgeter::RequestResult::
                kInsufficientLargerScopeBudget));
  }

  // Triggers the debug report
  EXPECT_CALL(
      *aggregation_service_,
      AssembleAndSendReport(ReportRequestIs(*expected_null_debug_request)));

  // Triggers the standard (non-debug) report.
  EXPECT_CALL(*aggregation_service_,
              ScheduleReport(ReportRequestIs(*null_request)));

  auto [generator, null_contributions] =
      CloneAndSplitOutGenerator(null_request.value());
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator),
      ConvertToWrapper(example_request.payload_contents().contributions),
      example_key, NullReportBehavior::kSendNullReport);

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

TEST_P(PrivateAggregationManagerImplTest,
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

  if (GetErrorReportingEnabledParam()) {
    EXPECT_CALL(*budgeter_, InspectBudgetAndLock).Times(0);
    EXPECT_CALL(
        *budgeter_,
        ConsumeBudget(testing::A<PrivateAggregationBudgeter::Lock>(), _, _, _))
        .Times(0);
  } else {
    EXPECT_CALL(*budgeter_, ConsumeBudget(testing::An<int>(), _, _, _))
        .Times(0);
  }

  // Triggers the debug report
  EXPECT_CALL(
      *aggregation_service_,
      AssembleAndSendReport(ReportRequestIs(*expected_null_debug_request)));

  // Triggers the standard (non-debug) report.
  EXPECT_CALL(*aggregation_service_,
              ScheduleReport(ReportRequestIs(*null_request)));

  auto [generator, null_contributions] =
      CloneAndSplitOutGenerator(null_request.value());
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator),
      /*contributions=*/
      ConvertToWrapper(
          std::vector<blink::mojom::AggregatableReportHistogramContribution>()),
      example_key, NullReportBehavior::kSendNullReport);

  histogram.ExpectTotalCount(kBudgeterResultHistogram, 0);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithoutContributions,
      1);
}

TEST_P(PrivateAggregationManagerImplTest,
       BindNewReceiver_InvokesHostMethodIdentically) {
  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));
  const url::Origin example_main_frame_origin =
      url::Origin::Create(GURL(kExampleMainFrameUrl));
  const url::Origin example_coordinator_origin =
      url::Origin::Create(GURL(kExampleCoordinatorUrl));

  for (auto api : {PrivateAggregationCallerApi::kProtectedAudience,
                   PrivateAggregationCallerApi::kSharedStorage}) {
    // Vary the api parameter.
    EXPECT_CALL(*host_,
                BindNewReceiver(
                    example_origin, example_main_frame_origin, api,
                    testing::Eq(std::nullopt), testing::Eq(std::nullopt),
                    testing::Eq(std::nullopt), 1, testing::Eq(std::nullopt), _))
        .WillOnce(Return(true));
    EXPECT_TRUE(manager_.BindNewReceiver(
        example_origin, example_main_frame_origin, api,
        /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        /*filtering_id_max_bytes=*/1,
        /*max_contributions=*/std::nullopt,
        mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));

    // Vary the api paired with a context ID.
    EXPECT_CALL(
        *host_,
        BindNewReceiver(example_origin, example_main_frame_origin, api,
                        testing::Eq("example_context_id"),
                        testing::Eq(std::nullopt), testing::Eq(std::nullopt), 1,
                        testing::Eq(std::nullopt), _))
        .WillOnce(Return(true));
    EXPECT_TRUE(manager_.BindNewReceiver(
        example_origin, example_main_frame_origin, api, "example_context_id",
        /*timeout=*/std::nullopt,
        /*aggregation_coordinator_origin=*/std::nullopt,
        /*filtering_id_max_bytes=*/1,
        /*max_contributions=*/std::nullopt,
        mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));
  }

  // Specify a context ID and a timeout.
  EXPECT_CALL(
      *host_,
      BindNewReceiver(example_origin, example_main_frame_origin,
                      PrivateAggregationCallerApi::kSharedStorage,
                      testing::Eq("example_context_id"),
                      testing::Eq(base::Seconds(5)), testing::Eq(std::nullopt),
                      1, testing::Eq(std::nullopt), _))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationCallerApi::kSharedStorage, "example_context_id",
      /*timeout=*/base::Seconds(5),
      /*aggregation_coordinator_origin=*/std::nullopt,
      /*filtering_id_max_bytes=*/1,
      /*max_contributions=*/std::nullopt,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));

  // Specify a coordinator origin.
  EXPECT_CALL(*host_, BindNewReceiver(
                          example_origin, example_main_frame_origin,
                          PrivateAggregationCallerApi::kProtectedAudience,
                          testing::Eq(std::nullopt), testing::Eq(std::nullopt),
                          testing::Eq(example_coordinator_origin), 1,
                          testing::Eq(std::nullopt), _))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      example_coordinator_origin, /*filtering_id_max_bytes=*/1,
      /*max_contributions=*/std::nullopt,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));

  // Specify a non-default `filtering_id_max_bytes`.
  EXPECT_CALL(*host_,
              BindNewReceiver(
                  example_origin, example_main_frame_origin,
                  PrivateAggregationCallerApi::kProtectedAudience,
                  testing::Eq(std::nullopt), testing::Eq(std::nullopt),
                  testing::Eq(std::nullopt), 8, testing::Eq(std::nullopt), _))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationCallerApi::kProtectedAudience,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      /*filtering_id_max_bytes=*/8,
      /*max_contributions=*/std::nullopt,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));

  // Specify `max_contributions`.
  EXPECT_CALL(
      *host_,
      BindNewReceiver(example_origin, example_main_frame_origin,
                      PrivateAggregationCallerApi::kSharedStorage,
                      testing::Eq(std::nullopt), testing::Eq(std::nullopt),
                      testing::Eq(std::nullopt), 1, testing::Optional(42), _))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationCallerApi::kSharedStorage,
      /*context_id=*/std::nullopt, /*timeout=*/std::nullopt,
      /*aggregation_coordinator_origin=*/std::nullopt,
      /*filtering_id_max_bytes=*/1,
      /*max_contributions=*/42,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>()));
}

TEST_P(PrivateAggregationManagerImplTest,
       ClearBudgetingData_InvokesClearDataIdentically) {
  {
    base::RunLoop run_loop;
    EXPECT_CALL(
        *budgeter_,
        ClearData(
            kExampleTime, kExampleTime + base::Days(1),
            testing::Property(
                &StoragePartition::StorageKeyMatcherFunction::is_null, true),
            _))
        .WillOnce(base::test::RunOnceCallback<3>());

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
    EXPECT_CALL(*budgeter_, ClearData(kExampleTime - base::Days(10),
                                      kExampleTime, example_filter, _))
        .WillOnce(base::test::RunOnceCallback<3>());
    manager_.ClearBudgetData(kExampleTime - base::Days(10), kExampleTime,
                             example_filter, run_loop.QuitClosure());
    run_loop.Run();
  }
}

TEST_P(PrivateAggregationManagerImplTest,
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

TEST_F(PrivateAggregationManagerImplErrorReportingEnabledTest,
       ConditionalRequestsAlsoHandled) {
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
  AggregationServicePayloadContents expected_payload_contents =
      example_request.payload_contents();
  expected_payload_contents.contributions = {
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/100,
          /*filtering_id=*/std::nullopt),
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/5,
          /*filtering_id=*/1u),
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/456,
          /*value=*/20,
          /*filtering_id=*/std::nullopt)};

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(expected_payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unconditional_contributions = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/5,
              /*filtering_id=*/1u),
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/456,
              /*value=*/20,
              /*filtering_id=*/std::nullopt)};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      conditional_contributions = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/100,
              /*filtering_id=*/std::nullopt)};

  {
    testing::InSequence seq;

    EXPECT_CALL(*budgeter_, InspectBudgetAndLock(unconditional_contributions,
                                                 example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kApproved,
                              std::vector<ResultForContribution>(
                                  2, ResultForContribution::kApproved)),
            PrivateAggregationBudgeter::Lock::CreateForTesting(),
            PendingReportLimitResult::kNotAtLimit)));
    EXPECT_CALL(
        *budgeter_,
        ConsumeBudget(_, expected_request.payload_contents().contributions,
                      example_key, _))
        .WillOnce(base::test::RunOnceCallback<3>(
            BudgetQueryResult(RequestResult::kApproved,
                              std::vector<ResultForContribution>(
                                  3, ResultForContribution::kApproved))));

    EXPECT_CALL(*aggregation_service_,
                ScheduleReport(ReportRequestIs(expected_request)));
  }

  PrivateAggregationPendingContributions::Wrapper wrapper =
      PrivateAggregationPendingContributions::Wrapper(
          PrivateAggregationPendingContributions(20u, {}));
  wrapper.GetPendingContributions().AddUnconditionalContributions(
      unconditional_contributions);
  wrapper.GetPendingContributions().AddConditionalContributions(
      blink::mojom::PrivateAggregationErrorEvent::kReportSuccess,
      conditional_contributions);
  wrapper.GetPendingContributions().MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(wrapper), example_key,
      NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_F(PrivateAggregationManagerImplErrorReportingEnabledTest,
       MergeableContributions_Merged) {
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
  AggregationServicePayloadContents expected_payload_contents =
      example_request.payload_contents();
  expected_payload_contents.contributions = {
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/105,
          /*filtering_id=*/std::nullopt)};

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(expected_payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unmerged_contributions = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/100,
              /*filtering_id=*/std::nullopt),
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/5,
              /*filtering_id=*/std::nullopt)};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unconditional_contributions = {unmerged_contributions[1]};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      conditional_contributions = {unmerged_contributions[0]};

  {
    testing::InSequence seq;

    EXPECT_CALL(*budgeter_, InspectBudgetAndLock(unconditional_contributions,
                                                 example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kApproved,
                              {ResultForContribution::kApproved}),
            PrivateAggregationBudgeter::Lock::CreateForTesting(),
            PendingReportLimitResult::kNotAtLimit)));
    EXPECT_CALL(*budgeter_,
                ConsumeBudget(_, unmerged_contributions, example_key, _))
        .WillOnce(base::test::RunOnceCallback<3>(
            BudgetQueryResult(RequestResult::kApproved,
                              std::vector<ResultForContribution>(
                                  2, ResultForContribution::kApproved))));

    EXPECT_CALL(*aggregation_service_,
                ScheduleReport(ReportRequestIs(expected_request)));
  }

  PrivateAggregationPendingContributions::Wrapper wrapper =
      PrivateAggregationPendingContributions::Wrapper(
          PrivateAggregationPendingContributions(20u, {}));
  wrapper.GetPendingContributions().AddUnconditionalContributions(
      unconditional_contributions);
  wrapper.GetPendingContributions().AddConditionalContributions(
      blink::mojom::PrivateAggregationErrorEvent::kReportSuccess,
      conditional_contributions);
  wrapper.GetPendingContributions().MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(wrapper), example_key,
      NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_F(PrivateAggregationManagerImplErrorReportingEnabledTest,
       ConditionalContributionsDenied) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unmerged_contributions = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/100,
              /*filtering_id=*/std::nullopt),
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/5,
              /*filtering_id=*/std::nullopt)};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unconditional_contributions = {unmerged_contributions[1]};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      conditional_contributions = {unmerged_contributions[0]};

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents expected_payload_contents =
      example_request.payload_contents();
  expected_payload_contents.contributions = unconditional_contributions;

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(expected_payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  {
    testing::InSequence seq;

    EXPECT_CALL(*budgeter_, InspectBudgetAndLock(unconditional_contributions,
                                                 example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kApproved,
                              {ResultForContribution::kApproved}),
            PrivateAggregationBudgeter::Lock::CreateForTesting(),
            PendingReportLimitResult::kNotAtLimit)));
    EXPECT_CALL(*budgeter_,
                ConsumeBudget(_, unmerged_contributions, example_key, _))
        .WillOnce(base::test::RunOnceCallback<3>(
            BudgetQueryResult(RequestResult::kInsufficientSmallerScopeBudget,
                              {ResultForContribution::kDenied,
                               ResultForContribution::kApproved})));
    EXPECT_CALL(*aggregation_service_,
                ScheduleReport(ReportRequestIs(expected_request)));
  }

  PrivateAggregationPendingContributions::Wrapper wrapper =
      PrivateAggregationPendingContributions::Wrapper(
          PrivateAggregationPendingContributions(20u, {}));
  wrapper.GetPendingContributions().AddUnconditionalContributions(
      unconditional_contributions);
  wrapper.GetPendingContributions().AddConditionalContributions(
      blink::mojom::PrivateAggregationErrorEvent::kReportSuccess,
      conditional_contributions);
  wrapper.GetPendingContributions().MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(wrapper), example_key,
      NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(kBudgeterResultHistogram,
                               PrivateAggregationBudgeter::RequestResult::
                                   kInsufficientSmallerScopeBudget,
                               1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_F(
    PrivateAggregationManagerImplErrorReportingEnabledTest,
    AllUnconditionalContributionsDenied_ConditionalContributionsStillTriggered) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unmerged_contributions = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/5,
              /*filtering_id=*/std::nullopt),
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/100,
              /*filtering_id=*/std::nullopt)};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unconditional_contributions = {unmerged_contributions[1]};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      conditional_contributions = {unmerged_contributions[0]};

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents expected_payload_contents =
      example_request.payload_contents();
  expected_payload_contents.contributions = conditional_contributions;

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(expected_payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  {
    testing::InSequence seq;

    EXPECT_CALL(*budgeter_, InspectBudgetAndLock(unconditional_contributions,
                                                 example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kInsufficientLargerScopeBudget,
                              {ResultForContribution::kDenied}),
            PrivateAggregationBudgeter::Lock::CreateForTesting(),
            PendingReportLimitResult::kNotAtLimit)));
    EXPECT_CALL(*budgeter_,
                ConsumeBudget(_, conditional_contributions, example_key, _))
        .WillOnce(base::test::RunOnceCallback<3>(BudgetQueryResult(
            RequestResult::kApproved, {ResultForContribution::kApproved})));

    EXPECT_CALL(*aggregation_service_,
                ScheduleReport(ReportRequestIs(expected_request)));
  }

  PrivateAggregationPendingContributions::Wrapper wrapper =
      PrivateAggregationPendingContributions::Wrapper(
          PrivateAggregationPendingContributions(20u, {}));
  wrapper.GetPendingContributions().AddUnconditionalContributions(
      unconditional_contributions);
  wrapper.GetPendingContributions().AddConditionalContributions(
      blink::mojom::PrivateAggregationErrorEvent::kInsufficientBudget,
      conditional_contributions);
  wrapper.GetPendingContributions().MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(wrapper), example_key,
      NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kInsufficientLargerScopeBudget,
      1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_F(PrivateAggregationManagerImplErrorReportingEnabledTest,
       PendingLimitNotReached_AssociatedContributionNotTriggered) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unmerged_contributions = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/5,
              /*filtering_id=*/std::nullopt),
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/100,
              /*filtering_id=*/std::nullopt)};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unconditional_contributions = {unmerged_contributions[1]};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      conditional_contributions = {unmerged_contributions[0]};

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents expected_payload_contents =
      example_request.payload_contents();
  expected_payload_contents.contributions = unconditional_contributions;

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(expected_payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  {
    testing::InSequence seq;

    EXPECT_CALL(*budgeter_, InspectBudgetAndLock(unconditional_contributions,
                                                 example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kApproved,
                              {ResultForContribution::kApproved}),
            PrivateAggregationBudgeter::Lock::CreateForTesting(),
            PendingReportLimitResult::kNotAtLimit)));
    EXPECT_CALL(*budgeter_,
                ConsumeBudget(_, unconditional_contributions, example_key, _))
        .WillOnce(base::test::RunOnceCallback<3>(BudgetQueryResult(
            RequestResult::kApproved, {ResultForContribution::kApproved})));

    EXPECT_CALL(*aggregation_service_,
                ScheduleReport(ReportRequestIs(expected_request)));
  }

  PrivateAggregationPendingContributions::Wrapper wrapper =
      PrivateAggregationPendingContributions::Wrapper(
          PrivateAggregationPendingContributions(20u, {}));
  wrapper.GetPendingContributions().AddUnconditionalContributions(
      unconditional_contributions);
  wrapper.GetPendingContributions().AddConditionalContributions(
      blink::mojom::PrivateAggregationErrorEvent::kPendingReportLimitReached,
      conditional_contributions);
  wrapper.GetPendingContributions().MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(wrapper), example_key,
      NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

TEST_F(PrivateAggregationManagerImplErrorReportingEnabledTest,
       PendingLimitReached_AssociatedContributionTriggered) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationCallerApi::kProtectedAudience)
          .value();

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unmerged_contributions = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/5,
              /*filtering_id=*/std::nullopt),
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/100,
              /*filtering_id=*/std::nullopt)};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unconditional_contributions = {unmerged_contributions[1]};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      conditional_contributions = {unmerged_contributions[0]};

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents expected_payload_contents =
      example_request.payload_contents();
  expected_payload_contents.contributions = {
      blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/105,
          /*filtering_id=*/std::nullopt)};

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(expected_payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  {
    testing::InSequence seq;

    EXPECT_CALL(*budgeter_, InspectBudgetAndLock(unconditional_contributions,
                                                 example_key, _))
        .WillOnce(base::test::RunOnceCallback<2>(InspectBudgetCallResult(
            BudgetQueryResult(RequestResult::kApproved,
                              {ResultForContribution::kApproved}),
            PrivateAggregationBudgeter::Lock::CreateForTesting(),
            PendingReportLimitResult::kAtLimit)));
    EXPECT_CALL(*budgeter_,
                ConsumeBudget(_, unmerged_contributions, example_key, _))
        .WillOnce(base::test::RunOnceCallback<3>(BudgetQueryResult(
            RequestResult::kApproved, {ResultForContribution::kApproved,
                                       ResultForContribution::kApproved})));

    EXPECT_CALL(*aggregation_service_,
                ScheduleReport(ReportRequestIs(expected_request)));
  }

  PrivateAggregationPendingContributions::Wrapper wrapper =
      PrivateAggregationPendingContributions::Wrapper(
          PrivateAggregationPendingContributions(20u, {}));
  wrapper.GetPendingContributions().AddUnconditionalContributions(
      unconditional_contributions);
  wrapper.GetPendingContributions().AddConditionalContributions(
      blink::mojom::PrivateAggregationErrorEvent::kPendingReportLimitReached,
      conditional_contributions);
  wrapper.GetPendingContributions().MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  auto [generator, contributions] = CloneAndSplitOutGenerator(expected_request);
  manager_.OnReportRequestDetailsReceivedFromHost(
      std::move(generator), std::move(wrapper), example_key,
      NullReportBehavior::kDontSendReport);

  histogram.ExpectUniqueSample(
      kBudgeterResultHistogram,
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
  histogram.ExpectUniqueSample(
      kManagerResultHistogram,
      PrivateAggregationManagerImpl::RequestResult::kSentWithContributions, 1);
}

}  // namespace content
