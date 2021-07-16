// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_test_utils.h"

#include <tuple>

#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"

namespace content {

namespace aggregation_service {

testing::AssertionResult PublicKeysEqual(const std::vector<PublicKey>& expected,
                                         const std::vector<PublicKey>& actual) {
  const auto tie = [](const PublicKey& key) {
    return std::make_tuple(key.id(), key.key(), key.not_before_time(),
                           key.not_after_time());
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

TestAggregatableReportManager::TestAggregatableReportManager()
    : storage_(base::SequenceBound<AggregationServiceStorage>(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}))) {}

TestAggregatableReportManager::~TestAggregatableReportManager() = default;

const base::SequenceBound<content::AggregationServiceKeyStorage>&
TestAggregatableReportManager::GetKeyStorage() {
  return storage_;
}

}  // namespace content
