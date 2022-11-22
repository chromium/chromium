// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/common/aggregatable_report.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using testing::_;
using testing::Invoke;
using testing::Return;

using Checkpoint = testing::MockFunction<void(int step)>;

// TODO(alexmt): Consider making FromJavaTime() constexpr.
const base::Time kExampleTime = base::Time::FromJavaTime(1652984901234);

constexpr char kExampleOriginUrl[] = "https://origin.example";
constexpr char kExampleMainFrameUrl[] = "https://main_frame.example";

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

  using PrivateAggregationManagerImpl::OnReportRequestReceivedFromHost;

  AggregationService* GetAggregationService() override {
    return aggregation_service_.get();
  }

 private:
  std::unique_ptr<AggregationService> aggregation_service_;
};

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
          PrivateAggregationBudgetKey::Api::kFledge)
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
    EXPECT_CALL(*budgeter_,
                ConsumeBudget(
                    expected_request.payload_contents().contributions[0].value,
                    example_key, _))
        .WillOnce(Invoke(
            [&checkpoint](
                int, const PrivateAggregationBudgetKey&,
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

  manager_.OnReportRequestReceivedFromHost(
      aggregation_service::CloneReportRequest(expected_request), example_key);

  histogram.ExpectUniqueSample(
      "PrivacySandbox.PrivateAggregation.Budgeter.RequestResult",
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
}

TEST_F(PrivateAggregationManagerImplTest,
       ReportRequestWithMultipleContributions_CorrectBudgetRequested) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge)
          .value();

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.contributions = {
      mojom::AggregatableReportHistogramContribution(/*bucket=*/123,
                                                     /*value=*/100),
      mojom::AggregatableReportHistogramContribution(/*bucket=*/123,
                                                     /*value=*/5),
      mojom::AggregatableReportHistogramContribution(/*bucket=*/456,
                                                     /*value=*/20)};

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*budgeter_, ConsumeBudget(/*budget=*/125, example_key, _))
        .WillOnce(Invoke(
            [&checkpoint](
                int, const PrivateAggregationBudgetKey&,
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

  manager_.OnReportRequestReceivedFromHost(
      aggregation_service::CloneReportRequest(expected_request), example_key);

  histogram.ExpectUniqueSample(
      "PrivacySandbox.PrivateAggregation.Budgeter.RequestResult",
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
}

TEST_F(PrivateAggregationManagerImplTest,
       BudgetRequestRejected_RequestNotScheduled) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge)
          .value();

  AggregatableReportRequest expected_request =
      aggregation_service::CreateExampleRequest();
  ASSERT_EQ(expected_request.payload_contents().contributions.size(), 1u);

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*budgeter_,
                ConsumeBudget(
                    expected_request.payload_contents().contributions[0].value,
                    example_key, _))
        .WillOnce(Invoke([&checkpoint](
                             int, const PrivateAggregationBudgetKey&,
                             base::OnceCallback<void(
                                 PrivateAggregationBudgeter::RequestResult)>
                                 on_done) {
          checkpoint.Call(1);
          std::move(on_done).Run(
              PrivateAggregationBudgeter::RequestResult::kInsufficientBudget);
        }));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*aggregation_service_, ScheduleReport).Times(0);
  }

  checkpoint.Call(0);

  manager_.OnReportRequestReceivedFromHost(
      aggregation_service::CloneReportRequest(expected_request), example_key);

  histogram.ExpectUniqueSample(
      "PrivacySandbox.PrivateAggregation.Budgeter.RequestResult",
      PrivateAggregationBudgeter::RequestResult::kInsufficientBudget, 1);
}

TEST_F(PrivateAggregationManagerImplTest,
       BudgetExceedsIntegerLimits_BudgetRejectedWithoutRequest) {
  base::HistogramTester histogram;

  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_origin, kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge)
          .value();

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.contributions = {
      mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/std::numeric_limits<int>::max()),
      mojom::AggregatableReportHistogramContribution(/*bucket=*/456,
                                                     /*value=*/1)};

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(payload_contents,
                                        example_request.shared_info().Clone())
          .value();

  EXPECT_CALL(*budgeter_, ConsumeBudget).Times(0);
  EXPECT_CALL(*aggregation_service_, ScheduleReport).Times(0);
  manager_.OnReportRequestReceivedFromHost(
      aggregation_service::CloneReportRequest(expected_request), example_key);

  histogram.ExpectUniqueSample(
      "PrivacySandbox.PrivateAggregation.Budgeter.RequestResult",
      PrivateAggregationBudgeter::RequestResult::kRequestedMoreThanTotalBudget,
      1);
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
          PrivateAggregationBudgetKey::Api::kFledge)
          .value();

  absl::optional<AggregatableReportRequest> standard_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), shared_info.Clone(),
          /*reporting_path=*/"/example-reporting-path");
  absl::optional<AggregatableReportRequest> expected_debug_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(shared_info),
          /*reporting_path=*/
          "/.well-known/private-aggregation/debug/report-fledge");
  ASSERT_TRUE(standard_request.has_value());
  ASSERT_TRUE(expected_debug_request.has_value());

  EXPECT_CALL(
      *budgeter_,
      ConsumeBudget(standard_request->payload_contents().contributions[0].value,
                    example_key, _))
      .WillOnce(Invoke(
          [](int, const PrivateAggregationBudgetKey&,
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

  manager_.OnReportRequestReceivedFromHost(
      aggregation_service::CloneReportRequest(standard_request.value()),
      example_key);

  histogram.ExpectUniqueSample(
      "PrivacySandbox.PrivateAggregation.Budgeter.RequestResult",
      PrivateAggregationBudgeter::RequestResult::kApproved, 1);
}

TEST_F(PrivateAggregationManagerImplTest, DebugReportingPath) {
  base::HistogramTester histogram;

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();
  shared_info.debug_mode = AggregatableReportSharedInfo::DebugMode::kEnabled;

  absl::optional<AggregatableReportRequest> standard_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), shared_info.Clone(),
          /*reporting_path=*/"/example-reporting-path");
  ASSERT_TRUE(standard_request.has_value());

  PrivateAggregationBudgetKey fledge_key =
      PrivateAggregationBudgetKey::Create(
          example_request.shared_info().reporting_origin, kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge)
          .value();
  PrivateAggregationBudgetKey shared_storage_key =
      PrivateAggregationBudgetKey::Create(
          example_request.shared_info().reporting_origin, kExampleTime,
          PrivateAggregationBudgetKey::Api::kSharedStorage)
          .value();

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    EXPECT_CALL(*budgeter_, ConsumeBudget(_, fledge_key, _))
        .WillOnce(
            Invoke([](int, const PrivateAggregationBudgetKey&,
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
                    "/.well-known/private-aggregation/debug/report-fledge");
        }));
    // Still triggers the standard (non-debug) report.
    EXPECT_CALL(*aggregation_service_, ScheduleReport);

    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(*budgeter_, ConsumeBudget(_, shared_storage_key, _))
        .WillOnce(
            Invoke([](int, const PrivateAggregationBudgetKey&,
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

  manager_.OnReportRequestReceivedFromHost(
      aggregation_service::CloneReportRequest(standard_request.value()),
      fledge_key);
  checkpoint.Call(1);
  manager_.OnReportRequestReceivedFromHost(
      aggregation_service::CloneReportRequest(standard_request.value()),
      shared_storage_key);

  histogram.ExpectUniqueSample(
      "PrivacySandbox.PrivateAggregation.Budgeter.RequestResult",
      PrivateAggregationBudgeter::RequestResult::kApproved, 2);
}

TEST_F(PrivateAggregationManagerImplTest,
       BudgetDenied_DebugRequestNotAssembledOrSent) {
  base::HistogramTester histogram;

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();
  shared_info.debug_mode = AggregatableReportSharedInfo::DebugMode::kEnabled;

  PrivateAggregationBudgetKey example_key =
      PrivateAggregationBudgetKey::Create(
          example_request.shared_info().reporting_origin, kExampleTime,
          PrivateAggregationBudgetKey::Api::kFledge)
          .value();

  absl::optional<AggregatableReportRequest> standard_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), shared_info.Clone(),
          /*reporting_path=*/"/example-reporting-path");
  ASSERT_TRUE(standard_request.has_value());

  EXPECT_CALL(
      *budgeter_,
      ConsumeBudget(standard_request->payload_contents().contributions[0].value,
                    example_key, _))
      .WillOnce(Invoke(
          [](int, const PrivateAggregationBudgetKey&,
             base::OnceCallback<void(PrivateAggregationBudgeter::RequestResult)>
                 on_done) {
            std::move(on_done).Run(
                PrivateAggregationBudgeter::RequestResult::kBadValuesOnDisk);
          }));
  EXPECT_CALL(*aggregation_service_, AssembleAndSendReport).Times(0);
  EXPECT_CALL(*aggregation_service_, ScheduleReport).Times(0);

  manager_.OnReportRequestReceivedFromHost(
      aggregation_service::CloneReportRequest(standard_request.value()),
      example_key);

  histogram.ExpectUniqueSample(
      "PrivacySandbox.PrivateAggregation.Budgeter.RequestResult",
      PrivateAggregationBudgeter::RequestResult::kBadValuesOnDisk, 1);
}

TEST_F(PrivateAggregationManagerImplTest,
       BindNewReceiver_InvokesHostMethodIdentically) {
  const url::Origin example_origin =
      url::Origin::Create(GURL(kExampleOriginUrl));
  const url::Origin example_main_frame_origin =
      url::Origin::Create(GURL(kExampleMainFrameUrl));

  EXPECT_CALL(*host_,
              BindNewReceiver(example_origin, example_main_frame_origin,
                              PrivateAggregationBudgetKey::Api::kFledge, _))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationBudgetKey::Api::kFledge,
      mojo::PendingReceiver<mojom::PrivateAggregationHost>()));

  EXPECT_CALL(*host_, BindNewReceiver(
                          example_origin, example_main_frame_origin,
                          PrivateAggregationBudgetKey::Api::kSharedStorage, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(manager_.BindNewReceiver(
      example_origin, example_main_frame_origin,
      PrivateAggregationBudgetKey::Api::kSharedStorage,
      mojo::PendingReceiver<mojom::PrivateAggregationHost>()));
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

}  // namespace content
