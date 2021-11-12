// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TEST_UTILS_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TEST_UTILS_H_

#include <stdint.h>

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "base/threading/sequence_bound.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"
#include "content/browser/aggregation_service/aggregation_service_key_storage.h"
#include "content/browser/aggregation_service/aggregation_service_storage_context.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "url/origin.h"

namespace base {
class Clock;
}  // namespace base

namespace content {

namespace aggregation_service {

struct TestHpkeKey {
  // Public-private key pair.
  EVP_HPKE_KEY full_hpke_key;

  // Contains a copy of the public key of `full_hpke_key`.
  PublicKey public_key;

  // Contains a base64-encoded copy of `public_key.key`
  std::string base64_encoded_public_key;
};

testing::AssertionResult PublicKeysEqual(const std::vector<PublicKey>& expected,
                                         const std::vector<PublicKey>& actual);
testing::AssertionResult AggregatableReportsEqual(
    const AggregatableReport& expected,
    const AggregatableReport& actual);
testing::AssertionResult ReportRequestsEqual(
    const AggregatableReportRequest& expected,
    const AggregatableReportRequest& actual);
testing::AssertionResult PayloadContentsEqual(
    const AggregationServicePayloadContents& expected,
    const AggregationServicePayloadContents& actual);
testing::AssertionResult SharedInfoEqual(
    const AggregatableReportSharedInfo& expected,
    const AggregatableReportSharedInfo& actual);

std::vector<url::Origin> GetExampleProcessingOrigins();

// Returns an example report request, using the given parameters. If the first
// signature is used, example processing origins will be used.
AggregatableReportRequest CreateExampleRequest(
    AggregationServicePayloadContents::ProcessingType processing_type =
        AggregationServicePayloadContents::ProcessingType::kTwoParty);
AggregatableReportRequest CreateExampleRequest(
    AggregationServicePayloadContents::ProcessingType processing_type,
    std::vector<url::Origin> processing_origins);

AggregatableReportRequest CloneReportRequest(
    const AggregatableReportRequest& request);
AggregatableReport CloneAggregatableReport(const AggregatableReport& report);

// Generates a public-private key pair for HPKE and also constructs a PublicKey
// object for use in assembler methods.
TestHpkeKey GenerateKey(std::string key_id = "example_id");

}  // namespace aggregation_service

// The strings "ABCD1234" and "EFGH5678", Base64-decoded to bytes. Note that
// both of these strings are valid Base64.
const std::vector<uint8_t> kABCD1234AsBytes = {0, 16, 131, 215, 109, 248};
const std::vector<uint8_t> kEFGH5678AsBytes = {16, 81, 135, 231, 174, 252};

class TestAggregationServiceStorageContext
    : public AggregationServiceStorageContext {
 public:
  explicit TestAggregationServiceStorageContext(const base::Clock* clock);
  TestAggregationServiceStorageContext(
      const TestAggregationServiceStorageContext& other) = delete;
  TestAggregationServiceStorageContext& operator=(
      const TestAggregationServiceStorageContext& other) = delete;
  ~TestAggregationServiceStorageContext() override;

  // AggregationServiceStorageContext:
  const base::SequenceBound<content::AggregationServiceKeyStorage>&
  GetKeyStorage() override;

 private:
  base::SequenceBound<content::AggregationServiceKeyStorage> storage_;
};

class TestAggregationServiceKeyFetcher : public AggregationServiceKeyFetcher {
 public:
  TestAggregationServiceKeyFetcher();
  ~TestAggregationServiceKeyFetcher() override;

  // AggregationServiceKeyFetcher:
  void GetPublicKey(const url::Origin& origin, FetchCallback callback) override;

  // Triggers a response for each fetch for `origin`, throwing an error if no
  // such fetches exist.
  void TriggerPublicKeyResponse(const url::Origin& origin,
                                absl::optional<PublicKey> key,
                                PublicKeyFetchStatus status);

  void TriggerPublicKeyResponseForAllOrigins(absl::optional<PublicKey> key,
                                             PublicKeyFetchStatus status);

  bool HasPendingCallbacks();

 private:
  std::map<url::Origin, std::vector<FetchCallback>> callbacks_;
};

// A simple class for mocking CreateFromRequestAndPublicKeys().
class TestAggregatableReportProvider : public AggregatableReport::Provider {
 public:
  TestAggregatableReportProvider();
  ~TestAggregatableReportProvider() override;

  absl::optional<AggregatableReport> CreateFromRequestAndPublicKeys(
      AggregatableReportRequest report_request,
      std::vector<PublicKey> public_keys) const override;

  int num_calls() const { return num_calls_; }

  const AggregatableReportRequest& PreviousRequest() const {
    EXPECT_TRUE(previous_request_.has_value());
    return previous_request_.value();
  }
  const std::vector<PublicKey>& PreviousPublicKeys() const {
    EXPECT_TRUE(previous_request_.has_value());
    return previous_public_keys_;
  }

  void set_report_to_return(
      absl::optional<AggregatableReport> report_to_return) {
    report_to_return_ = std::move(report_to_return);
  }

 private:
  absl::optional<AggregatableReport> report_to_return_;

  // The following are mutable to allow `CreateFromRequestAndPublicKeys()` to be
  // const.

  // Number of times `CreateFromRequestAndPublicKeys()` is called.
  mutable int num_calls_ = 0;

  // `absl::nullopt` iff `num_calls_` is 0.
  mutable absl::optional<AggregatableReportRequest> previous_request_;

  // Empty if `num_calls_` is 0.
  mutable std::vector<PublicKey> previous_public_keys_;
};

// Only used for logging in tests.
std::ostream& operator<<(
    std::ostream& out,
    const AggregationServicePayloadContents::Operation& operation);
std::ostream& operator<<(
    std::ostream& out,
    const AggregationServicePayloadContents::ProcessingType& processing_type);

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TEST_UTILS_H_
