// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_platform_thread_isolated_allocator.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_root.h"

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)

#include <sys/mman.h>
#include <sys/syscall.h>

#include "base/allocator/partition_allocator/src/partition_alloc/thread_isolation/pkey.h"
#include "gin/thread_isolation.h"

#if BUILDFLAG(ENABLE_PKEYS)
#else  // BUILDFLAG(ENABLE_PKEYS)
#error Not implemented for non-pkey thread isolation
#endif  // BUILDFLAG(ENABLE_PKEYS)

namespace gin {

ThreadIsolatedAllocator::ThreadIsolatedAllocator() = default;
ThreadIsolatedAllocator::~ThreadIsolatedAllocator() = default;

void ThreadIsolatedAllocator::Initialize(int pkey) {
  pkey_ = pkey;
  partition_alloc::PartitionOptions opts;
  opts.aligned_alloc = partition_alloc::PartitionOptions::kAllowed;
  opts.thread_isolation = partition_alloc::ThreadIsolationOption(pkey_);
  allocator_.init(opts);
}

void* ThreadIsolatedAllocator::Allocate(size_t size) {
  return allocator_.root()->AllocInline<partition_alloc::AllocFlags::kNoHooks>(
      size);
}

void ThreadIsolatedAllocator::Free(void* object) {
  allocator_.root()->FreeInline<partition_alloc::FreeFlags::kNoHooks>(object);
}

enum ThreadIsolatedAllocator::Type ThreadIsolatedAllocator::Type() const {
  return Type::kPkey;
}

int ThreadIsolatedAllocator::Pkey() const {
  return pkey_;
}

}  // namespace gin

#endif  // BUILDFLAG(ENABLE_THREAD_ISOLATION)
