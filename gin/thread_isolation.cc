// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/thread_isolation.h"

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)

#include <sys/mman.h>
#include <cstddef>

#include "base/allocator/partition_allocator/thread_isolation/alignment.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/page_size.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"

namespace {

int PkeyAlloc(int access_rights) {
#ifdef SYS_pkey_alloc
  return syscall(SYS_pkey_alloc, 0, access_rights);
#else
  return -1;
#endif
}

uint32_t Rdpkru() {
  uint32_t pkru;
  asm volatile(".byte 0x0f,0x01,0xee\n" : "=a"(pkru) : "c"(0), "d"(0));
  return pkru;
}

void Wrpkru(uint32_t pkru) {
  asm volatile(".byte 0x0f,0x01,0xef\n" : : "a"(pkru), "c"(0), "d"(0));
}

static constexpr uint32_t kBitsPerPkey = 2;

void PkeyDisableWriteAccess(int pkey) {
#ifdef PKEY_DISABLE_WRITE
  uint32_t pkru = Rdpkru();
  uint32_t disable_write = static_cast<uint32_t>(PKEY_DISABLE_WRITE)
                           << (static_cast<uint32_t>(pkey) * kBitsPerPkey);
  Wrpkru(pkru | disable_write);
#endif
}

}  // namespace

namespace gin {

void ThreadIsolationData::InitializeBeforeThreadCreation() {
  bool page_size_mismatch = PA_THREAD_ISOLATED_ALIGN_SZ < base::GetPageSize();
  base::UmaHistogramBoolean("V8.CFIPageSizeMismatch", page_size_mismatch);
  if (page_size_mismatch) {
    // We write-protect global variables and need to align and pad them to (a
    // multiple of) the OS page size. But since page size is not a compile time
    // constant, check at runtime that our value was large enough.
    return;
  }

  pkey = PkeyAlloc(0);
  if (pkey == -1) {
    return;
  }
  allocator->Initialize(pkey);
  PkeyDisableWriteAccess(pkey);
}

bool ThreadIsolationData::Initialized() const {
  return pkey != -1;
}

ThreadIsolationData& GetThreadIsolationData() {
  static ThreadIsolationData thread_isolation_data PA_THREAD_ISOLATED_ALIGN;
  DCHECK_EQ((reinterpret_cast<size_t>(&thread_isolation_data) %
             PA_THREAD_ISOLATED_ALIGN_SZ),
            0llu);
  return thread_isolation_data;
}

}  // namespace gin

#endif  // BUILDFLAG(ENABLE_THREAD_ISOLATION)
