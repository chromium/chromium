// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "base/containers/contains.h"
#include "base/guid.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  if (expected.privacy_budget_key != actual.privacy_budget_key) {
    return testing::AssertionFailure()
           << "Expected privacy_budget_key " << expected.privacy_budget_key
           << ", actual: " << actual.privacy_budget_key;
  }
  if (expected.report_id != actual.report_id) {
    return testing::AssertionFailure()
           << "Expected report_id " << expected.report_id
           << ", actual: " << actual.report_id;
  }
  if (expected.debug_mode != actual.debug_mode) {
    return testing::AssertionFailure()
           << "Expected debug_mode " << expected.debug_mode
           << ", actual: " << actual.debug_mode;
  }

  return testing::AssertionSuccess();
}

AggregatableReportRequest CreateExampleRequest(
    AggregationServicePayloadContents::AggregationMode aggregation_mode) {
  return AggregatableReportRequest::Create(
             AggregationServicePayloadContents(
                 AggregationServicePayloadContents::Operation::kHistogram,
                 {AggregationServicePayloadContents::HistogramContribution{
                     .bucket = 123, .value = 456}},
                 aggregation_mode),
             AggregatableReportSharedInfo(
                 /*scheduled_report_time=*/base::Time::Now(),
                 /*privacy_budget_key=*/"example_budget_key",
                 /*report_id=*/
                 base::GUID::GenerateRandomV4(),
                 url::Origin::Create(GURL("https://reporting.example")),
                 AggregatableReportSharedInfo::DebugMode::kDisabled))
      .value();
}

AggregatableReportRequest CloneReportRequest(
    const AggregatableReportRequest& request) {
  return AggregatableReportRequest::CreateForTesting(request.processing_urls(),
                                                     request.payload_contents(),
                                                     request.shared_info())
      .value();
}

AggregatableReport CloneAggregatableReport(const AggregatableReport& report) {
  std::vector<AggregationServicePayload> payloads;
  for (const AggregationServicePayload& payload : report.payloads()) {
    payloads.emplace_back(payload.payload, payload.key_id,
                          payload.debug_cleartext_payload);
  }

  return AggregatableReport(std::move(payloads), report.shared_info());
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

const base::SequenceBound<content::AggregationServiceKeyStorage>&
TestAggregationServiceStorageContext::GetKeyStorage() {
  return storage_;
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
    AggregationServicePayloadContents::AggregationMode aggregation_mode) {
  switch (aggregation_mode) {
    case AggregationServicePayloadContents::AggregationMode::kTeeBased:
      return out << "kTeeBased";
    case AggregationServicePayloadContents::AggregationMode::
        kExperimentalPoplar:
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
