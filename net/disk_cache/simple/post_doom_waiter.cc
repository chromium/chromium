// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/post_doom_waiter.h"

#include "base/bind.h"
#include "base/callback.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"

namespace disk_cache {

SimplePostDoomWaiter::SimplePostDoomWaiter() {}

SimplePostDoomWaiter::SimplePostDoomWaiter(base::OnceClosure to_run_post_doom)
    : time_queued(base::TimeTicks::Now()),
      run_post_doom(std::move(to_run_post_doom)) {}

SimplePostDoomWaiter::SimplePostDoomWaiter(SimplePostDoomWaiter&& other) =
    default;
SimplePostDoomWaiter& SimplePostDoomWaiter::operator=(
    SimplePostDoomWaiter&& other) = default;
SimplePostDoomWaiter::~SimplePostDoomWaiter() {}

SimplePostDoomWaiterTable::SimplePostDoomWaiterTable(net::CacheType cache_type)
    : cache_type_(cache_type) {}
SimplePostDoomWaiterTable::~SimplePostDoomWaiterTable() = default;

void SimplePostDoomWaiterTable::OnDoomStart(uint64_t entry_hash) {
  DCHECK_EQ(0u, entries_pending_doom_.count(entry_hash));
  entries_pending_doom_.insert(
      std::make_pair(entry_hash, std::vector<SimplePostDoomWaiter>()));
}

void SimplePostDoomWaiterTable::OnDoomComplete(uint64_t entry_hash) {
  DCHECK_EQ(1u, entries_pending_doom_.count(entry_hash));
  auto it = entries_pending_doom_.find(entry_hash);
  std::vector<SimplePostDoomWaiter> to_handle_waiters;
  to_handle_waiters.swap(it->second);
  entries_pending_doom_.erase(it);

  SIMPLE_CACHE_UMA(COUNTS_1000, "NumOpsBlockedByPendingDoom", cache_type_,
                   to_handle_waiters.size());

  for (SimplePostDoomWaiter& post_doom : to_handle_waiters) {
    SIMPLE_CACHE_UMA(TIMES, "QueueLatency.PendingDoom", cache_type_,
                     (base::TimeTicks::Now() - post_doom.time_queued));
    std::move(post_doom.run_post_doom).Run();
  }
}

std::vector<SimplePostDoomWaiter>* SimplePostDoomWaiterTable::Find(
    uint64_t entry_hash) {
  auto doom_it = entries_pending_doom_.find(entry_hash);
  if (doom_it != entries_pending_doom_.end())
    return &doom_it->second;
  else
    return nullptr;
}

}  // namespace disk_cache
