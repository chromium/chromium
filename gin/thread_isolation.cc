// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/thread_isolation.h"

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

#include <sys/mman.h>
#include <sys/utsname.h>

#include <cstddef>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/memory/page_size.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "partition_alloc/thread_isolation/alignment.h"

WEAK_SYMBOL extern int pkey_alloc(unsigned int flags,
                                  unsigned int access_rights);

namespace {

bool KernelHasPkruFix() {
  // PKU was broken on Linux kernels before 5.13 (see
  // https://lore.kernel.org/all/20210623121456.399107624@linutronix.de/).
  // A fix is also included in the 5.4.182 and 5.10.103 versions ("x86/fpu:
  // Correct pkru/xstate inconsistency" by Brian Geffon <bgeffon@google.com>).
  // Thus check the kernel version we are running on, and bail out if does not
  // contain the fix.
  struct utsname uname_buffer;
  CHECK_EQ(0, uname(&uname_buffer));
  int kernel, major, minor;
  // Conservatively return if the release does not match the format we expect.
  if (sscanf(uname_buffer.release, "%d.%d.%d", &kernel, &major, &minor) != 3) {
    return -1;
  }
  return kernel > 5 || (kernel == 5 && major >= 13) ||   // anything >= 5.13
         (kernel == 5 && major == 4 && minor >= 182) ||  // 5.4 >= 5.4.182
         (kernel == 5 && major == 10 && minor >= 103);   // 5.10 >= 5.10.103
}

int PkeyAlloc(int access_rights) {
  if (!pkey_alloc) {
    return -1;
  }

  static bool kernel_has_pkru_fix = KernelHasPkruFix();
  if (!kernel_has_pkru_fix) {
    return -1;
  }

  return pkey_alloc(0, access_rights);
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
  static ThreadIsolationData thread_isolation_data;
  DCHECK_EQ((reinterpret_cast<size_t>(&thread_isolation_data) %
             PA_THREAD_ISOLATED_ALIGN_SZ),
            0llu);
  return thread_isolation_data;
}

}  // namespace gin

#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
