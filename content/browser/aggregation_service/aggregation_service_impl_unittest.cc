// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_impl.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregatable_report_scheduler.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

// TODO(alexmt): Consider rewriting these tests using gmock.

class TestAggregatableReportAssembler : public AggregatableReportAssembler {
 public:
  explicit TestAggregatableReportAssembler(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : AggregatableReportAssembler(
            /*storage_context=*/nullptr,
            std::move(url_loader_factory)) {}
  ~TestAggregatableReportAssembler() override = default;

  void AssembleReport(AggregatableReportRequest request,
                      AssemblyCallback callback) override {
    callbacks_.emplace(unique_id_counter_++, std::move(callback));
  }

  void TriggerResponse(int64_t report_id,
                       absl::optional<AggregatableReport> report,
                       AssemblyStatus status) {
    ASSERT_TRUE(base::Contains(callbacks_, report_id));
    ASSERT_EQ(report.has_value(), status == AssemblyStatus::kOk);

    std::move(callbacks_[report_id]).Run(std::move(report), status);
    callbacks_.erase(report_id);
  }

 private:
  int64_t unique_id_counter_ = 0;
  std::map<int64_t, AssemblyCallback> callbacks_;
};

class TestAggregatableReportSender : public AggregatableReportSender {
 public:
  explicit TestAggregatableReportSender(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : AggregatableReportSender(std::move(url_loader_factory)) {}
  ~TestAggregatableReportSender() override = default;

  void SendReport(const GURL& url,
                  const base::Value& contents,
                  ReportSentCallback callback) override {
    callbacks_.emplace(unique_id_counter_++, std::move(callback));
  }

  void TriggerResponse(int64_t report_id, RequestStatus status) {
    ASSERT_TRUE(base::Contains(callbacks_, report_id));
    std::move(callbacks_[report_id]).Run(status);
    callbacks_.erase(report_id);
  }

 private:
  int64_t unique_id_counter_ = 0;
  std::map<int64_t, ReportSentCallback> callbacks_;
};

class TestAggregatableReportScheduler : public AggregatableReportScheduler {
 public:
  TestAggregatableReportScheduler(
      AggregationServiceStorageContext* storage_context,
      base::RepeatingCallback<
          void(std::vector<AggregationServiceStorage::RequestAndId>)>
          on_scheduled_report_time_reached)
      : AggregatableReportScheduler(storage_context, base::DoNothing()),
        on_scheduled_report_time_reached_(
            std::move(on_scheduled_report_time_reached)) {}
  ~TestAggregatableReportScheduler() override = default;

  void ScheduleRequest(AggregatableReportRequest request) override {
    scheduled_reports_.emplace(unique_id_counter_++, std::move(request));
  }

  void NotifyInProgressRequestSucceeded(
      AggregationServiceStorage::RequestId request_id) override {
    completed_requests_status_[request_id] = true;
  }

  void NotifyInProgressRequestFailed(
      AggregationServiceStorage::RequestId request_id) override {
    completed_requests_status_[request_id] = false;
  }

  void TriggerReportingTime(
      std::vector<AggregationServiceStorage::RequestId> request_ids) {
    std::vector<AggregationServiceStorage::RequestAndId> return_value;
    for (AggregationServiceStorage::RequestId request_id : request_ids) {
      ASSERT_TRUE(base::Contains(scheduled_reports_, request_id));
      return_value.push_back(AggregationServiceStorage::RequestAndId{
          .request = std::move(scheduled_reports_.at(request_id)),
          .id = request_id});
      scheduled_reports_.erase(request_id);
    }
    on_scheduled_report_time_reached_.Run(std::move(return_value));
  }

  // Returns a boolean representing whether the request was successfully
  // completed. Returns absl::nullopt if the request has not yet completed.
  absl::optional<bool> WasRequestSuccessful(
      AggregationServiceStorage::RequestId request_id) {
    if (!base::Contains(completed_requests_status_, request_id)) {
      return absl::nullopt;
    }
    return completed_requests_status_[request_id];
  }

 private:
  base::RepeatingCallback<void(
      std::vector<AggregationServiceStorage::RequestAndId>)>
      on_scheduled_report_time_reached_;
  int64_t unique_id_counter_ = 1;
  base::flat_map<AggregationServiceStorage::RequestId,
                 AggregatableReportRequest>
      scheduled_reports_;

  // Each completed request's ID is the key, with value whether it was completed
  // successfully.
  base::flat_map<AggregationServiceStorage::RequestId, bool>
      completed_requests_status_;
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
        std::make_unique<TestAggregatableReportAssembler>(url_loader_factory);
    test_assembler_ = assembler.get();

    auto sender =
        std::make_unique<TestAggregatableReportSender>(url_loader_factory);
    test_sender_ = sender.get();

    auto scheduler = std::make_unique<TestAggregatableReportScheduler>(
        &storage_context_,
        base::BindLambdaForTesting(
            [this](std::vector<AggregationServiceStorage::RequestAndId>
                       requests_and_ids) {
              service_impl_->OnScheduledReportTimeReached(
                  std::move(requests_and_ids));
            }));
    test_scheduler_ = scheduler.get();

    service_impl_ = AggregationServiceImpl::CreateForTesting(
        /*run_in_memory=*/true, dir_.GetPath(),
        task_environment_.GetMockClock(), std::move(scheduler),
        std::move(assembler), std::move(sender));
  }

  void AssembleReport(AggregatableReportRequest request) {
    service()->AssembleReport(
        std::move(request), base::BindLambdaForTesting(
                                [&](absl::optional<AggregatableReport> report,
                                    AggregationService::AssemblyStatus status) {
                                  last_assembled_report_ = std::move(report);
                                  last_assembly_status_ = status;
                                }));
  }

  void SendReport(const GURL& url, const AggregatableReport& report) {
    service()->SendReport(
        url, report,
        base::BindLambdaForTesting([&](AggregationService::SendStatus status) {
          last_send_status_ = status;
        }));
  }

  void ScheduleReport(AggregatableReportRequest request) {
    service()->ScheduleReport(std::move(request));
  }

  AggregationServiceImpl* service() { return service_impl_.get(); }
  TestAggregatableReportAssembler* assembler() { return test_assembler_; }
  TestAggregatableReportSender* sender() { return test_sender_; }
  TestAggregatableReportScheduler* scheduler() { return test_scheduler_; }

  // Returns `absl::nullopt` if no report callback has been run or if the last
  // assembly had an error.
  const absl::optional<AggregatableReport>& last_assembled_report() const {
    return last_assembled_report_;
  }

  // Returns `absl::nullopt` if no report callback has been run.
  const absl::optional<AggregationService::AssemblyStatus>&
  last_assembly_status() const {
    return last_assembly_status_;
  }

  // Returns `absl::nullopt` if no report callback has been run.
  const absl::optional<AggregationService::SendStatus>& last_send_status()
      const {
    return last_send_status_;
  }

 private:
  base::ScopedTempDir dir_;
  BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<AggregationServiceImpl> service_impl_;
  TestAggregationServiceStorageContext storage_context_;
  raw_ptr<TestAggregatableReportAssembler> test_assembler_ = nullptr;
  raw_ptr<TestAggregatableReportSender> test_sender_ = nullptr;
  raw_ptr<TestAggregatableReportScheduler> test_scheduler_ = nullptr;

  absl::optional<AggregatableReport> last_assembled_report_;
  absl::optional<AggregationService::AssemblyStatus> last_assembly_status_;

  absl::optional<AggregationService::SendStatus> last_send_status_;
};

TEST_F(AggregationServiceImplTest, AssembleReport_Succeed) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  AssembleReport(std::move(request));

  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/absl::nullopt);

  AggregatableReport report(std::move(payloads), "example_shared_info");
  assembler()->TriggerResponse(
      /*report_id=*/0, std::move(report),
      AggregatableReportAssembler::AssemblyStatus::kOk);

  EXPECT_TRUE(last_assembled_report().has_value());
  ASSERT_TRUE(last_assembly_status().has_value());
  EXPECT_EQ(last_assembly_status().value(),
            AggregationService::AssemblyStatus::kOk);
}

TEST_F(AggregationServiceImplTest, AssembleReport_Fail) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  AssembleReport(std::move(request));

  assembler()->TriggerResponse(
      /*report_id=*/0, absl::nullopt,
      AggregatableReportAssembler::AssemblyStatus::kPublicKeyFetchFailed);

  EXPECT_FALSE(last_assembled_report().has_value());
  ASSERT_TRUE(last_assembly_status().has_value());
  EXPECT_EQ(last_assembly_status().value(),
            AggregationService::AssemblyStatus::kPublicKeyFetchFailed);
}

TEST_F(AggregationServiceImplTest, SendReport) {
  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/absl::nullopt);

  AggregatableReport report(std::move(payloads), "example_shared_info");

  SendReport(GURL("https://example.com/reports"), report);

  sender()->TriggerResponse(/*report_id=*/0,
                            AggregatableReportSender::RequestStatus::kOk);

  ASSERT_TRUE(last_send_status().has_value());
  EXPECT_EQ(last_send_status().value(), AggregationService::SendStatus::kOk);
}

TEST_F(AggregationServiceImplTest, ScheduleReport_Success) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  ScheduleReport(std::move(request));

  // Request IDs begin at 1.
  scheduler()->TriggerReportingTime(
      /*request_ids=*/{AggregationServiceStorage::RequestId(1)});

  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/absl::nullopt);
  AggregatableReport report(std::move(payloads), "example_shared_info");

  assembler()->TriggerResponse(
      /*report_id=*/0, std::move(report),
      AggregatableReportAssembler::AssemblyStatus::kOk);

  sender()->TriggerResponse(/*report_id=*/0,
                            AggregatableReportSender::RequestStatus::kOk);

  ASSERT_TRUE(
      scheduler()
          ->WasRequestSuccessful(AggregationServiceStorage::RequestId(1))
          .has_value());
  EXPECT_TRUE(
      scheduler()
          ->WasRequestSuccessful(AggregationServiceStorage::RequestId(1))
          .value());
}

TEST_F(AggregationServiceImplTest, ScheduleReport_FailedAssembly) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  ScheduleReport(std::move(request));

  // Request IDs begin at 1.
  scheduler()->TriggerReportingTime(
      /*request_ids=*/{AggregationServiceStorage::RequestId(1)});

  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/absl::nullopt);
  AggregatableReport report(std::move(payloads), "example_shared_info");

  assembler()->TriggerResponse(
      /*report_id=*/0, absl::nullopt,
      AggregatableReportAssembler::AssemblyStatus::kAssemblyFailed);

  ASSERT_TRUE(
      scheduler()
          ->WasRequestSuccessful(AggregationServiceStorage::RequestId(1))
          .has_value());
  EXPECT_FALSE(
      scheduler()
          ->WasRequestSuccessful(AggregationServiceStorage::RequestId(1))
          .value());
}

TEST_F(AggregationServiceImplTest, ScheduleReport_FailedSending) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  ScheduleReport(std::move(request));

  scheduler()->TriggerReportingTime(
      /*request_ids=*/{AggregationServiceStorage::RequestId(1)});

  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/absl::nullopt);
  AggregatableReport report(std::move(payloads), "example_shared_info");

  assembler()->TriggerResponse(
      /*report_id=*/0, std::move(report),
      AggregatableReportAssembler::AssemblyStatus::kOk);

  sender()->TriggerResponse(
      /*report_id=*/0, AggregatableReportSender::RequestStatus::kNetworkError);

  ASSERT_TRUE(
      scheduler()
          ->WasRequestSuccessful(AggregationServiceStorage::RequestId(1))
          .has_value());
  EXPECT_FALSE(
      scheduler()
          ->WasRequestSuccessful(AggregationServiceStorage::RequestId(1))
          .value());
}

TEST_F(AggregationServiceImplTest,
       MultipleReportsReturnedFromScheduler_Success) {
  AggregatableReportRequest request_1 =
      aggregation_service::CreateExampleRequest();
  AggregatableReportRequest request_2 =
      aggregation_service::CreateExampleRequest();

  ScheduleReport(std::move(request_1));
  ScheduleReport(std::move(request_2));

  // Request IDs begin at 1.
  scheduler()->TriggerReportingTime(
      /*request_ids=*/{AggregationServiceStorage::RequestId(1),
                       AggregationServiceStorage::RequestId(2)});

  std::vector<AggregatableReport::AggregationServicePayload> payloads;
  payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                        /*key_id=*/"key_1",
                        /*debug_cleartext_payload=*/absl::nullopt);
  AggregatableReport report_1(payloads, "example_shared_info");
  AggregatableReport report_2(payloads, "example_shared_info");

  assembler()->TriggerResponse(
      /*report_id=*/0, std::move(report_1),
      AggregatableReportAssembler::AssemblyStatus::kOk);
  assembler()->TriggerResponse(
      /*report_id=*/1, std::move(report_2),
      AggregatableReportAssembler::AssemblyStatus::kOk);

  sender()->TriggerResponse(/*report_id=*/0,
                            AggregatableReportSender::RequestStatus::kOk);
  sender()->TriggerResponse(/*report_id=*/1,
                            AggregatableReportSender::RequestStatus::kOk);

  ASSERT_TRUE(
      scheduler()
          ->WasRequestSuccessful(AggregationServiceStorage::RequestId(1))
          .has_value());
  EXPECT_TRUE(
      scheduler()
          ->WasRequestSuccessful(AggregationServiceStorage::RequestId(1))
          .value());

  ASSERT_TRUE(
      scheduler()
          ->WasRequestSuccessful(AggregationServiceStorage::RequestId(2))
          .has_value());
  EXPECT_TRUE(
      scheduler()
          ->WasRequestSuccessful(AggregationServiceStorage::RequestId(2))
          .value());
}

}  // namespace content
