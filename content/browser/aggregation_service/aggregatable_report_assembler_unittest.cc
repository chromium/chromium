// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_assembler.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

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

auto MoveRequestAndReturnReport(absl::optional<AggregatableReportRequest>* out,
                                AggregatableReport report) {
  return [out, report = std::move(report)](
             AggregatableReportRequest report_request,
             std::vector<PublicKey> public_keys) {
    *out = std::move(report_request);
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
              (const url::Origin& origin, FetchCallback callback),
              (override));
};

class MockAggregatableReportProvider : public AggregatableReport::Provider {
 public:
  MOCK_METHOD(absl::optional<AggregatableReport>,
              CreateFromRequestAndPublicKeys,
              (AggregatableReportRequest, std::vector<PublicKey>),
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

  void ResetAssembler() { assembler_.reset(); }

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

TEST_F(AggregatableReportAssemblerTest, BothKeyFetchesFail_ErrorReturned) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  std::vector<url::Origin> processing_origins = request.processing_origins();

  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[0], _))
      .WillOnce(base::test::RunOnceCallback<1>(
          absl::nullopt, PublicKeyFetchStatus::kPublicKeyFetchFailed));
  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[1], _))
      .WillOnce(base::test::RunOnceCallback<1>(
          absl::nullopt, PublicKeyFetchStatus::kPublicKeyFetchFailed));
  EXPECT_CALL(callback(),
              Run(Eq(absl::nullopt), AssemblyStatus::kPublicKeyFetchFailed));

  EXPECT_CALL(*report_provider(), CreateFromRequestAndPublicKeys(_, _))
      .Times(0);

  assembler()->AssembleReport(std::move(request), callback().Get());
}

TEST_F(AggregatableReportAssemblerTest, FirstKeyFetchFails_ErrorReturned) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  std::vector<url::Origin> processing_origins = request.processing_origins();

  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[0], _))
      .WillOnce(base::test::RunOnceCallback<1>(
          absl::nullopt, PublicKeyFetchStatus::kPublicKeyFetchFailed));
  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[1], _))
      .WillOnce(base::test::RunOnceCallback<1>(
          aggregation_service::GenerateKey().public_key,
          PublicKeyFetchStatus::kOk));
  EXPECT_CALL(callback(),
              Run(Eq(absl::nullopt), AssemblyStatus::kPublicKeyFetchFailed));

  EXPECT_CALL(*report_provider(), CreateFromRequestAndPublicKeys(_, _))
      .Times(0);

  assembler()->AssembleReport(std::move(request), callback().Get());
}

TEST_F(AggregatableReportAssemblerTest, SecondKeyFetchFails_ErrorReturned) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  std::vector<url::Origin> processing_origins = request.processing_origins();

  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[0], _))
      .WillOnce(base::test::RunOnceCallback<1>(
          aggregation_service::GenerateKey().public_key,
          PublicKeyFetchStatus::kOk));
  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[1], _))
      .WillOnce(base::test::RunOnceCallback<1>(
          absl::nullopt, PublicKeyFetchStatus::kPublicKeyFetchFailed));
  EXPECT_CALL(callback(),
              Run(Eq(absl::nullopt), AssemblyStatus::kPublicKeyFetchFailed));

  EXPECT_CALL(*report_provider(), CreateFromRequestAndPublicKeys(_, _))
      .Times(0);

  assembler()->AssembleReport(std::move(request), callback().Get());
}

TEST_F(AggregatableReportAssemblerTest,
       BothKeyFetchesSucceed_ValidReportReturned) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  std::vector<url::Origin> processing_origins = request.processing_origins();
  std::vector<PublicKey> public_keys = {
      aggregation_service::GenerateKey("id123").public_key,
      aggregation_service::GenerateKey("456abc").public_key};

  absl::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          aggregation_service::CloneReportRequest(request), public_keys);
  ASSERT_TRUE(report.has_value());

  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[0], _))
      .WillOnce(base::test::RunOnceCallback<1>(public_keys[0],
                                               PublicKeyFetchStatus::kOk));
  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[1], _))
      .WillOnce(base::test::RunOnceCallback<1>(public_keys[1],
                                               PublicKeyFetchStatus::kOk));
  EXPECT_CALL(callback(), Run(report, AssemblyStatus::kOk));

  absl::optional<AggregatableReportRequest> actual_request;
  EXPECT_CALL(*report_provider(),
              CreateFromRequestAndPublicKeys(_, public_keys))
      .WillOnce(MoveRequestAndReturnReport(&actual_request,
                                           std::move(report.value())));

  assembler()->AssembleReport(aggregation_service::CloneReportRequest(request),
                              callback().Get());
  ASSERT_TRUE(actual_request.has_value());
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(actual_request.value(),
                                                       request));
}

TEST_F(AggregatableReportAssemblerTest,
       SingleServerKeyFetchSucceeds_ValidReportReturned) {
  AggregatableReportRequest request = aggregation_service::CreateExampleRequest(
      AggregationServicePayloadContents::ProcessingType::kSingleServer);

  PublicKey public_key = aggregation_service::GenerateKey("id123").public_key;

  absl::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          aggregation_service::CloneReportRequest(request), {public_key});
  ASSERT_TRUE(report.has_value());

  EXPECT_CALL(*fetcher(), GetPublicKey)
      .WillOnce(base::test::RunOnceCallback<1>(public_key,
                                               PublicKeyFetchStatus::kOk));
  EXPECT_CALL(callback(), Run(report, AssemblyStatus::kOk));

  absl::optional<AggregatableReportRequest> actual_request;
  EXPECT_CALL(*report_provider(), CreateFromRequestAndPublicKeys(
                                      _, std::vector<PublicKey>{public_key}))
      .WillOnce(MoveRequestAndReturnReport(&actual_request,
                                           std::move(report.value())));

  assembler()->AssembleReport(aggregation_service::CloneReportRequest(request),
                              callback().Get());
  ASSERT_TRUE(actual_request.has_value());
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(actual_request.value(),
                                                       request));
}

TEST_F(AggregatableReportAssemblerTest,
       SingleServerKeyFetchFails_ErrorReturned) {
  AggregatableReportRequest request = aggregation_service::CreateExampleRequest(
      AggregationServicePayloadContents::ProcessingType::kSingleServer);

  EXPECT_CALL(*fetcher(), GetPublicKey)
      .WillOnce(base::test::RunOnceCallback<1>(
          absl::nullopt, PublicKeyFetchStatus::kPublicKeyFetchFailed));
  EXPECT_CALL(callback(),
              Run(Eq(absl::nullopt), AssemblyStatus::kPublicKeyFetchFailed));

  EXPECT_CALL(*report_provider(), CreateFromRequestAndPublicKeys(_, _))
      .Times(0);

  assembler()->AssembleReport(std::move(request), callback().Get());
}

TEST_F(AggregatableReportAssemblerTest,
       KeyFetchesReturnInSwappedOrder_ValidReportReturned) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  std::vector<url::Origin> processing_origins = request.processing_origins();
  std::vector<PublicKey> public_keys = {
      aggregation_service::GenerateKey("id123").public_key,
      aggregation_service::GenerateKey("456abc").public_key};

  absl::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          aggregation_service::CloneReportRequest(request), public_keys);
  ASSERT_TRUE(report.has_value());

  std::vector<FetchCallback> pending_callbacks(2);
  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[0], _))
      .WillOnce(MoveArg<1>(&pending_callbacks.front()));
  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[1], _))
      .WillOnce(MoveArg<1>(&pending_callbacks.back()));
  EXPECT_CALL(callback(), Run(report, AssemblyStatus::kOk));

  absl::optional<AggregatableReportRequest> actual_request;
  EXPECT_CALL(*report_provider(),
              CreateFromRequestAndPublicKeys(_, public_keys))
      .WillOnce(MoveRequestAndReturnReport(&actual_request,
                                           std::move(report.value())));

  assembler()->AssembleReport(aggregation_service::CloneReportRequest(request),
                              callback().Get());

  // Swap order of responses
  std::move(pending_callbacks.back())
      .Run(public_keys[1], PublicKeyFetchStatus::kOk);
  std::move(pending_callbacks.front())
      .Run(public_keys[0], PublicKeyFetchStatus::kOk);
}

TEST_F(AggregatableReportAssemblerTest,
       AssemblerDeleted_PendingRequestsNotRun) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  std::vector<url::Origin> processing_origins = request.processing_origins();

  EXPECT_CALL(callback(), Run).Times(0);
  assembler()->AssembleReport(std::move(request), callback().Get());

  ResetAssembler();
}

TEST_F(AggregatableReportAssemblerTest,
       MultipleSimultaneousRequests_BothSucceed) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  std::vector<url::Origin> processing_origins = request.processing_origins();
  std::vector<PublicKey> public_keys = {
      aggregation_service::GenerateKey("id123").public_key,
      aggregation_service::GenerateKey("456abc").public_key};

  absl::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          aggregation_service::CloneReportRequest(request), public_keys);
  ASSERT_TRUE(report.has_value());

  std::vector<FetchCallback> pending_callbacks_1(2);
  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[0], _))
      .WillOnce(MoveArg<1>(&pending_callbacks_1.front()))
      .WillOnce(MoveArg<1>(&pending_callbacks_1.back()));

  std::vector<FetchCallback> pending_callbacks_2(2);
  EXPECT_CALL(*fetcher(), GetPublicKey(processing_origins[1], _))
      .WillOnce(MoveArg<1>(&pending_callbacks_2.front()))
      .WillOnce(MoveArg<1>(&pending_callbacks_2.back()));

  EXPECT_CALL(callback(), Run(report, AssemblyStatus::kOk)).Times(2);

  absl::optional<AggregatableReportRequest> first_request;
  absl::optional<AggregatableReportRequest> second_request;
  EXPECT_CALL(*report_provider(),
              CreateFromRequestAndPublicKeys(_, public_keys))
      .WillOnce(MoveRequestAndReturnReport(&first_request, report.value()))
      .WillOnce(MoveRequestAndReturnReport(&second_request,
                                           std::move(report.value())));

  assembler()->AssembleReport(aggregation_service::CloneReportRequest(request),
                              callback().Get());
  assembler()->AssembleReport(aggregation_service::CloneReportRequest(request),
                              callback().Get());

  std::move(pending_callbacks_1.front())
      .Run(public_keys[0], PublicKeyFetchStatus::kOk);
  std::move(pending_callbacks_1.back())
      .Run(public_keys[0], PublicKeyFetchStatus::kOk);

  std::move(pending_callbacks_2.front())
      .Run(public_keys[1], PublicKeyFetchStatus::kOk);
  std::move(pending_callbacks_2.back())
      .Run(public_keys[1], PublicKeyFetchStatus::kOk);

  ASSERT_TRUE(first_request.has_value());
  EXPECT_TRUE(
      aggregation_service::ReportRequestsEqual(first_request.value(), request));
  ASSERT_TRUE(second_request.has_value());
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(second_request.value(),
                                                       request));
}

TEST_F(AggregatableReportAssemblerTest,
       TooManySimultaneousRequests_ErrorCausedForNewRequests) {
  std::vector<PublicKey> public_keys = {
      aggregation_service::GenerateKey("id123").public_key,
      aggregation_service::GenerateKey("456abc").public_key};
  absl::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          aggregation_service::CreateExampleRequest(), std::move(public_keys));
  ASSERT_TRUE(report.has_value());

  std::vector<FetchCallback> pending_callbacks;

  Checkpoint checkpoint;
  int current_call = 1;

  {
    testing::InSequence seq;
    int current_check = 1;

    EXPECT_CALL(*fetcher(), GetPublicKey)
        .Times(2 * AggregatableReportAssembler::kMaxSimultaneousRequests)
        .WillRepeatedly(
            Invoke([&](const url::Origin& origin, FetchCallback callback) {
              pending_callbacks.push_back(std::move(callback));
            }));

    EXPECT_CALL(callback(), Run(_, _)).Times(0);

    EXPECT_CALL(checkpoint, Call(current_check++));

    EXPECT_CALL(callback(), Run(Eq(absl::nullopt),
                                AssemblyStatus::kTooManySimultaneousRequests))
        .Times(1);

    EXPECT_CALL(checkpoint, Call(current_check++));

    for (size_t i = 0;
         i < AggregatableReportAssembler::kMaxSimultaneousRequests; i++) {
      EXPECT_CALL(checkpoint, Call(current_check++));
      EXPECT_CALL(*report_provider(), CreateFromRequestAndPublicKeys)
          .WillOnce(Return(report));
      EXPECT_CALL(callback(), Run(report, AssemblyStatus::kOk));
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

  for (size_t i = 0; i < pending_callbacks.size(); ++i) {
    // Every request has 2 pending fetch callbacks.
    if (i % 2 == 0)
      checkpoint.Call(current_call++);

    std::move(pending_callbacks[i])
        .Run(aggregation_service::GenerateKey("id123").public_key,
             PublicKeyFetchStatus::kOk);
  }
}

}  // namespace content
