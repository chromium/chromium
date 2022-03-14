// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_impl.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

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

class AggregationServiceImplTest : public testing::Test {
 public:
  AggregationServiceImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
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

    service_impl_ = AggregationServiceImpl::CreateForTesting(
        /*run_in_memory=*/true, dir_.GetPath(),
        task_environment_.GetMockClock(), std::move(assembler),
        std::move(sender));
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

  AggregationServiceImpl* service() { return service_impl_.get(); }
  TestAggregatableReportAssembler* assembler() { return test_assembler_; }
  TestAggregatableReportSender* sender() { return test_sender_; }

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
  raw_ptr<TestAggregatableReportAssembler> test_assembler_ = nullptr;
  raw_ptr<TestAggregatableReportSender> test_sender_ = nullptr;

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

}  // namespace content