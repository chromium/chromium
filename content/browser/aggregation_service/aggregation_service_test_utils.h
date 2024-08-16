// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TEST_UTILS_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TEST_UTILS_H_

#include <stdint.h>

#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_observer.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_storage_context.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace base {
class Clock;
class FilePath;
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

class AggregationServiceStorage;

namespace aggregation_service {

class TestHpkeKey {
 public:
  // Generates a new HPKE key. Note that `key_id` is just a label.
  explicit TestHpkeKey(std::string key_id = "example_id");
  ~TestHpkeKey();

  // This class is move-only.
  TestHpkeKey(TestHpkeKey&&);
  TestHpkeKey& operator=(TestHpkeKey&&);
  TestHpkeKey(TestHpkeKey&) = delete;
  TestHpkeKey& operator=(TestHpkeKey&) = delete;

  std::string_view key_id() const { return key_id_; }
  const EVP_HPKE_KEY& full_hpke_key() const { return *full_hpke_key_.get(); }

  // Returns the HPKE key's corresponding public key.
  PublicKey GetPublicKey() const;
  // Returns the HPKE key's corresponding public key encoded in base64.
  std::string GetPublicKeyBase64() const;

 private:
  std::string key_id_;
  bssl::ScopedEVP_HPKE_KEY full_hpke_key_;
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

// Returns an example report request, using the given parameters.
AggregatableReportRequest CreateExampleRequest(
    blink::mojom::AggregationServiceMode aggregation_mode =
        blink::mojom::AggregationServiceMode::kDefault,
    int failed_send_attempts = 0,
    std::optional<url::Origin> aggregation_coordinator_origin = std::nullopt,
    std::optional<AggregatableReportRequest::DelayType> = std::nullopt);

AggregatableReportRequest CreateExampleRequestWithReportTime(
    base::Time report_time,
    blink::mojom::AggregationServiceMode aggregation_mode =
        blink::mojom::AggregationServiceMode::kDefault,
    int failed_send_attempts = 0,
    std::optional<url::Origin> aggregation_coordinator_origin = std::nullopt,
    std::optional<AggregatableReportRequest::DelayType> = std::nullopt);

AggregatableReportRequest CloneReportRequest(
    const AggregatableReportRequest& request);
AggregatableReport CloneAggregatableReport(const AggregatableReport& report);

base::expected<PublicKeyset, std::string> ReadAndParsePublicKeys(
    const base::FilePath& file,
    base::Time now);

// Returns empty vector in the case of an error.
std::vector<uint8_t> DecryptPayloadWithHpke(
    base::span<const uint8_t> payload,
    const EVP_HPKE_KEY& key,
    std::string_view expected_serialized_shared_info);

MATCHER_P(RequestIdIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.id, result_listener);
}

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
  const base::SequenceBound<content::AggregationServiceStorage>& GetStorage()
      override;

 private:
  base::SequenceBound<content::AggregationServiceStorage> storage_;
};

class MockAggregationService : public AggregationService {
 public:
  MockAggregationService();
  ~MockAggregationService() override;

  // AggregationService:
  MOCK_METHOD(void,
              AssembleReport,
              (AggregatableReportRequest request,
               AggregationService::AssemblyCallback callback),
              (override));

  MOCK_METHOD(void,
              SendReport,
              (const GURL& url,
               const AggregatableReport& report,
               std::optional<AggregatableReportRequest::DelayType> delay_type,
               AggregationService::SendCallback callback),
              (override));

  MOCK_METHOD(void,
              SendReport,
              (const GURL& url,
               const base::Value& value,
               std::optional<AggregatableReportRequest::DelayType> delay_type,
               AggregationService::SendCallback callback),
              (override));

  MOCK_METHOD(void,
              ClearData,
              (base::Time delete_begin,
               base::Time delete_end,
               StoragePartition::StorageKeyMatcherFunction filter,
               base::OnceClosure done),
              (override));

  MOCK_METHOD(void,
              ScheduleReport,
              (AggregatableReportRequest report_request),
              (override));

  MOCK_METHOD(void,
              AssembleAndSendReport,
              (AggregatableReportRequest report_request),
              (override));

  MOCK_METHOD(
      void,
      GetPendingReportRequestsForWebUI,
      (base::OnceCallback<
          void(std::vector<AggregationServiceStorage::RequestAndId>)> callback),
      (override));

  MOCK_METHOD(void,
              SendReportsForWebUI,
              (const std::vector<AggregationServiceStorage::RequestId>& ids,
               base::OnceClosure reports_sent_callback),
              (override));

  MOCK_METHOD(void,
              GetPendingReportReportingOrigins,
              (base::OnceCallback<void(std::set<url::Origin>)> callback),
              (override));

  void AddObserver(AggregationServiceObserver* observer) override;

  void RemoveObserver(AggregationServiceObserver* observer) override;

  void NotifyRequestStorageModified();

  // `report_handled_time` indicates when the report has been handled.
  void NotifyReportHandled(
      const AggregatableReportRequest& request,
      std::optional<AggregationServiceStorage::RequestId> id,
      std::optional<AggregatableReport> report,
      base::Time report_handled_time,
      AggregationServiceObserver::ReportStatus status);

 private:
  base::ObserverList<AggregationServiceObserver, /*check_empty=*/true>
      observers_;
};

class AggregatableReportRequestsAndIdsBuilder {
 public:
  AggregatableReportRequestsAndIdsBuilder();
  ~AggregatableReportRequestsAndIdsBuilder();

  AggregatableReportRequestsAndIdsBuilder&& AddRequestWithID(
      AggregatableReportRequest request,
      AggregationServiceStorage::RequestId id) &&;

  std::vector<AggregationServiceStorage::RequestAndId> Build() &&;

 private:
  std::vector<AggregationServiceStorage::RequestAndId> requests_;
};

// Only used for logging in tests.
std::ostream& operator<<(
    std::ostream& out,
    AggregationServicePayloadContents::Operation operation);
std::ostream& operator<<(std::ostream& out,
                         blink::mojom::AggregationServiceMode aggregation_mode);
std::ostream& operator<<(std::ostream& out,
                         AggregatableReportSharedInfo::DebugMode debug_mode);

bool operator==(const PublicKey& a, const PublicKey& b);

bool operator==(const AggregatableReport& a, const AggregatableReport& b);

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TEST_UTILS_H_
