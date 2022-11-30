// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/file_lock.h"

#include <atomic>

#include "build/build_config.h"

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
  (*updating_) = (*updating_) + 1;
  Barrier();
  acquired_ = true;
}

FileLock::~FileLock() {
  Unlock();
}

void FileLock::Lock() {
  if (acquired_)
    return;
  (*updating_) = (*updating_) + 1;
  Barrier();
}

void FileLock::Unlock() {
  if (!acquired_)
    return;
  Barrier();
  (*updating_) = (*updating_) - 1;
}

}  // namespace disk_cache
