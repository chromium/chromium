// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_test_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <tuple>
#include <vector>

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

  return testing::AssertionSuccess();
}

AggregatableReportRequest CreateExampleRequest() {
  return AggregatableReportRequest::Create(
             {url::Origin::Create(GURL("https://a.example")),
              url::Origin::Create(GURL("https://b.example"))},
             AggregationServicePayloadContents(
                 AggregationServicePayloadContents::Operation::
                     kCountValueHistogram,
                 /*bucket=*/123, /*value=*/456,
                 AggregationServicePayloadContents::ProcessingType::kTwoParty,
                 url::Origin::Create(GURL("https://reporting.example"))),
             AggregatableReportSharedInfo(
                 /*scheduled_report_time=*/base::Time::Now(),
                 /*privacy_budget_key=*/"example_budget_key"))
      .value();
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

  TestHpkeKey hpke_key{{}, PublicKey(key_id, public_key)};
  EVP_HPKE_KEY_copy(&hpke_key.full_hpke_key, key.get());

  return hpke_key;
}

}  // namespace aggregation_service

TestAggregatableReportManager::TestAggregatableReportManager(
    const base::Clock* clock)
    : storage_(base::SequenceBound<AggregationServiceStorageSql>(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
          /*run_in_memory=*/true,
          /*path_to_database=*/base::FilePath(),
          clock)) {}

TestAggregatableReportManager::~TestAggregatableReportManager() = default;

const base::SequenceBound<content::AggregationServiceKeyStorage>&
TestAggregatableReportManager::GetKeyStorage() {
  return storage_;
}

}  // namespace content
