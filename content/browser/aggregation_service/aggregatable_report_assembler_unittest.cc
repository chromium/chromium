// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_assembler.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;

// Will be used to verify the sequence of expected function calls.
using Checkpoint = ::testing::MockFunction<void(int)>;

using FetchCallback = AggregationServiceKeyFetcher::FetchCallback;
using AssemblyCallback = AggregatableReportAssembler::AssemblyCallback;
using PublicKeyFetchStatus = AggregationServiceKeyFetcher::PublicKeyFetchStatus;
using AssemblyStatus = AggregatableReportAssembler::AssemblyStatus;

constexpr std::string_view kReportAssemblerStatusHistogramName =
    "PrivacySandbox.AggregationService.ReportAssembler.Status";

auto CloneRequestAndReturnReport(std::optional<AggregatableReportRequest>* out,
                                 AggregatableReport report) {
  return [out, report = std::move(report)](
             const AggregatableReportRequest& report_request,
             PublicKey public_key) {
    *out = aggregation_service::CloneReportRequest(report_request);
    return std::move(report);
  };
}

class MockAggregationServiceKeyFetcher : public AggregationServiceKeyFetcher {
 public:
  MockAggregationServiceKeyFetcher()
      : AggregationServiceKeyFetcher(/*storage_context=*/nullptr,
                                     /*network_fetcher=*/nullptr) {}

  MOCK_METHOD(void,
              GetPublicKey,
              (const GURL& url, FetchCallback callback),
              (override));
};

class MockAggregatableReportProvider : public AggregatableReport::Provider {
 public:
  MOCK_METHOD(std::optional<AggregatableReport>,
              CreateFromRequestAndPublicKey,
              (const AggregatableReportRequest&, PublicKey),
              (const, override));
};

}  // namespace

class AggregatableReportAssemblerTest : public testing::Test {
 public:
  AggregatableReportAssemblerTest() = default;

  void SetUp() override {
    auto fetcher = std::make_unique<MockAggregationServiceKeyFetcher>();
    auto report_provider = std::make_unique<MockAggregatableReportProvider>();

    fetcher_ = fetcher.get();
    report_provider_ = report_provider.get();
    assembler_ = AggregatableReportAssembler::CreateForTesting(
        std::move(fetcher), std::move(report_provider));
  }

  void ResetAssembler() {
    fetcher_ = nullptr;
    report_provider_ = nullptr;
    assembler_.reset();
  }

  AggregatableReportAssembler* assembler() { return assembler_.get(); }
  MockAggregationServiceKeyFetcher* fetcher() { return fetcher_; }
  MockAggregatableReportProvider* report_provider() { return report_provider_; }
  base::MockCallback<AssemblyCallback>& callback() { return callback_; }

 private:
  std::unique_ptr<AggregatableReportAssembler> assembler_;

  // These objects are owned by `assembler_`.
  raw_ptr<MockAggregationServiceKeyFetcher> fetcher_;
  raw_ptr<MockAggregatableReportProvider> report_provider_;

  base::MockCallback<AssemblyCallback> callback_;
};

TEST_F(AggregatableReportAssemblerTest, KeyFetchSucceeds_ValidReportReturned) {
  base::HistogramTester histograms;

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  PublicKey public_key =
      aggregation_service::TestHpkeKey("id123").GetPublicKey();

  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKey(request,
                                                                   public_key);
  ASSERT_TRUE(report.has_value());

  EXPECT_CALL(*fetcher(), GetPublicKey)
      .WillOnce(base::test::RunOnceCallback<1>(public_key,
                                               PublicKeyFetchStatus::kOk));
  EXPECT_CALL(callback(), Run(_, report, AssemblyStatus::kOk));

  std::optional<AggregatableReportRequest> actual_request;
  EXPECT_CALL(*report_provider(), CreateFromRequestAndPublicKey(_, public_key))
      .WillOnce(CloneRequestAndReturnReport(&actual_request,
                                            std::move(report.value())));

  assembler()->AssembleReport(aggregation_service::CloneReportRequest(request),
                              callback().Get());
  ASSERT_TRUE(actual_request.has_value());
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(actual_request.value(),
                                                       request));

  histograms.ExpectUniqueSample(
      kReportAssemblerStatusHistogramName,
      AggregatableReportAssembler::AssemblyStatus::kOk, 1);
}

TEST_F(AggregatableReportAssemblerTest, KeyFetchFails_ErrorReturned) {
  base::HistogramTester histograms;

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  EXPECT_CALL(*fetcher(), GetPublicKey)
      .WillOnce(base::test::RunOnceCallback<1>(
          std::nullopt, PublicKeyFetchStatus::kPublicKeyFetchFailed));
  EXPECT_CALL(callback(),
              Run(_, Eq(std::nullopt), AssemblyStatus::kPublicKeyFetchFailed));

  EXPECT_CALL(*report_provider(), CreateFromRequestAndPublicKey(_, _)).Times(0);

  assembler()->AssembleReport(std::move(request), callback().Get());

  histograms.ExpectUniqueSample(
      kReportAssemblerStatusHistogramName,
      AggregatableReportAssembler::AssemblyStatus::kPublicKeyFetchFailed, 1);
}

TEST_F(AggregatableReportAssemblerTest,
       AssemblerDeleted_PendingRequestsNotRun) {
  base::HistogramTester histograms;

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  EXPECT_CALL(callback(), Run).Times(0);
  EXPECT_CALL(*fetcher(), GetPublicKey);
  assembler()->AssembleReport(std::move(request), callback().Get());

  ResetAssembler();

  histograms.ExpectTotalCount(kReportAssemblerStatusHistogramName, 0);
}

TEST_F(AggregatableReportAssemblerTest,
       MultipleSimultaneousRequests_BothSucceed) {
  base::HistogramTester histograms;

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  PublicKey public_key =
      aggregation_service::TestHpkeKey("id123").GetPublicKey();

  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKey(request,
                                                                   public_key);
  ASSERT_TRUE(report.has_value());

  std::vector<FetchCallback> pending_callbacks(2);
  EXPECT_CALL(*fetcher(), GetPublicKey(request.processing_url(), _))
      .WillOnce(MoveArg<1>(&pending_callbacks.front()))
      .WillOnce(MoveArg<1>(&pending_callbacks.back()));

  EXPECT_CALL(callback(), Run(_, report, AssemblyStatus::kOk)).Times(2);

  std::optional<AggregatableReportRequest> first_request;
  std::optional<AggregatableReportRequest> second_request;
  EXPECT_CALL(*report_provider(), CreateFromRequestAndPublicKey(_, public_key))
      .WillOnce(CloneRequestAndReturnReport(&first_request, report.value()))
      .WillOnce(CloneRequestAndReturnReport(&second_request,
                                            std::move(report.value())));

  assembler()->AssembleReport(aggregation_service::CloneReportRequest(request),
                              callback().Get());
  assembler()->AssembleReport(aggregation_service::CloneReportRequest(request),
                              callback().Get());

  std::move(pending_callbacks.front())
      .Run(public_key, PublicKeyFetchStatus::kOk);
  std::move(pending_callbacks.back())
      .Run(public_key, PublicKeyFetchStatus::kOk);

  ASSERT_TRUE(first_request.has_value());
  EXPECT_TRUE(
      aggregation_service::ReportRequestsEqual(first_request.value(), request));
  ASSERT_TRUE(second_request.has_value());
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(second_request.value(),
                                                       request));

  histograms.ExpectUniqueSample(
      kReportAssemblerStatusHistogramName,
      AggregatableReportAssembler::AssemblyStatus::kOk, 2);
}

TEST_F(AggregatableReportAssemblerTest,
       TooManySimultaneousRequests_ErrorCausedForNewRequests) {
  base::HistogramTester histograms;

  PublicKey public_key =
      aggregation_service::TestHpkeKey("id123").GetPublicKey();
  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKey(
          aggregation_service::CreateExampleRequest(), std::move(public_key));
  ASSERT_TRUE(report.has_value());

  std::vector<FetchCallback> pending_callbacks;

  Checkpoint checkpoint;
  int current_call = 1;

  {
    testing::InSequence seq;
    int current_check = 1;

    EXPECT_CALL(*fetcher(), GetPublicKey)
        .Times(AggregatableReportAssembler::kMaxSimultaneousRequests)
        .WillRepeatedly(Invoke([&](const GURL& url, FetchCallback callback) {
          pending_callbacks.push_back(std::move(callback));
        }));

    EXPECT_CALL(callback(), Run).Times(0);

    EXPECT_CALL(checkpoint, Call(current_check++));

    EXPECT_CALL(callback(), Run(_, Eq(std::nullopt),
                                AssemblyStatus::kTooManySimultaneousRequests))
        .Times(1);

    EXPECT_CALL(checkpoint, Call(current_check++));

    for (size_t i = 0;
         i < AggregatableReportAssembler::kMaxSimultaneousRequests; i++) {
      EXPECT_CALL(checkpoint, Call(current_check++));
      EXPECT_CALL(*report_provider(), CreateFromRequestAndPublicKey)
          .WillOnce(Return(report));
      EXPECT_CALL(callback(), Run(_, report, AssemblyStatus::kOk));
    }
  }

  for (size_t i = 0; i < AggregatableReportAssembler::kMaxSimultaneousRequests;
       ++i) {
    assembler()->AssembleReport(aggregation_service::CreateExampleRequest(),
                                callback().Get());
  }

  checkpoint.Call(current_call++);

  assembler()->AssembleReport(aggregation_service::CreateExampleRequest(),
                              callback().Get());

  checkpoint.Call(current_call++);

  for (FetchCallback& pending_callback : pending_callbacks) {
    checkpoint.Call(current_call++);

    std::move(pending_callback)
        .Run(aggregation_service::TestHpkeKey("id123").GetPublicKey(),
             PublicKeyFetchStatus::kOk);
  }

  histograms.ExpectBucketCount(
      kReportAssemblerStatusHistogramName,
      AggregatableReportAssembler::AssemblyStatus::kOk,
      AggregatableReportAssembler::kMaxSimultaneousRequests);
  histograms.ExpectBucketCount(
      kReportAssemblerStatusHistogramName,
      AggregatableReportAssembler::AssemblyStatus::kTooManySimultaneousRequests,
      1);
}

}  // namespace content
