// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_THREAD_ISOLATION_H_
#define GIN_THREAD_ISOLATION_H_

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

#include "base/no_destructor.h"
#include "gin/gin_export.h"
#include "gin/v8_platform_thread_isolated_allocator.h"
#include "partition_alloc/thread_isolation/alignment.h"

namespace gin {

// All data used for thread isolation needs to live in this struct since we need
// to write-protect it with the same thread isolation mechanism.
// The implementation is platform specific, e.g. using pkeys on x64.
struct GIN_EXPORT PA_THREAD_ISOLATED_ALIGN ThreadIsolationData {
  // If we're using pkeys for isolation, we need to allocate the key before
  // spawning any threads. Otherwise, the other threads will not have read
  // permissions to memory tagged with this key.
  void InitializeBeforeThreadCreation();

  // Returns if InitializeBeforeThreadCreation has been called and the
  // initialization was successful, i.e. the platform supports the isolation
  // mechanism.
  bool Initialized() const;

  base::NoDestructor<ThreadIsolatedAllocator> allocator;
  int pkey = -1;
};
static_assert(PA_THREAD_ISOLATED_FILL_PAGE_SZ(sizeof(ThreadIsolationData)) ==
              0);

GIN_EXPORT ThreadIsolationData& GetThreadIsolationData();

}  // namespace gin

#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

#endif  // GIN_THREAD_ISOLATION_H_
