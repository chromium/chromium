// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_test_utils.h"

#include <tuple>

#include "base/task/thread_pool.h"
#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"

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
