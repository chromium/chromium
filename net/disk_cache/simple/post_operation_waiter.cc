// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/post_operation_waiter.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"

namespace disk_cache {

SimplePostOperationWaiterTable::SimplePostOperationWaiterTable() = default;
SimplePostOperationWaiterTable::~SimplePostOperationWaiterTable() = default;

void SimplePostOperationWaiterTable::OnOperationStart(uint64_t entry_hash) {
  auto [_, inserted] = entries_pending_operation_.emplace(
      entry_hash, std::vector<base::OnceClosure>());
  CHECK(inserted);
}

void SimplePostOperationWaiterTable::OnOperationComplete(uint64_t entry_hash) {
  auto it = entries_pending_operation_.find(entry_hash);
  CHECK(it != entries_pending_operation_.end());
  std::vector<base::OnceClosure> to_handle_waiters;
  to_handle_waiters.swap(it->second);
  entries_pending_operation_.erase(it);

  for (base::OnceClosure& post_operation : to_handle_waiters) {
    std::move(post_operation).Run();
  }
}

std::vector<base::OnceClosure>* SimplePostOperationWaiterTable::Find(
    uint64_t entry_hash) {
  auto doom_it = entries_pending_operation_.find(entry_hash);
  if (doom_it != entries_pending_operation_.end()) {
    return &doom_it->second;
  } else {
    return nullptr;
  }
}

}  // namespace disk_cache
