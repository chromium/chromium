// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy_android.h"

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "sandbox/linux/seccomp-bpf/bpf_tests.h"

namespace sandbox {
namespace {

BPF_TEST_C(BaselinePolicyAndroid, Getrusage, BaselinePolicyAndroid) {
  struct rusage usage{};

  errno = 0;
  BPF_ASSERT_EQ(0, getrusage(RUSAGE_SELF, &usage));

  errno = 0;
  BPF_ASSERT_EQ(0, getrusage(RUSAGE_THREAD, &usage));
}

BPF_TEST_C(BaselinePolicyAndroid, CanOpenProcCpuinfo, BaselinePolicyAndroid) {
  // This is required for |android_getCpuFeatures()|, which is used to enable
  // various fast paths in the renderer (for instance, in zlib).
  //
  // __NR_open is blocked in 64 bit mode, but as long as libc's open() redirects
  // open() to openat(), then this should work. Make sure this stays true.
  BPF_ASSERT_NE(-1, open("/proc/cpuinfo", O_RDONLY));
}

BPF_TEST_C(BaselinePolicyAndroid, Membarrier, BaselinePolicyAndroid) {
  // Should not crash.
  syscall(__NR_membarrier, 32 /* cmd */, 0 /* flags */);
}

}  // namespace
}  // namespace sandbox
