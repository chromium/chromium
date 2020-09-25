// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/synchronization/one_writer_seqlock.h"

namespace device {

OneWriterSeqLock::OneWriterSeqLock() : sequence_(0) {}

void OneWriterSeqLock::AtomicWriterMemcpy(void* dest,
                                          const void* src,
                                          size_t size) {
  DCHECK(!(reinterpret_cast<std::uintptr_t>(dest) % 4));
  DCHECK(!(reinterpret_cast<std::uintptr_t>(src) % 4));
  DCHECK(size % 4 == 0);
  for (size_t i = 0; i < size / 4; ++i) {
    reinterpret_cast<std::atomic<int32_t>*>(dest)[i].store(
        reinterpret_cast<const int32_t*>(src)[i], std::memory_order_relaxed);
  }
}

void OneWriterSeqLock::AtomicReaderMemcpy(void* dest,
                                          const void* src,
                                          size_t size) {
  DCHECK(!(reinterpret_cast<std::uintptr_t>(dest) % 4));
  DCHECK(!(reinterpret_cast<std::uintptr_t>(src) % 4));
  DCHECK(size % 4 == 0);
  for (size_t i = 0; i < size / 4; ++i) {
    reinterpret_cast<int32_t*>(dest)[i] =
        reinterpret_cast<const std::atomic<int32_t>*>(src)[i].load(
            std::memory_order_relaxed);
  }
}

int32_t OneWriterSeqLock::ReadBegin(uint32_t max_retries) const {
  int32_t version;
  for (uint32_t i = 0; i <= max_retries; ++i) {
    version = sequence_.load(std::memory_order_acquire);

    // If the counter is even, then the associated data might be in a
    // consistent state, so we can try to read.
    if ((version & 1) == 0)
      break;

    // Otherwise, the writer is in the middle of an update. Retry the read.
    // In a multiprocessor environment with short write time, thread yield is
    // expensive and best be avoided during the first 10~100 iterations.
    if (i > 10)
      base::PlatformThread::YieldCurrentThread();
  }
  return version;
}

bool OneWriterSeqLock::ReadRetry(int32_t version) const {
  // If the sequence number was updated then a read should be re-attempted.
  // -- Load fence, read membarrier
  atomic_thread_fence(std::memory_order_acquire);
  return sequence_.load(std::memory_order_relaxed) != version;
}

void OneWriterSeqLock::WriteBegin() {
  // Increment the sequence number to odd to indicate the beginning of a write
  // update.
  int32_t version = sequence_.fetch_add(1, std::memory_order_relaxed);
  atomic_thread_fence(std::memory_order_release);
  DCHECK((version & 1) == 0);
  // -- Store fence, write membarrier
}

void OneWriterSeqLock::WriteEnd() {
  // Increment the sequence to an even number to indicate the completion of
  // a write update.
  // -- Store fence, write membarrier
  int32_t version = sequence_.fetch_add(1, std::memory_order_release);
  DCHECK((version & 1) != 0);
}

}  // namespace device
