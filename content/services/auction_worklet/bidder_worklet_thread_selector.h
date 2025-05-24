// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_THREAD_SELECTOR_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_THREAD_SELECTOR_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace auction_worklet {

// BidderWorkletThreadSelector chooses a thread on which to run each
// task on the bidder worklet. It prioritizes keeping tasks with
// the same group by origin key on the same thread (to help with context reuse),
// and if `kFledgeBidderUseBalancingThreadSelector` is enabled, it prevents any
// given thread from being assigned many more tasks than another.
class CONTENT_EXPORT BidderWorkletThreadSelector {
 public:
  explicit BidderWorkletThreadSelector(size_t num_threads);

  ~BidderWorkletThreadSelector();

  // If `group_by_origin_key` is null or we haven't encountered it yet,
  // the thread will be assigned to the thread with the shortest queue.
  // Otherwise, choose a thread previously used by this origin. If
  // `kFledgeBidderUseBalancingThreadSelector` is enabled and choosing the
  // previously used thread thread would cause us to exceed
  // `BidderThreadSelectorMaxImbalance`, use the thread with the shortest queue.
  size_t GetThread(std::optional<uint64_t> group_by_origin_key = std::nullopt);

  // Let this class know a thread finished a task so that we can use that
  // information for load balancing.
  void TaskCompletedOnThread(size_t thread);

  uint64_t group_by_origin_key_hash_salt_for_testing() const {
    return group_by_origin_key_hash_salt_;
  }

 private:
  // Use the previous logic for getting a thread index (hash
  // `group_by_origin_key` to get the thread or use a round robin otherwise).
  size_t GetThreadWithLegacyLogic(
      std::optional<uint64_t> group_by_origin_key = std::nullopt);

  // Get the next thread in the round robin.
  size_t GetNextThread();

  size_t next_thread_index_ = 0;

  size_t num_threads_;

  // A salt value used to hash `group_by_origin_key` if
  // `kFledgeBidderUseBalancingThreadSelector` is disabled and it's included as
  // an argument of GetThread. The hash will determine the thread responsible
  // for handling the task.
  uint64_t group_by_origin_key_hash_salt_;

  base::flat_map<uint64_t, size_t> group_by_origin_key_to_thread_;

  std::vector<size_t> tasks_sent_to_each_thread_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_THREAD_SELECTOR_H_
