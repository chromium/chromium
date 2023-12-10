// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_internals_handler_impl.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_observer.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/private_aggregation/private_aggregation_internals.mojom.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockObserver : public private_aggregation_internals::mojom::Observer {
 public:
  MOCK_METHOD(void, OnRequestStorageModified, (), (override));

  MOCK_METHOD(
      void,
      OnReportHandled,
      (private_aggregation_internals::mojom::WebUIAggregatableReportPtr report),
      (override));
};

void VerifyWebUIAggregatableReport(
    const private_aggregation_internals::mojom::WebUIAggregatableReport&
        web_report,
    const AggregatableReportRequest& request,
    std::optional<AggregationServiceStorage::RequestId> id,
    const std::optional<AggregatableReport>& report,
    base::Time report_time,
    private_aggregation_internals::mojom::ReportStatus status) {
  EXPECT_EQ(web_report.id, id);

  EXPECT_EQ(web_report.report_time,
            report_time.InMillisecondsFSinceUnixEpoch());
  EXPECT_EQ(web_report.api_identifier, request.shared_info().api_identifier);
  EXPECT_EQ(web_report.api_version, request.shared_info().api_version);
  EXPECT_EQ(web_report.report_url, request.GetReportingUrl());
  EXPECT_TRUE(web_report.status == status);

  ASSERT_EQ(web_report.contributions.size(),
            request.payload_contents().contributions.size());
  for (size_t i = 0; i < web_report.contributions.size(); ++i) {
    EXPECT_EQ(web_report.contributions[i]->bucket,
              request.payload_contents().contributions[i].bucket);
    EXPECT_EQ(web_report.contributions[i]->value,
              request.payload_contents().contributions[i].value);
  }

  base::Value report_body = base::test::ParseJson(web_report.report_body);
  ASSERT_TRUE(report_body.is_dict());
  const base::Value::Dict& report_body_dict = report_body.GetDict();

  if (report) {
    EXPECT_EQ(report_body_dict, report->GetAsJson());
  } else {
    const std::string* shared_info = report_body_dict.FindString("shared_info");
    ASSERT_TRUE(shared_info);
    EXPECT_EQ(*shared_info, request.shared_info().SerializeAsJson());

    const std::string* payloads =
        report_body_dict.FindString("aggregation_service_payloads");
    ASSERT_TRUE(payloads);
    EXPECT_EQ(*payloads, "Not generated prior to send");
  }
}

}  // namespace

class PrivateAggregationInternalsHandlerImplTest
    : public RenderViewHostTestHarness {
 public:
  PrivateAggregationInternalsHandlerImplTest() : receiver_(&observer_) {}

 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    web_ui_.set_web_contents(web_contents());

    StoragePartitionImpl* storage_partition_impl =
        static_cast<StoragePartitionImpl*>(web_ui_.GetWebContents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition());

    auto aggregation_service = std::make_unique<MockAggregationService>();
    aggregation_service_ = aggregation_service.get();
    storage_partition_impl->OverrideAggregationServiceForTesting(
        std::move(aggregation_service));

    auto private_aggregation_manager =
        std::make_unique<MockPrivateAggregationManagerImpl>(
            storage_partition_impl);
    private_aggregation_manager_ = private_aggregation_manager.get();
    storage_partition_impl->OverridePrivateAggregationManagerForTesting(
        std::move(private_aggregation_manager));

    internals_handler_ =
        std::make_unique<PrivateAggregationInternalsHandlerImpl>(
            &web_ui_, receiver_.BindNewPipeAndPassRemote(),
            remote_handler_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    // Resets `internals_handler_` to remove the observer from
    // `aggregation_service_` before it's destroyed.
    internals_handler_.reset();
    aggregation_service_ = nullptr;
    private_aggregation_manager_ = nullptr;
    web_ui_.set_web_contents(nullptr);
    RenderViewHostTestHarness::TearDown();
  }

  void OnRequestStorageModified() {
    internals_handler_->OnRequestStorageModified();
  }

  void OnReportHandled(const AggregatableReportRequest& request,
                       std::optional<AggregationServiceStorage::RequestId> id,
                       const std::optional<AggregatableReport>& report,
                       base::Time actual_report_time,
                       AggregationServiceObserver::ReportStatus status) {
    internals_handler_->OnReportHandled(request, id, report, actual_report_time,
                                        status);
  }

  TestWebUI web_ui_;
  raw_ptr<MockAggregationService> aggregation_service_;
  raw_ptr<MockPrivateAggregationManagerImpl> private_aggregation_manager_;
  mojo::Remote<private_aggregation_internals::mojom::Handler> remote_handler_;
  MockObserver observer_;
  mojo::Receiver<private_aggregation_internals::mojom::Observer> receiver_;
  std::unique_ptr<PrivateAggregationInternalsHandlerImpl> internals_handler_;
};

TEST_F(PrivateAggregationInternalsHandlerImplTest, GetReports) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  AggregationServiceStorage::RequestId id{20};

  EXPECT_CALL(*aggregation_service_, GetPendingReportRequestsForWebUI)
      .WillOnce([&](base::OnceCallback<void(
                        std::vector<AggregationServiceStorage::RequestAndId>)>
                        callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder()
                .AddRequestWithID(
                    aggregation_service::CloneReportRequest(request), id)
                .Build());
      });

  base::RunLoop run_loop;
  internals_handler_->GetReports(base::BindLambdaForTesting(
      [&](std::vector<
          private_aggregation_internals::mojom::WebUIAggregatableReportPtr>
              reports) {
        ASSERT_EQ(reports.size(), 1u);
        VerifyWebUIAggregatableReport(
            *reports.front(), request, id, /*report=*/std::nullopt,
            request.shared_info().scheduled_report_time,
            private_aggregation_internals::mojom::ReportStatus::kPending);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PrivateAggregationInternalsHandlerImplTest, SendReports) {
  EXPECT_CALL(*aggregation_service_,
              SendReportsForWebUI(
                  testing::ElementsAre(AggregationServiceStorage::RequestId(5)),
                  testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());

  base::RunLoop run_loop;
  internals_handler_->SendReports({AggregationServiceStorage::RequestId(5)},
                                  run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(PrivateAggregationInternalsHandlerImplTest, ClearStorage) {
  EXPECT_CALL(*aggregation_service_, ClearData)
      .WillOnce(base::test::RunOnceCallback<3>());
  EXPECT_CALL(*private_aggregation_manager_, ClearBudgetData)
      .WillOnce(base::test::RunOnceCallback<3>());

  base::RunLoop run_loop;
  internals_handler_->ClearStorage(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(PrivateAggregationInternalsHandlerImplTest, NotifyReportsChanged) {
  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnRequestStorageModified)
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  OnRequestStorageModified();
  run_loop.Run();
}

TEST_F(PrivateAggregationInternalsHandlerImplTest, NotifyReportHandled) {
  private_aggregation_internals::mojom::WebUIAggregatableReportPtr web_report;
  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnReportHandled)
      .WillOnce(testing::DoAll(base::test::RunClosure(run_loop.QuitClosure()),
                               MoveArg<0>(&web_report)));

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  AggregationServiceStorage::RequestId id{5};

  aggregation_service::TestHpkeKey hpke_key{/*key_id=*/"id123"};
  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          request, {hpke_key.GetPublicKey()});

  base::Time now = base::Time::Now();

  OnReportHandled(request, id, report, /*actual_report_time=*/now,
                  AggregationServiceObserver::ReportStatus::kSent);
  run_loop.Run();

  ASSERT_TRUE(web_report);
  VerifyWebUIAggregatableReport(
      *web_report, request, id, report, now,
      private_aggregation_internals::mojom::ReportStatus::kSent);
}

TEST_F(PrivateAggregationInternalsHandlerImplTest, NotifyReportHandled_NoId) {
  private_aggregation_internals::mojom::WebUIAggregatableReportPtr web_report;
  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnReportHandled)
      .WillOnce(testing::DoAll(base::test::RunClosure(run_loop.QuitClosure()),
                               MoveArg<0>(&web_report)));

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  aggregation_service::TestHpkeKey hpke_key{/*key_id=*/"id123"};
  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          request, {hpke_key.GetPublicKey()});

  base::Time now = base::Time::Now();

  OnReportHandled(request, /*id=*/std::nullopt, report,
                  /*actual_report_time=*/now,
                  AggregationServiceObserver::ReportStatus::kSent);
  run_loop.Run();

  ASSERT_TRUE(web_report);
  VerifyWebUIAggregatableReport(
      *web_report, request, /*id=*/std::nullopt, report, now,
      private_aggregation_internals::mojom::ReportStatus::kSent);
}

}  // namespace content
