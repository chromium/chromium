// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregatable_report_scheduler.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "content/browser/aggregation_service/aggregation_service_observer.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::content::aggregation_service::RequestIdIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::SizeIs;
using ::testing::StrictMock;

auto InvokeCallback(std::optional<AggregatableReport> report,
                    AggregationService::AssemblyStatus status) {
  return [report = std::move(report), status = status](
             AggregatableReportRequest report_request,
             AggregationService::AssemblyCallback callback) {
    std::move(callback).Run(std::move(report_request), std::move(report),
                            status);
  };
}

AggregatableReport CreateExampleAggregatableReport() {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/std::nullopt);
  return AggregatableReport(std::move(payloads), "example_shared_info",
                            /*debug_key=*/std::nullopt,
                            /*additional_fields=*/{},
                            /*aggregation_coordinator_origin=*/std::nullopt);
}

}  // namespace

class MockAggregatableReportAssembler : public AggregatableReportAssembler {
 public:
  explicit MockAggregatableReportAssembler(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : AggregatableReportAssembler(
            /*storage_context=*/nullptr,
            std::move(url_loader_factory)) {}
  ~MockAggregatableReportAssembler() override = default;

  MOCK_METHOD(void,
              AssembleReport,
              (AggregatableReportRequest request, AssemblyCallback callback),
              (override));
};

class MockAggregatableReportSender : public AggregatableReportSender {
 public:
  explicit MockAggregatableReportSender(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : AggregatableReportSender(std::move(url_loader_factory)) {}
  ~MockAggregatableReportSender() override = default;

  MOCK_METHOD(void,
              SendReport,
              (const GURL& url,
               const base::Value& contents,
               std::optional<AggregatableReportRequest::DelayType> delay_type,
               ReportSentCallback callback),
              (override));
};

class MockAggregatableReportScheduler : public AggregatableReportScheduler {
 public:
  explicit MockAggregatableReportScheduler(
      AggregationServiceStorageContext* storage_context)
      : AggregatableReportScheduler(storage_context, base::DoNothing()) {}
  ~MockAggregatableReportScheduler() override = default;

  MOCK_METHOD(void,
              ScheduleRequest,
              (AggregatableReportRequest request),
              (override));

  MOCK_METHOD(void,
              NotifyInProgressRequestSucceeded,
              (AggregationServiceStorage::RequestId request_id),
              (override));

  MOCK_METHOD(bool,
              NotifyInProgressRequestFailed,
              (AggregationServiceStorage::RequestId request_id,
               int previous_failed_attempts),
              (override));
};

class MockAggregationServiceObserver : public AggregationServiceObserver {
 public:
  MockAggregationServiceObserver() = default;
  ~MockAggregationServiceObserver() override = default;

  MOCK_METHOD(void, OnRequestStorageModified, (), (override));

  MOCK_METHOD(void,
              OnReportHandled,
              (const AggregatableReportRequest& request,
               std::optional<AggregationServiceStorage::RequestId> id,
               const std::optional<AggregatableReport>& report,
               base::Time report_handle_time,
               ReportStatus status),
              (override));
};

class AggregationServiceImplTest : public testing::Test {
 public:
  AggregationServiceImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        storage_context_(task_environment_.GetMockClock()) {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    auto assembler =
        std::make_unique<MockAggregatableReportAssembler>(url_loader_factory);
    test_assembler_ = assembler.get();

    auto sender =
        std::make_unique<MockAggregatableReportSender>(url_loader_factory);
    test_sender_ = sender.get();

    auto scheduler =
        std::make_unique<MockAggregatableReportScheduler>(&storage_context_);
    test_scheduler_ = scheduler.get();

    service_impl_ = AggregationServiceImpl::CreateForTesting(
        /*run_in_memory=*/true, dir_.GetPath(),
        task_environment_.GetMockClock(), std::move(scheduler),
        std::move(assembler), std::move(sender));
  }

  void StoreReport(AggregatableReportRequest request) {
    service_impl_->storage_.AsyncCall(&AggregationServiceStorage::StoreRequest)
        .WithArgs(std::move(request));
  }

  void OnScheduledReportTimeReached(
      std::vector<AggregationServiceStorage::RequestAndId> requests) {
    service_impl_->OnScheduledReportTimeReached(std::move(requests));
  }

  // Verify bucket counts and/or total counts of relevant histograms nested
  // under "PrivacySandbox.AggregationService.ScheduledRequests".
  //
  // Args:
  //   `statuses`: The complete expected contents of the Status histogram.
  //   `num_retries_recorded`: The expected number of samples in the
  //                           NumRetriesBeforeSuccess histogram. (If you expect
  //                           one send with zero retries, that's one sample!)
  //   `num_assemblies`: The number of samples expected for the AssemblyTime
  //                     histogram.
  //   `num_sends`: The number of samples expected for the SendAttemptTime
  //                histogram.
  void VerifyScheduledHistograms(
      std::vector<base::Bucket> statuses = {},
      base::HistogramBase::Count num_successful_sends = 0,
      base::HistogramBase::Count num_assemblies = 0,
      base::HistogramBase::Count num_attempted_sends = 0) {
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            "PrivacySandbox.AggregationService.ScheduledRequests.Status"),
        base::BucketsAreArray(statuses));

    histogram_tester_.ExpectTotalCount(
        "PrivacySandbox.AggregationService.ScheduledRequests."
        "NumRetriesBeforeSuccess",
        num_successful_sends);

    histogram_tester_.ExpectTotalCount(
        "PrivacySandbox.AggregationService.ScheduledRequests.AssemblyTime",
        num_assemblies);

    histogram_tester_.ExpectTotalCount(
        "PrivacySandbox.AggregationService.ScheduledRequests.SendAttemptTime",
        num_attempted_sends);

    histogram_tester_.ExpectTotalCount(
        "PrivacySandbox.AggregationService.ScheduledRequests."
        "DelayFromOriginalReportTime",
        num_attempted_sends);
  }

  // Verify bucket counts and/or total counts of relevant histograms nested
  // under "PrivacySandbox.AggregationService.UnscheduledRequests".
  //
  // Args:
  //   `statuses`: The complete expected contents of the Status histogram.
  //   `num_assemblies`: The number of samples expected for the AssemblyTime
  //                     histogram.
  //   `num_sends`: The number of samples expected for the SendAttemptTime
  //                histogram.
  void VerifyUnscheduledHistograms(
      std::vector<base::Bucket> statuses = {},
      base::HistogramBase::Count num_assemblies = 0,
      base::HistogramBase::Count num_attempted_sends = 0) {
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            "PrivacySandbox.AggregationService.UnscheduledRequests.Status"),
        base::BucketsAreArray(statuses));

    histogram_tester_.ExpectTotalCount(
        "PrivacySandbox.AggregationService.UnscheduledRequests.AssemblyTime",
        num_assemblies);

    histogram_tester_.ExpectTotalCount(
        "PrivacySandbox.AggregationService.UnscheduledRequests.SendAttemptTime",
        num_attempted_sends);

    histogram_tester_.ExpectTotalCount(
        "PrivacySandbox.AggregationService.UnscheduledRequests."
        "DelayFromOriginalReportTime",
        num_attempted_sends);
  }

 protected:
  base::ScopedTempDir dir_;
  BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestAggregationServiceStorageContext storage_context_;

  std::unique_ptr<AggregationServiceImpl> service_impl_;
  raw_ptr<MockAggregatableReportAssembler> test_assembler_ = nullptr;
  raw_ptr<MockAggregatableReportSender> test_sender_ = nullptr;
  raw_ptr<MockAggregatableReportScheduler> test_scheduler_ = nullptr;

  base::HistogramTester histogram_tester_;
};

TEST_F(AggregationServiceImplTest, AssembleReport_Succeed) {
  EXPECT_CALL(*test_assembler_, AssembleReport)
      .WillOnce(
          InvokeCallback(CreateExampleAggregatableReport(),
                         AggregatableReportAssembler::AssemblyStatus::kOk));

  base::RunLoop run_loop;
  service_impl_->AssembleReport(
      aggregation_service::CreateExampleRequest(),
      base::BindLambdaForTesting(
          [&](AggregatableReportRequest request,
              std::optional<AggregatableReport> report,
              AggregationService::AssemblyStatus status) {
            EXPECT_TRUE(report.has_value());
            EXPECT_EQ(status, AggregationService::AssemblyStatus::kOk);
            run_loop.Quit();
          }));

  run_loop.Run();

  VerifyScheduledHistograms();
  VerifyUnscheduledHistograms();
}

TEST_F(AggregationServiceImplTest, AssembleReport_Fail) {
  EXPECT_CALL(*test_assembler_, AssembleReport)
      .WillOnce(InvokeCallback(
          /*report=*/std::nullopt,
          AggregatableReportAssembler::AssemblyStatus::kPublicKeyFetchFailed));

  base::RunLoop run_loop;
  service_impl_->AssembleReport(
      aggregation_service::CreateExampleRequest(),
      base::BindLambdaForTesting(
          [&](AggregatableReportRequest request,
              std::optional<AggregatableReport> report,
              AggregationService::AssemblyStatus status) {
            EXPECT_FALSE(report.has_value());
            EXPECT_EQ(
                status,
                AggregationService::AssemblyStatus::kPublicKeyFetchFailed);
            run_loop.Quit();
          }));

  run_loop.Run();

  VerifyScheduledHistograms();
  VerifyUnscheduledHistograms();
}

TEST_F(AggregationServiceImplTest, SendReport) {
  EXPECT_CALL(*test_sender_, SendReport)
      .WillOnce(base::test::RunOnceCallback<3>(
          AggregatableReportSender::RequestStatus::kOk));

  base::RunLoop run_loop;
  service_impl_->SendReport(
      GURL("https://example.com/reports"), CreateExampleAggregatableReport(),
      AggregatableReportRequest::DelayType::Unscheduled,
      base::BindLambdaForTesting([&](AggregationService::SendStatus status) {
        EXPECT_EQ(status, AggregationService::SendStatus::kOk);
        run_loop.Quit();
      }));

  run_loop.Run();

  VerifyScheduledHistograms();
  VerifyUnscheduledHistograms();
}

TEST_F(AggregationServiceImplTest, ScheduleReport_Success) {
  std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids;

  // Request IDs begin at 1.
  AggregationServiceStorage::RequestId request_id(1);

  EXPECT_CALL(*test_scheduler_, ScheduleRequest)
      .WillOnce([&](AggregatableReportRequest request) {
        requests_and_ids.push_back(AggregationServiceStorage::RequestAndId{
            .request = std::move(request),
            .id = request_id,
        });
      });

  service_impl_->ScheduleReport(aggregation_service::CreateExampleRequest());

  EXPECT_CALL(*test_assembler_, AssembleReport)
      .WillOnce(
          InvokeCallback(CreateExampleAggregatableReport(),
                         AggregatableReportAssembler::AssemblyStatus::kOk));
  EXPECT_CALL(*test_sender_, SendReport)
      .WillOnce(base::test::RunOnceCallback<3>(
          AggregatableReportSender::RequestStatus::kOk));
  EXPECT_CALL(*test_scheduler_, NotifyInProgressRequestSucceeded(request_id));

  EXPECT_THAT(requests_and_ids, SizeIs(1));
  OnScheduledReportTimeReached(std::move(requests_and_ids));

  VerifyScheduledHistograms(
      /*statuses=*/{{AggregationServiceObserver::ReportStatus::kSent, 1}},
      /*num_successful_sends=*/1,
      /*num_assemblies=*/1,
      /*num_attempted_sends=*/1);
  VerifyUnscheduledHistograms();

  histogram_tester_.ExpectUniqueSample(
      "PrivacySandbox.AggregationService.ScheduledRequests."
      "NumRetriesBeforeSuccess",
      /*sample=*/0, 1);
}

TEST_F(AggregationServiceImplTest, ScheduleReport_FailedAssembly) {
  std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids;

  // Request IDs begin at 1.
  AggregationServiceStorage::RequestId request_id(1);

  EXPECT_CALL(*test_scheduler_, ScheduleRequest)
      .WillOnce([&](AggregatableReportRequest request) {
        requests_and_ids.push_back(AggregationServiceStorage::RequestAndId{
            .request = std::move(request),
            .id = request_id,
        });
      });

  AggregatableReportRequest request = aggregation_service::CreateExampleRequest(
      /*aggregation_mode=*/blink::mojom::AggregationServiceMode::kDefault,
      /*failed_send_attempts=*/AggregatableReportScheduler::kMaxRetries);

  service_impl_->ScheduleReport(std::move(request));

  EXPECT_CALL(*test_assembler_, AssembleReport)
      .WillOnce(InvokeCallback(
          /*report=*/std::nullopt,
          AggregatableReportAssembler::AssemblyStatus::kAssemblyFailed));
  EXPECT_CALL(*test_scheduler_,
              NotifyInProgressRequestFailed(
                  request_id, AggregatableReportScheduler::kMaxRetries))
      .WillOnce(::testing::Return(false));

  StrictMock<MockAggregationServiceObserver> observer;
  base::ScopedObservation<AggregationService, AggregationServiceObserver>
      observation(&observer);
  observation.Observe(service_impl_.get());

  EXPECT_CALL(observer, OnRequestStorageModified);
  EXPECT_CALL(observer,
              OnReportHandled(
                  _, Optional(request_id), _, _,
                  AggregationServiceObserver::ReportStatus::kFailedToAssemble));

  EXPECT_THAT(requests_and_ids, SizeIs(1));
  OnScheduledReportTimeReached(std::move(requests_and_ids));

  VerifyScheduledHistograms(
      /*statuses=*/{{AggregationServiceObserver::ReportStatus::
                         kFailedToAssemble,
                     1}},
      /*num_successful_sends=*/0,
      /*num_assemblies=*/1,
      /*num_attempted_sends=*/0);
  VerifyUnscheduledHistograms();
}

TEST_F(AggregationServiceImplTest, ScheduleReport_FailedSending) {
  std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids;

  // Request IDs begin at 1.
  AggregationServiceStorage::RequestId request_id(1);

  EXPECT_CALL(*test_scheduler_, ScheduleRequest)
      .WillOnce([&](AggregatableReportRequest request) {
        requests_and_ids.push_back(AggregationServiceStorage::RequestAndId{
            .request = std::move(request),
            .id = request_id,
        });
      });

  service_impl_->ScheduleReport(aggregation_service::CreateExampleRequest());

  EXPECT_CALL(*test_assembler_, AssembleReport)
      .WillOnce(
          InvokeCallback(CreateExampleAggregatableReport(),
                         AggregatableReportAssembler::AssemblyStatus::kOk));
  EXPECT_CALL(*test_sender_, SendReport)
      .WillOnce(base::test::RunOnceCallback<3>(
          AggregatableReportSender::RequestStatus::kNetworkError));
  EXPECT_CALL(*test_scheduler_, NotifyInProgressRequestFailed(
                                    request_id, /*previous_failed_attempts=*/0))
      .WillOnce(::testing::Return(true));

  StrictMock<MockAggregationServiceObserver> observer;
  base::ScopedObservation<AggregationService, AggregationServiceObserver>
      observation(&observer);
  observation.Observe(service_impl_.get());

  EXPECT_CALL(observer, OnRequestStorageModified);
  // The report should not be considered handled when it is scheduled for a
  // retry
  EXPECT_CALL(observer, OnReportHandled).Times(0);

  EXPECT_THAT(requests_and_ids, SizeIs(1));
  OnScheduledReportTimeReached(std::move(requests_and_ids));

  VerifyScheduledHistograms(
      /*statuses=*/{},
      /*num_successful_sends=*/0,
      /*num_assemblies=*/1,
      /*num_attempted_sends=*/1);
  VerifyUnscheduledHistograms();
}

TEST_F(AggregationServiceImplTest,
       MultipleReportsReturnedFromScheduler_Success) {
  std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids;

  // Request IDs begin at 1.
  AggregationServiceStorage::RequestId request_id_1(1);
  AggregationServiceStorage::RequestId request_id_2(2);

  EXPECT_CALL(*test_scheduler_, ScheduleRequest)
      .WillOnce([&](AggregatableReportRequest request) {
        requests_and_ids.push_back(AggregationServiceStorage::RequestAndId{
            .request = std::move(request),
            .id = request_id_1,
        });
      })
      .WillOnce([&](AggregatableReportRequest request) {
        requests_and_ids.push_back(AggregationServiceStorage::RequestAndId{
            .request = std::move(request),
            .id = request_id_2,
        });
      });

  AggregatableReportRequest request_1 =
      aggregation_service::CreateExampleRequest();
  AggregatableReportRequest request_2 =
      aggregation_service::CreateExampleRequest(
          /*aggregation_mode=*/blink::mojom::AggregationServiceMode::kDefault,
          /*failed_send_attempts=*/2);

  service_impl_->ScheduleReport(std::move(request_1));
  service_impl_->ScheduleReport(std::move(request_2));

  EXPECT_CALL(*test_assembler_, AssembleReport)
      .WillRepeatedly(
          InvokeCallback(CreateExampleAggregatableReport(),
                         AggregatableReportAssembler::AssemblyStatus::kOk));
  EXPECT_CALL(*test_sender_, SendReport)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<3>(
          AggregatableReportSender::RequestStatus::kOk));

  EXPECT_CALL(*test_scheduler_, NotifyInProgressRequestSucceeded(request_id_1));
  EXPECT_CALL(*test_scheduler_, NotifyInProgressRequestSucceeded(request_id_2));

  EXPECT_THAT(requests_and_ids, SizeIs(2));
  OnScheduledReportTimeReached(std::move(requests_and_ids));

  VerifyScheduledHistograms(
      /*statuses=*/{{AggregationServiceObserver::ReportStatus::kSent, 2}},
      /*num_successful_sends=*/2,
      /*num_assemblies=*/2,
      /*num_attempted_sends=*/2);
  VerifyUnscheduledHistograms();

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.AggregationService.ScheduledRequests."
      "NumRetriesBeforeSuccess",
      /*sample=*/0, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.AggregationService.ScheduledRequests."
      "NumRetriesBeforeSuccess",
      /*sample=*/2, 1);
}

TEST_F(AggregationServiceImplTest, AssembleAndSendReport_Success) {
  EXPECT_CALL(*test_assembler_, AssembleReport)
      .WillOnce(
          InvokeCallback(CreateExampleAggregatableReport(),
                         AggregatableReportAssembler::AssemblyStatus::kOk));

  EXPECT_CALL(*test_sender_, SendReport)
      .WillOnce(base::test::RunOnceCallback<3>(
          AggregatableReportSender::RequestStatus::kOk));

  StrictMock<MockAggregationServiceObserver> observer;
  base::ScopedObservation<AggregationService, AggregationServiceObserver>
      observation(&observer);
  observation.Observe(service_impl_.get());

  EXPECT_CALL(observer,
              OnReportHandled(_, Eq(std::nullopt), _, _,
                              AggregationServiceObserver::ReportStatus::kSent));

  // The scheduler should not have been interacted with.
  EXPECT_CALL(*test_scheduler_, NotifyInProgressRequestSucceeded).Times(0);
  EXPECT_CALL(*test_scheduler_, NotifyInProgressRequestFailed).Times(0);

  service_impl_->AssembleAndSendReport(
      aggregation_service::CreateExampleRequest());

  VerifyScheduledHistograms();
  VerifyUnscheduledHistograms(
      /*statuses=*/
      {{AggregationServiceObserver::ReportStatus::kSent, 1}},
      /*num_assemblies=*/1,
      /*num_attempted_sends=*/1);
}

TEST_F(AggregationServiceImplTest, AssembleAndSendReport_FailedAssembly) {
  EXPECT_CALL(*test_assembler_, AssembleReport)
      .WillOnce(InvokeCallback(
          /*report=*/std::nullopt,
          AggregatableReportAssembler::AssemblyStatus::kAssemblyFailed));

  StrictMock<MockAggregationServiceObserver> observer;
  base::ScopedObservation<AggregationService, AggregationServiceObserver>
      observation(&observer);
  observation.Observe(service_impl_.get());

  EXPECT_CALL(observer,
              OnReportHandled(
                  _, Eq(std::nullopt), _, _,
                  AggregationServiceObserver::ReportStatus::kFailedToAssemble));

  // The scheduler should not have been interacted with.
  EXPECT_CALL(*test_scheduler_, NotifyInProgressRequestSucceeded).Times(0);
  EXPECT_CALL(*test_scheduler_, NotifyInProgressRequestFailed).Times(0);

  service_impl_->AssembleAndSendReport(
      aggregation_service::CreateExampleRequest());

  VerifyScheduledHistograms();
  VerifyUnscheduledHistograms(
      /*statuses=*/{{AggregationServiceObserver::ReportStatus::
                         kFailedToAssemble,
                     1}},
      /*num_assemblies=*/1);
}

TEST_F(AggregationServiceImplTest, AssembleAndSendReport_FailedSender) {
  EXPECT_CALL(*test_assembler_, AssembleReport)
      .WillOnce(
          InvokeCallback(CreateExampleAggregatableReport(),
                         AggregatableReportAssembler::AssemblyStatus::kOk));

  EXPECT_CALL(*test_sender_, SendReport)
      .WillOnce(base::test::RunOnceCallback<3>(
          AggregatableReportSender::RequestStatus::kNetworkError));

  StrictMock<MockAggregationServiceObserver> observer;
  base::ScopedObservation<AggregationService, AggregationServiceObserver>
      observation(&observer);
  observation.Observe(service_impl_.get());

  EXPECT_CALL(
      observer,
      OnReportHandled(_, Eq(std::nullopt), _, _,
                      AggregationServiceObserver::ReportStatus::kFailedToSend));

  // The scheduler should not have been interacted with.
  EXPECT_CALL(*test_scheduler_, NotifyInProgressRequestSucceeded).Times(0);
  EXPECT_CALL(*test_scheduler_, NotifyInProgressRequestFailed).Times(0);

  service_impl_->AssembleAndSendReport(
      aggregation_service::CreateExampleRequest());

  VerifyScheduledHistograms();
  VerifyUnscheduledHistograms(
      /*statuses=*/
      {{AggregationServiceObserver::ReportStatus::kFailedToSend, 1}},
      /*num_assemblies=*/1,
      /*num_attempted_sends=*/1);
}

TEST_F(AggregationServiceImplTest, GetPendingReportRequestsForWebUI) {
  StoreReport(aggregation_service::CreateExampleRequest(
      blink::mojom::AggregationServiceMode::kDefault,
      /*failed_send_attempts=*/0,
      /*aggregation_coordinator_origin=*/std::nullopt,
      AggregatableReportRequest::DelayType::ScheduledWithFullDelay));
  StoreReport(aggregation_service::CreateExampleRequest(
      blink::mojom::AggregationServiceMode::kDefault,
      /*failed_send_attempts=*/0,
      /*aggregation_coordinator_origin=*/std::nullopt,
      AggregatableReportRequest::DelayType::ScheduledWithFullDelay));

  base::RunLoop run_loop;
  service_impl_->GetPendingReportRequestsForWebUI(base::BindLambdaForTesting(
      [&](std::vector<AggregationServiceStorage::RequestAndId>
              requests_and_ids) {
        // IDs autoincrement from 1.
        EXPECT_THAT(
            requests_and_ids,
            ElementsAre(RequestIdIs(AggregationServiceStorage::RequestId(1)),
                        RequestIdIs(AggregationServiceStorage::RequestId(2))));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(AggregationServiceImplTest, SendReportsForWebUI) {
  StoreReport(aggregation_service::CreateExampleRequest(
      blink::mojom::AggregationServiceMode::kDefault,
      /*failed_send_attempts=*/0,
      /*aggregation_coordinator_origin=*/std::nullopt,
      AggregatableReportRequest::DelayType::ScheduledWithFullDelay));

  EXPECT_CALL(*test_assembler_, AssembleReport)
      .WillOnce(
          InvokeCallback(CreateExampleAggregatableReport(),
                         AggregatableReportAssembler::AssemblyStatus::kOk));

  base::RunLoop run_loop;
  EXPECT_CALL(*test_sender_, SendReport)
      .WillOnce(
          testing::DoAll(base::test::RunOnceClosure(run_loop.QuitClosure()),
                         base::test::RunOnceCallback<3>(
                             AggregatableReportSender::RequestStatus::kOk)));

  // IDs autoincrement from 1.
  AggregationServiceStorage::RequestId request_id(1);

  StrictMock<MockAggregationServiceObserver> observer;
  base::ScopedObservation<AggregationService, AggregationServiceObserver>
      observation(&observer);
  observation.Observe(service_impl_.get());

  EXPECT_CALL(observer, OnRequestStorageModified);
  EXPECT_CALL(observer, OnReportHandled(_, Optional(request_id), _, _,
                                        AggregationServiceObserver::kSent));

  service_impl_->SendReportsForWebUI({request_id}, base::DoNothing());

  run_loop.Run();
}

TEST_F(AggregationServiceImplTest, ClearData_NotifyObservers) {
  StrictMock<MockAggregationServiceObserver> observer;
  base::ScopedObservation<AggregationService, AggregationServiceObserver>
      observation(&observer);
  observation.Observe(service_impl_.get());

  EXPECT_CALL(observer, OnRequestStorageModified);

  base::RunLoop run_loop;
  service_impl_->ClearData(/*delete_begin=*/base::Time::Min(),
                           /*delete_end=*/base::Time::Max(),
                           /*filter=*/base::NullCallback(),
                           run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace content
