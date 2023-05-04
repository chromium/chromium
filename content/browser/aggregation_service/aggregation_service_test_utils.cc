// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_test_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/browser/aggregation_service/public_key_parsing_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace aggregation_service {

testing::AssertionResult PublicKeysEqual(const std::vector<PublicKey>& expected,
                                         const std::vector<PublicKey>& actual) {
  const auto tie = [](const PublicKey& key) {
    return std::make_tuple(key.id, key.key);
  };

  if (expected.size() != actual.size()) {
    return testing::AssertionFailure() << "Expected length " << expected.size()
                                       << ", actual: " << actual.size();
  }

  for (size_t i = 0; i < expected.size(); i++) {
    if (tie(expected[i]) != tie(actual[i])) {
      return testing::AssertionFailure()
             << "Expected " << expected[i] << " at index " << i
             << ", actual: " << actual[i];
    }
  }

  return testing::AssertionSuccess();
}

using AggregationServicePayload = AggregatableReport::AggregationServicePayload;

testing::AssertionResult AggregatableReportsEqual(
    const AggregatableReport& expected,
    const AggregatableReport& actual) {
  if (expected.payloads().size() != actual.payloads().size()) {
    return testing::AssertionFailure()
           << "Expected payloads size " << expected.payloads().size()
           << ", actual: " << actual.payloads().size();
  }

  for (size_t i = 0; i < expected.payloads().size(); ++i) {
    const AggregationServicePayload& expected_payload = expected.payloads()[i];
    const AggregationServicePayload& actual_payload = actual.payloads()[i];

    if (expected_payload.payload != actual_payload.payload) {
      return testing::AssertionFailure()
             << "Expected payloads at payload index " << i << " to match";
    }

    if (expected_payload.key_id != actual_payload.key_id) {
      return testing::AssertionFailure()
             << "Expected key_id " << expected_payload.key_id
             << " at payload index " << i
             << ", actual: " << actual_payload.key_id;
    }
  }

  if (expected.shared_info() != actual.shared_info()) {
    return testing::AssertionFailure()
           << "Expected shared info " << expected.shared_info()
           << ", actual: " << actual.shared_info();
  }

  if (expected.additional_fields() != actual.additional_fields()) {
    return testing::AssertionFailure() << "Expected additional fields to match";
  }

  return testing::AssertionSuccess();
}

testing::AssertionResult ReportRequestsEqual(
    const AggregatableReportRequest& expected,
    const AggregatableReportRequest& actual) {
  if (expected.processing_urls().size() != actual.processing_urls().size()) {
    return testing::AssertionFailure()
           << "Expected processing_urls size "
           << expected.processing_urls().size()
           << ", actual: " << actual.processing_urls().size();
  }
  for (size_t i = 0; i < expected.processing_urls().size(); ++i) {
    if (expected.processing_urls()[i] != actual.processing_urls()[i]) {
      return testing::AssertionFailure()
             << "Expected processing_urls()[" << i << "] to be "
             << expected.processing_urls()[i]
             << ", actual: " << actual.processing_urls()[i];
    }
  }

  testing::AssertionResult payload_contents_equal = PayloadContentsEqual(
      expected.payload_contents(), actual.payload_contents());
  if (!payload_contents_equal)
    return payload_contents_equal;

  if (expected.reporting_path() != actual.reporting_path()) {
    return testing::AssertionFailure()
           << "Expected reporting_path " << expected.reporting_path()
           << ", actual: " << actual.reporting_path();
  }

  if (expected.additional_fields() != actual.additional_fields()) {
    return testing::AssertionFailure() << "Expected additional fields to match";
  }

  return SharedInfoEqual(expected.shared_info(), actual.shared_info());
}

testing::AssertionResult PayloadContentsEqual(
    const AggregationServicePayloadContents& expected,
    const AggregationServicePayloadContents& actual) {
  if (expected.operation != actual.operation) {
    return testing::AssertionFailure()
           << "Expected operation " << expected.operation
           << ", actual: " << actual.operation;
  }
  if (expected.contributions.size() != actual.contributions.size()) {
    return testing::AssertionFailure()
           << "Expected contributions.size() " << expected.contributions.size()
           << ", actual: " << actual.contributions.size();
  }
  for (size_t i = 0; i < expected.contributions.size(); ++i) {
    if (expected.contributions[i].bucket != actual.contributions[i].bucket) {
      return testing::AssertionFailure()
             << "Expected contribution " << i << " bucket "
             << expected.contributions[i].bucket
             << ", actual: " << actual.contributions[i].bucket;
    }
    if (expected.contributions[i].value != actual.contributions[i].value) {
      return testing::AssertionFailure()
             << "Expected contribution " << i << " value "
             << expected.contributions[i].value
             << ", actual: " << actual.contributions[i].value;
    }
  }
  if (expected.aggregation_mode != actual.aggregation_mode) {
    return testing::AssertionFailure()
           << "Expected aggregation_mode " << expected.aggregation_mode
           << ", actual: " << actual.aggregation_mode;
  }

  if (expected.aggregation_coordinator != actual.aggregation_coordinator) {
    return testing::AssertionFailure()
           << "Expected aggregation_coordinator "
           << expected.aggregation_coordinator
           << ", actual: " << actual.aggregation_coordinator;
  }

  return testing::AssertionSuccess();
}

testing::AssertionResult SharedInfoEqual(
    const AggregatableReportSharedInfo& expected,
    const AggregatableReportSharedInfo& actual) {
  if (expected.scheduled_report_time != actual.scheduled_report_time) {
    return testing::AssertionFailure()
           << "Expected scheduled_report_time "
           << expected.scheduled_report_time
           << ", actual: " << actual.scheduled_report_time;
  }
  if (expected.report_id != actual.report_id) {
    return testing::AssertionFailure()
           << "Expected report_id " << expected.report_id
           << ", actual: " << actual.report_id;
  }
  if (expected.reporting_origin != actual.reporting_origin) {
    return testing::AssertionFailure()
           << "Expected reporting_origin " << expected.reporting_origin
           << ", actual: " << actual.reporting_origin;
  }
  if (expected.debug_mode != actual.debug_mode) {
    return testing::AssertionFailure()
           << "Expected debug_mode " << expected.debug_mode
           << ", actual: " << actual.debug_mode;
  }
  if (expected.additional_fields != actual.additional_fields) {
    return testing::AssertionFailure()
           << "Expected additional_fields " << expected.additional_fields
           << ", actual: " << actual.additional_fields;
  }
  if (expected.api_version != actual.api_version) {
    return testing::AssertionFailure()
           << "Expected api_version " << expected.api_version
           << ", actual: " << actual.api_version;
  }
  if (expected.api_identifier != actual.api_identifier) {
    return testing::AssertionFailure()
           << "Expected api_identifier " << expected.api_identifier
           << ", actual: " << actual.api_identifier;
  }

  return testing::AssertionSuccess();
}

AggregatableReportRequest CreateExampleRequest(
    blink::mojom::AggregationServiceMode aggregation_mode,
    int failed_send_attempts,
    ::aggregation_service::mojom::AggregationCoordinator
        aggregation_coordinator) {
  return CreateExampleRequestWithReportTime(
      /*report_time=*/base::Time::Now(), aggregation_mode, failed_send_attempts,
      aggregation_coordinator);
}

AggregatableReportRequest CreateExampleRequestWithReportTime(
    base::Time report_time,
    blink::mojom::AggregationServiceMode aggregation_mode,
    int failed_send_attempts,
    ::aggregation_service::mojom::AggregationCoordinator
        aggregation_coordinator) {
  return AggregatableReportRequest::Create(
             AggregationServicePayloadContents(
                 AggregationServicePayloadContents::Operation::kHistogram,
                 {blink::mojom::AggregatableReportHistogramContribution(
                     /*bucket=*/123,
                     /*value=*/456)},
                 aggregation_mode, aggregation_coordinator),
             AggregatableReportSharedInfo(
                 /*scheduled_report_time=*/report_time,
                 /*report_id=*/
                 base::Uuid::GenerateRandomV4(),
                 url::Origin::Create(GURL("https://reporting.example")),
                 AggregatableReportSharedInfo::DebugMode::kDisabled,
                 /*additional_fields=*/base::Value::Dict(),
                 /*api_version=*/"",
                 /*api_identifier=*/"example-api"),
             /*reporting_path=*/"example-path",
             /*debug_key=*/absl::nullopt, /*additional_fields=*/{},
             failed_send_attempts)
      .value();
}

AggregatableReportRequest CloneReportRequest(
    const AggregatableReportRequest& request) {
  return AggregatableReportRequest::CreateForTesting(
             request.processing_urls(), request.payload_contents(),
             request.shared_info().Clone(), request.reporting_path(),
             request.debug_key(), request.additional_fields(),
             request.failed_send_attempts())
      .value();
}

AggregatableReport CloneAggregatableReport(const AggregatableReport& report) {
  std::vector<AggregationServicePayload> payloads;
  for (const AggregationServicePayload& payload : report.payloads()) {
    payloads.emplace_back(payload.payload, payload.key_id,
                          payload.debug_cleartext_payload);
  }

  return AggregatableReport(std::move(payloads), report.shared_info(),
                            report.debug_key(), report.additional_fields(),
                            report.aggregation_coordinator());
}

TestHpkeKey GenerateKey(std::string key_id) {
  bssl::ScopedEVP_HPKE_KEY key;
  EXPECT_TRUE(EVP_HPKE_KEY_generate(key.get(), EVP_hpke_x25519_hkdf_sha256()));

  std::vector<uint8_t> public_key(X25519_PUBLIC_VALUE_LEN);
  size_t public_key_len;
  EXPECT_TRUE(EVP_HPKE_KEY_public_key(
      /*key=*/key.get(), /*out=*/public_key.data(),
      /*out_len=*/&public_key_len, /*max_out=*/public_key.size()));
  EXPECT_EQ(public_key.size(), public_key_len);

  TestHpkeKey hpke_key{
      {}, PublicKey(key_id, public_key), base::Base64Encode(public_key)};
  EVP_HPKE_KEY_copy(&hpke_key.full_hpke_key, key.get());

  return hpke_key;
}

base::expected<PublicKeyset, std::string> ReadAndParsePublicKeys(
    const base::FilePath& file,
    base::Time now) {
  if (!base::PathExists(file)) {
    return base::unexpected("Failed to open file: " + file.MaybeAsASCII());
  }

  std::string contents;
  if (!base::ReadFileToString(file, &contents)) {
    return base::unexpected("Failed to read file: " + file.MaybeAsASCII());
  }

  auto value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(contents);
  if (!value_with_error.has_value()) {
    return base::unexpected(
        base::StrCat({"Failed to parse \"", contents,
                      "\" as JSON: ", value_with_error.error().message}));
  }

  std::vector<PublicKey> keys = GetPublicKeys(*value_with_error);
  if (keys.empty()) {
    return base::unexpected(
        base::StrCat({"Failed to parse public keys from \"", contents, "\""}));
  }

  return PublicKeyset(std::move(keys), /*fetch_time=*/now,
                      /*expiry_time=*/base::Time::Max());
}

std::vector<uint8_t> DecryptPayloadWithHpke(
    base::span<const uint8_t> payload,
    const EVP_HPKE_KEY& key,
    const std::string& expected_serialized_shared_info) {
  base::span<const uint8_t> enc = payload.subspan(0, X25519_PUBLIC_VALUE_LEN);

  std::string authenticated_info_str =
      base::StrCat({AggregatableReport::kDomainSeparationPrefix,
                    expected_serialized_shared_info});
  base::span<const uint8_t> authenticated_info =
      base::as_bytes(base::make_span(authenticated_info_str));

  // No null terminators should have been copied when concatenating the strings.
  DCHECK(!base::Contains(authenticated_info_str, '\0'));

  bssl::ScopedEVP_HPKE_CTX recipient_context;
  if (!EVP_HPKE_CTX_setup_recipient(
          /*ctx=*/recipient_context.get(), /*key=*/&key,
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/EVP_hpke_chacha20_poly1305(),
          /*enc=*/enc.data(), /*enc_len=*/enc.size(),
          /*info=*/authenticated_info.data(),
          /*info_len=*/authenticated_info.size())) {
    return {};
  }

  base::span<const uint8_t> ciphertext =
      payload.subspan(X25519_PUBLIC_VALUE_LEN);
  std::vector<uint8_t> plaintext(ciphertext.size());
  size_t plaintext_len;

  if (!EVP_HPKE_CTX_open(
          /*ctx=*/recipient_context.get(), /*out=*/plaintext.data(),
          /*out_len*/ &plaintext_len, /*max_out_len=*/plaintext.size(),
          /*in=*/ciphertext.data(), /*in_len=*/ciphertext.size(),
          /*ad=*/nullptr,
          /*ad_len=*/0)) {
    return {};
  }

  plaintext.resize(plaintext_len);
  return plaintext;
}

}  // namespace aggregation_service

TestAggregationServiceStorageContext::TestAggregationServiceStorageContext(
    const base::Clock* clock)
    : storage_(base::SequenceBound<AggregationServiceStorageSql>(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
          /*run_in_memory=*/true,
          /*path_to_database=*/base::FilePath(),
          clock)) {}

TestAggregationServiceStorageContext::~TestAggregationServiceStorageContext() =
    default;

const base::SequenceBound<content::AggregationServiceStorage>&
TestAggregationServiceStorageContext::GetStorage() {
  return storage_;
}

MockAggregationService::MockAggregationService() = default;

MockAggregationService::~MockAggregationService() = default;

void MockAggregationService::AddObserver(AggregationServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void MockAggregationService::RemoveObserver(
    AggregationServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MockAggregationService::NotifyRequestStorageModified() {
  for (auto& observer : observers_) {
    observer.OnRequestStorageModified();
  }
}

void MockAggregationService::NotifyReportHandled(
    const AggregatableReportRequest& request,
    absl::optional<AggregationServiceStorage::RequestId> id,
    absl::optional<AggregatableReport> report,
    base::Time report_handled_time,
    AggregationServiceObserver::ReportStatus status) {
  for (auto& observer : observers_)
    observer.OnReportHandled(request, id, report, report_handled_time, status);
}

AggregatableReportRequestsAndIdsBuilder::
    AggregatableReportRequestsAndIdsBuilder() = default;

AggregatableReportRequestsAndIdsBuilder::
    ~AggregatableReportRequestsAndIdsBuilder() = default;

AggregatableReportRequestsAndIdsBuilder&&
AggregatableReportRequestsAndIdsBuilder::AddRequestWithID(
    AggregatableReportRequest request,
    AggregationServiceStorage::RequestId id) && {
  requests_.push_back(AggregationServiceStorage::RequestAndId({
      .request = std::move(request),
      .id = id,
  }));
  return std::move(*this);
}

std::vector<AggregationServiceStorage::RequestAndId>
AggregatableReportRequestsAndIdsBuilder::Build() && {
  return std::move(requests_);
}

std::ostream& operator<<(
    std::ostream& out,
    AggregationServicePayloadContents::Operation operation) {
  switch (operation) {
    case AggregationServicePayloadContents::Operation::kHistogram:
      return out << "kHistogram";
  }
}

std::ostream& operator<<(
    std::ostream& out,
    blink::mojom::AggregationServiceMode aggregation_mode) {
  switch (aggregation_mode) {
    case blink::mojom::AggregationServiceMode::kTeeBased:
      return out << "kTeeBased";
    case blink::mojom::AggregationServiceMode::kExperimentalPoplar:
      return out << "kExperimentalPoplar";
  }
}

std::ostream& operator<<(std::ostream& out,
                         AggregatableReportSharedInfo::DebugMode debug_mode) {
  switch (debug_mode) {
    case AggregatableReportSharedInfo::DebugMode::kDisabled:
      return out << "kDisabled";
    case AggregatableReportSharedInfo::DebugMode::kEnabled:
      return out << "kEnabled";
  }
}

bool operator==(const PublicKey& a, const PublicKey& b) {
  const auto tie = [](const PublicKey& public_key) {
    return std::make_tuple(public_key.id, public_key.key);
  };
  return tie(a) == tie(b);
}

bool operator==(const AggregatableReport& a, const AggregatableReport& b) {
  return aggregation_service::AggregatableReportsEqual(a, b);
}

}  // namespace content
