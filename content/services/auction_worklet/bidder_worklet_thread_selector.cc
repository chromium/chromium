// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet_thread_selector.h"

#include <algorithm>
#include <cstddef>

#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "content/services/auction_worklet/public/cpp/auction_worklet_features.h"

namespace auction_worklet {

BidderWorkletThreadSelector::BidderWorkletThreadSelector(size_t num_threads)
    : num_threads_(num_threads),
      group_by_origin_key_hash_salt_(base::RandUint64()),
      tasks_sent_to_each_thread_(num_threads, 0) {}

BidderWorkletThreadSelector::~BidderWorkletThreadSelector() = default;

size_t BidderWorkletThreadSelector::GetThread(
    std::optional<uint64_t> group_by_origin_key) {
  if (num_threads_ == 1) {
    // Don't bother storing any information in this class
    // if we only have 1 thread.
    return 0;
  }
  if (!base::FeatureList::IsEnabled(
          features::kFledgeBidderUseBalancingThreadSelector)) {
    return GetThreadWithLegacyLogic(group_by_origin_key);
  }
  // Default to the least used thread.
  auto least_used_thread_it = std::min_element(
      tasks_sent_to_each_thread_.begin(), tasks_sent_to_each_thread_.end());
  size_t chosen_thread =
      std::distance(tasks_sent_to_each_thread_.begin(), least_used_thread_it);

  // If it exists, choose a thread that has already seen this
  // `group_by_origin_key` unless it would cause a large imbalance.
  if (group_by_origin_key) {
    auto it = group_by_origin_key_to_thread_.find(group_by_origin_key.value());
    if (it != group_by_origin_key_to_thread_.end() &&
        (tasks_sent_to_each_thread_[it->second] <
         *least_used_thread_it +
             features::kFledgeBidderThreadSelectorMaxImbalance.Get())) {
      chosen_thread = it->second;
    } else {
      // Purposely overwrite any previously chosen thread because each thread
      // uses a LRU cache of contexts and the new thread has a shorter queue.
      group_by_origin_key_to_thread_[*group_by_origin_key] = chosen_thread;
    }
  }
  CHECK_LT(chosen_thread, num_threads_);

  ++tasks_sent_to_each_thread_[chosen_thread];
  return chosen_thread;
}

void BidderWorkletThreadSelector::TaskCompletedOnThread(size_t thread) {
  CHECK_LT(thread, num_threads_);
  --tasks_sent_to_each_thread_[thread];
}

size_t BidderWorkletThreadSelector::GetThreadWithLegacyLogic(
    std::optional<uint64_t> group_by_origin_key) {
  // In 'group-by-origin' mode, make the thread assignment sticky to
  // group_by_origin_key. This favors context reuse to save memory. The
  // inclusion of the random auction ID in the hash concidentally makes sure
  // certain origins won't always be grouped together.
  if (group_by_origin_key) {
    size_t join_origin_hash =
        base::HashCombine(group_by_origin_key_hash_salt_, group_by_origin_key);
    return join_origin_hash % num_threads_;
  }
  return GetNextThread();
}

size_t BidderWorkletThreadSelector::GetNextThread() {
  size_t result = next_thread_index_;
  next_thread_index_++;
  next_thread_index_ %= num_threads_;
  return result;
}

}  // namespace auction_worklet
