// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_POST_DOOM_WAITER_H_
#define NET_DISK_CACHE_SIMPLE_POST_DOOM_WAITER_H_

#include <stdint.h>

#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/cache_type.h"

namespace disk_cache {

struct SimplePostDoomWaiter {
  SimplePostDoomWaiter();
  // Also initializes |time_queued|.
  explicit SimplePostDoomWaiter(base::OnceClosure to_run_post_doom);
  explicit SimplePostDoomWaiter(SimplePostDoomWaiter&& other);
  ~SimplePostDoomWaiter();
  SimplePostDoomWaiter& operator=(SimplePostDoomWaiter&& other);

  base::TimeTicks time_queued;
  base::OnceClosure run_post_doom;
};

// See |SimpleBackendImpl::post_doom_waiting_| for the description. This is
// refcounted since sometimes this needs to survive backend destruction to
// complete some per-entry operations.
class SimplePostDoomWaiterTable
    : public base::RefCounted<SimplePostDoomWaiterTable> {
  friend class base::RefCounted<SimplePostDoomWaiterTable>;

 public:
  explicit SimplePostDoomWaiterTable(net::CacheType cache_type);

  // The entry for |entry_hash| is being doomed; the backend will not attempt
  // to run new operations for this |entry_hash| until the Doom is completed.
  void OnDoomStart(uint64_t entry_hash);

  // The entry for |entry_hash| has been successfully doomed, we can now allow
  // operations on this entry, and we can run any operations enqueued while the
  // doom completed.
  void OnDoomComplete(uint64_t entry_hash);

  // Returns nullptr if not found.
  std::vector<SimplePostDoomWaiter>* Find(uint64_t entry_hash);

  bool Has(uint64_t entry_hash) {
    return entries_pending_doom_.find(entry_hash) !=
           entries_pending_doom_.end();
  }

 private:
  ~SimplePostDoomWaiterTable();

  net::CacheType cache_type_;
  std::unordered_map<uint64_t, std::vector<SimplePostDoomWaiter>>
      entries_pending_doom_;

  DISALLOW_COPY_AND_ASSIGN(SimplePostDoomWaiterTable);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_POST_DOOM_WAITER_H_
