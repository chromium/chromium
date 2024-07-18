// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/bpf_dsl/syscall_set.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "sandbox/linux/bpf_dsl/linux_syscall_ranges.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

const SyscallSet kSyscallSets[] = {
    SyscallSet::All(),
    SyscallSet::InvalidOnly(),
};

SANDBOX_TEST(SyscallSet, Monotonous) {
  for (const SyscallSet& set : kSyscallSets) {
    uint32_t prev = 0;
    bool have_prev = false;
    for (uint32_t sysnum : set) {
      if (have_prev) {
        SANDBOX_ASSERT(sysnum > prev);
      } else if (set == SyscallSet::All()) {
        // The iterator should start at 0.
        SANDBOX_ASSERT(sysnum == 0);
      }

      prev = sysnum;
      have_prev = true;
    }

    // The iterator should always return 0xFFFFFFFFu as the last value.
    SANDBOX_ASSERT(have_prev);
    SANDBOX_ASSERT(prev == 0xFFFFFFFFu);
  }
}

// AssertRange checks that SyscallIterator produces all system call
// numbers in the inclusive range [min, max].
void AssertRange(uint32_t min, uint32_t max) {
  SANDBOX_ASSERT(min < max);
  uint32_t prev = min - 1;
  for (uint32_t sysnum : SyscallSet::All()) {
    if (sysnum >= min && sysnum <= max) {
      SANDBOX_ASSERT(prev == sysnum - 1);
      prev = sysnum;
    }
  }
  SANDBOX_ASSERT(prev == max);
}

SANDBOX_TEST(SyscallSet, ValidSyscallRanges) {
  AssertRange(MIN_SYSCALL, MAX_PUBLIC_SYSCALL);
#if defined(__arm__)
  AssertRange(MIN_PRIVATE_SYSCALL, MAX_PRIVATE_SYSCALL);
  AssertRange(MIN_GHOST_SYSCALL, MAX_SYSCALL);
#endif
}

SANDBOX_TEST(SyscallSet, InvalidSyscalls) {
  static const uint32_t kExpected[] = {
#if defined(__mips__)
    0,
    MIN_SYSCALL - 1,
#endif
    MAX_PUBLIC_SYSCALL + 1,
#if defined(__arm__)
    MIN_PRIVATE_SYSCALL - 1,
    MAX_PRIVATE_SYSCALL + 1,
    MIN_GHOST_SYSCALL - 1,
    MAX_SYSCALL + 1,
#endif
    0x7FFFFFFFu,
    0x80000000u,
    0xFFFFFFFFu,
  };

  for (const SyscallSet& set : kSyscallSets) {
    size_t i = 0;
    for (uint32_t sysnum : set) {
      if (!SyscallSet::IsValid(sysnum)) {
        SANDBOX_ASSERT(i < std::size(kExpected));
        SANDBOX_ASSERT(kExpected[i] == sysnum);
        ++i;
      }
    }
    SANDBOX_ASSERT(i == std::size(kExpected));
  }
}

SANDBOX_TEST(SyscallSet, ValidOnlyIsOnlyValid) {
  for (uint32_t sysnum : SyscallSet::ValidOnly()) {
    SANDBOX_ASSERT(SyscallSet::IsValid(sysnum));
  }
}

SANDBOX_TEST(SyscallSet, InvalidOnlyIsOnlyInvalid) {
  for (uint32_t sysnum : SyscallSet::InvalidOnly()) {
    SANDBOX_ASSERT(!SyscallSet::IsValid(sysnum));
  }
}

SANDBOX_TEST(SyscallSet, AllIsValidOnlyPlusInvalidOnly) {
  std::vector<uint32_t> merged;
  const SyscallSet valid_only = SyscallSet::ValidOnly();
  const SyscallSet invalid_only = SyscallSet::InvalidOnly();
  std::merge(valid_only.begin(),
             valid_only.end(),
             invalid_only.begin(),
             invalid_only.end(),
             std::back_inserter(merged));

  const SyscallSet all = SyscallSet::All();
  SANDBOX_ASSERT(merged == std::vector<uint32_t>(all.begin(), all.end()));
}

}  // namespace

}  // namespace sandbox
