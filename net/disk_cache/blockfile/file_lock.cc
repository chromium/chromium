// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/file_lock.h"

#include <atomic>

namespace {

void Barrier() {
#if !defined(COMPILER_MSVC)
  // VS uses memory barrier semantics for volatiles.
  std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}

}  // namespace

namespace disk_cache {

FileLock::FileLock(BlockFileHeader* header) {
  updating_ = &header->updating;
  (*updating_)++;
  Barrier();
  acquired_ = true;
}

FileLock::~FileLock() {
  Unlock();
}

void FileLock::Lock() {
  if (acquired_)
    return;
  (*updating_)++;
  Barrier();
}

void FileLock::Unlock() {
  if (!acquired_)
    return;
  Barrier();
  (*updating_)--;
}

}  // namespace disk_cache
