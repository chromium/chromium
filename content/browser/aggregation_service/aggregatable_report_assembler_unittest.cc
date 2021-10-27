// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_assembler.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AggregatableReportAssemblerTest : public testing::Test {
 public:
  AggregatableReportAssemblerTest() = default;

  void SetUp() override {
    auto fetcher = std::make_unique<TestAggregationServiceKeyFetcher>();
    auto report_provider = std::make_unique<TestAggregatableReportProvider>();

    fetcher_ = fetcher.get();
    report_provider_ = report_provider.get();
    assembler_ = AggregatableReportAssembler::CreateForTesting(
        std::move(fetcher), std::move(report_provider));

    num_assembly_callbacks_run_ = 0;
  }

  void AssembleReport(AggregatableReportRequest request) {
    assembler()->AssembleReport(
        std::move(request),
        base::BindLambdaForTesting(
            [&](absl::optional<AggregatableReport> report,
                AggregatableReportAssembler::AssemblyStatus status) {
              last_assembled_report_ = std::move(report);
              last_assembled_status_ = std::move(status);

              ++num_assembly_callbacks_run_;
            }));
  }

  void ResetAssembler() { assembler_.reset(); }

  AggregatableReportAssembler* assembler() { return assembler_.get(); }
  TestAggregationServiceKeyFetcher* fetcher() { return fetcher_; }
  TestAggregatableReportProvider* report_provider() { return report_provider_; }
  int num_assembly_callbacks_run() const { return num_assembly_callbacks_run_; }

  // Should only be called after the report callback has been run.
  const absl::optional<AggregatableReport>& last_assembled_report() const {
    EXPECT_GT(num_assembly_callbacks_run_, 0);
    return last_assembled_report_;
  }
  const AggregatableReportAssembler::AssemblyStatus& last_assembled_status()
      const {
    EXPECT_GT(num_assembly_callbacks_run_, 0);
    return last_assembled_status_;
  }

 private:
  std::unique_ptr<AggregatableReportAssembler> assembler_;

  // These objects are owned by `assembler_`.
  TestAggregationServiceKeyFetcher* fetcher_;
  TestAggregatableReportProvider* report_provider_;

  int num_assembly_callbacks_run_ = 0;

  // The last arguments passed to the AssemblyCallback are saved here.
  absl::optional<AggregatableReport> last_assembled_report_;
  AggregatableReportAssembler::AssemblyStatus last_assembled_status_;
};

TEST_F(AggregatableReportAssemblerTest, BothKeyFetchesFail_ErrorReturned) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  std::vector<url::Origin> processing_origins = request.processing_origins();

  AssembleReport(std::move(request));

  fetcher()->TriggerPublicKeyResponse(
      processing_origins[0], /*key=*/absl::nullopt,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::
          kPublicKeyFetchFailed);
  fetcher()->TriggerPublicKeyResponse(
      processing_origins[1], /*key=*/absl::nullopt,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::
          kPublicKeyFetchFailed);

  EXPECT_FALSE(fetcher()->HasPendingCallbacks());
  EXPECT_EQ(num_assembly_callbacks_run(), 1);
  EXPECT_EQ(report_provider()->num_calls(), 0);

  EXPECT_FALSE(last_assembled_report().has_value());
  EXPECT_EQ(last_assembled_status(),
            AggregatableReportAssembler::AssemblyStatus::kPublicKeyFetchFailed);
}

TEST_F(AggregatableReportAssemblerTest, FirstKeyFetchFails_ErrorReturned) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  std::vector<url::Origin> processing_origins = request.processing_origins();

  AssembleReport(std::move(request));

  fetcher()->TriggerPublicKeyResponse(
      processing_origins[0], /*key=*/absl::nullopt,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::
          kPublicKeyFetchFailed);
  fetcher()->TriggerPublicKeyResponse(
      processing_origins[1], aggregation_service::GenerateKey().public_key,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);

  EXPECT_FALSE(fetcher()->HasPendingCallbacks());
  EXPECT_EQ(num_assembly_callbacks_run(), 1);
  EXPECT_EQ(report_provider()->num_calls(), 0);

  EXPECT_FALSE(last_assembled_report().has_value());
  EXPECT_EQ(last_assembled_status(),
            AggregatableReportAssembler::AssemblyStatus::kPublicKeyFetchFailed);
}

TEST_F(AggregatableReportAssemblerTest, SecondKeyFetchFails_ErrorReturned) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  std::vector<url::Origin> processing_origins = request.processing_origins();

  AssembleReport(std::move(request));

  fetcher()->TriggerPublicKeyResponse(
      processing_origins[0], aggregation_service::GenerateKey().public_key,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
  fetcher()->TriggerPublicKeyResponse(
      processing_origins[1], /*key=*/absl::nullopt,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::
          kPublicKeyFetchFailed);

  EXPECT_FALSE(fetcher()->HasPendingCallbacks());
  EXPECT_EQ(num_assembly_callbacks_run(), 1);
  EXPECT_EQ(report_provider()->num_calls(), 0);

  EXPECT_FALSE(last_assembled_report().has_value());
  EXPECT_EQ(last_assembled_status(),
            AggregatableReportAssembler::AssemblyStatus::kPublicKeyFetchFailed);
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
  report_provider()->set_report_to_return(std::move(report.value()));

  AssembleReport(aggregation_service::CloneReportRequest(request));

  fetcher()->TriggerPublicKeyResponse(
      processing_origins[0], public_keys[0],
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
  fetcher()->TriggerPublicKeyResponse(
      processing_origins[1], public_keys[1],
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);

  EXPECT_FALSE(fetcher()->HasPendingCallbacks());
  EXPECT_EQ(num_assembly_callbacks_run(), 1);

  EXPECT_EQ(report_provider()->num_calls(), 1);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      report_provider()->PreviousRequest(), request));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      report_provider()->PreviousPublicKeys(), public_keys));

  EXPECT_TRUE(last_assembled_report().has_value());
  EXPECT_EQ(last_assembled_status(),
            AggregatableReportAssembler::AssemblyStatus::kOk);
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
  report_provider()->set_report_to_return(std::move(report.value()));

  AssembleReport(aggregation_service::CloneReportRequest(request));

  // Swap order of responses
  fetcher()->TriggerPublicKeyResponse(
      processing_origins[1], public_keys[1],
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
  fetcher()->TriggerPublicKeyResponse(
      processing_origins[0], public_keys[0],
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);

  EXPECT_FALSE(fetcher()->HasPendingCallbacks());
  EXPECT_EQ(num_assembly_callbacks_run(), 1);

  EXPECT_EQ(report_provider()->num_calls(), 1);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      report_provider()->PreviousRequest(), request));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      report_provider()->PreviousPublicKeys(), public_keys));

  EXPECT_TRUE(last_assembled_report().has_value());
  EXPECT_EQ(last_assembled_status(),
            AggregatableReportAssembler::AssemblyStatus::kOk);
}

TEST_F(AggregatableReportAssemblerTest,
       BothProcessingOriginsAreIdentical_ValidReportReturned) {
  AggregatableReportRequest starter_request =
      aggregation_service::CreateExampleRequest();

  url::Origin processing_origin = starter_request.processing_origins()[0];

  // Set second processing origin to match the first and create a new request.
  absl::optional<AggregatableReportRequest> request =
      AggregatableReportRequest::Create({processing_origin, processing_origin},
                                        starter_request.payload_contents(),
                                        starter_request.shared_info());

  ASSERT_TRUE(request.has_value());

  PublicKey public_key = aggregation_service::GenerateKey("id123").public_key;

  absl::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          aggregation_service::CloneReportRequest(request.value()),
          /*public_keys=*/{public_key, public_key});
  ASSERT_TRUE(report.has_value());
  report_provider()->set_report_to_return(std::move(report.value()));

  AssembleReport(aggregation_service::CloneReportRequest(request.value()));

  fetcher()->TriggerPublicKeyResponse(
      processing_origin, public_key,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);

  EXPECT_FALSE(fetcher()->HasPendingCallbacks());
  EXPECT_EQ(num_assembly_callbacks_run(), 1);

  EXPECT_EQ(report_provider()->num_calls(), 1);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      report_provider()->PreviousRequest(), request.value()));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      report_provider()->PreviousPublicKeys(), {public_key, public_key}));

  EXPECT_TRUE(last_assembled_report().has_value());
  EXPECT_EQ(last_assembled_status(),
            AggregatableReportAssembler::AssemblyStatus::kOk);
}

TEST_F(AggregatableReportAssemblerTest,
       AssemblerDeleted_PendingRequestsNotRun) {
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  std::vector<url::Origin> processing_origins = request.processing_origins();

  AssembleReport(std::move(request));

  ResetAssembler();
  EXPECT_EQ(num_assembly_callbacks_run(), 0);
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
  report_provider()->set_report_to_return(std::move(report.value()));

  AssembleReport(aggregation_service::CloneReportRequest(request));
  AssembleReport(aggregation_service::CloneReportRequest(request));

  EXPECT_EQ(num_assembly_callbacks_run(), 0);

  fetcher()->TriggerPublicKeyResponse(
      processing_origins[0], public_keys[0],
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
  fetcher()->TriggerPublicKeyResponse(
      processing_origins[1], public_keys[1],
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);

  EXPECT_FALSE(fetcher()->HasPendingCallbacks());
  EXPECT_EQ(num_assembly_callbacks_run(), 2);

  EXPECT_EQ(report_provider()->num_calls(), 2);

  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      report_provider()->PreviousRequest(), request));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      report_provider()->PreviousPublicKeys(), public_keys));

  EXPECT_TRUE(last_assembled_report().has_value());
  EXPECT_EQ(last_assembled_status(),
            AggregatableReportAssembler::AssemblyStatus::kOk);
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
  report_provider()->set_report_to_return(std::move(report.value()));

  for (size_t i = 0; i < AggregatableReportAssembler::kMaxSimultaneousRequests;
       ++i) {
    AssembleReport(aggregation_service::CreateExampleRequest());
  }

  // All requests are still pending.
  EXPECT_EQ(num_assembly_callbacks_run(), 0);

  // Adding one request too many causes that new request to fail.
  AssembleReport(aggregation_service::CreateExampleRequest());
  EXPECT_EQ(num_assembly_callbacks_run(), 1);

  EXPECT_FALSE(last_assembled_report().has_value());
  EXPECT_EQ(last_assembled_status(),
            AggregatableReportAssembler::AssemblyStatus::
                kTooManySimultaneousRequests);

  // But all other requests should remain pending.
  fetcher()->TriggerPublicKeyResponseForAllOrigins(
      aggregation_service::GenerateKey("id123").public_key,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);

  EXPECT_TRUE(num_assembly_callbacks_run() ==
              AggregatableReportAssembler::kMaxSimultaneousRequests + 1);
}

}  // namespace content
