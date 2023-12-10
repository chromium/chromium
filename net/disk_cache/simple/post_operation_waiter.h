// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_POST_OPERATION_WAITER_H_
#define NET_DISK_CACHE_SIMPLE_POST_OPERATION_WAITER_H_

#include <stdint.h>

#include <unordered_map>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "net/base/cache_type.h"

namespace disk_cache {

// See |SimpleBackendImpl::post_doom_waiting_| for the description. This is
// refcounted since sometimes this needs to survive backend destruction to
// complete some per-entry operations.
class SimplePostOperationWaiterTable
    : public base::RefCounted<SimplePostOperationWaiterTable> {
  friend class base::RefCounted<SimplePostOperationWaiterTable>;

 public:
  SimplePostOperationWaiterTable();

  SimplePostOperationWaiterTable(const SimplePostOperationWaiterTable&) =
      delete;
  SimplePostOperationWaiterTable& operator=(
      const SimplePostOperationWaiterTable&) = delete;

  // The entry for |entry_hash| is performing an operation like doom or opening
  // by hash; the backend will not attempt to run new operations for this
  // |entry_hash| until it is is completed.
  void OnOperationStart(uint64_t entry_hash);

  // The entry for |entry_hash| has been successfully doomed or had its key
  // figured out, we can now allow operations on this entry, and we can run any
  // operations enqueued while the operation was taking place. This will happen
  // synchronously.
  void OnOperationComplete(uint64_t entry_hash);

  // Returns nullptr if not found.
  std::vector<base::OnceClosure>* Find(uint64_t entry_hash);

  bool Has(uint64_t entry_hash) {
    return entries_pending_operation_.contains(entry_hash);
  }

 private:
  ~SimplePostOperationWaiterTable();

  std::unordered_map<uint64_t, std::vector<base::OnceClosure>>
      entries_pending_operation_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_POST_OPERATION_WAITER_H_
