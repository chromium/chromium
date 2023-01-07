// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"

#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {

// NOTE: most tests for the SandboxBPF class are currently in
// integration_tests/.

TEST(SandboxBPF, CreateDestroy) {
  // Give an opportunity to dynamic tools to perform some simple testing.
  SandboxBPF sandbox(nullptr);
  SandboxBPF* sandbox_ptr = new SandboxBPF(nullptr);
  delete sandbox_ptr;
}

// This test should execute no matter whether we have kernel support. So,
// we make it a TEST() instead of a BPF_TEST().
TEST(SandboxBPF, DISABLE_ON_TSAN(CallSupports)) {
  // We check that we don't crash, but it's ok if the kernel doesn't
  // support it.
  bool seccomp_bpf_supported = SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::SINGLE_THREADED);
  bool seccomp_bpf_tsync_supported = SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::MULTI_THREADED);
  // We want to log whether or not seccomp BPF is actually supported
  // since actual test coverage depends on it.
  std::cout << "Seccomp BPF supported (single thread): "
            << (seccomp_bpf_supported ? "true." : "false.") << "\n";
  std::cout << "Seccomp BPF supported (multi thread): "
            << (seccomp_bpf_tsync_supported ? "true." : "false.") << "\n";
  std::cout << "Pointer size: " << sizeof(void*) << "\n";
}

SANDBOX_TEST(SandboxBPF, DISABLE_ON_TSAN(CallSupportsTwice)) {
  bool single1 = SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::SINGLE_THREADED);
  bool single2 = SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::SINGLE_THREADED);
  ASSERT_EQ(single1, single2);
  bool multi1 = SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::MULTI_THREADED);
  bool multi2 = SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::MULTI_THREADED);
  ASSERT_EQ(multi1, multi2);

  // Multi threaded support implies single threaded support.
  if (multi1) {
    ASSERT_TRUE(single1);
  }
}

TEST(SandboxBPF, ProcTaskFdDescriptorGetsClosed) {
  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));
  base::ScopedFD read_end(pipe_fds[0]);
  base::ScopedFD write_end(pipe_fds[1]);

  {
    SandboxBPF sandbox(nullptr);
    sandbox.SetProcFd(std::move(write_end));
  }

  ASSERT_EQ(0, fcntl(read_end.get(), F_SETFL, O_NONBLOCK));
  char c;
  // Check that the sandbox closed the write_end (read will EOF instead of
  // returning EWOULDBLOCK).
  ASSERT_EQ(0, read(read_end.get(), &c, 1));
}

}  // namespace
}  // sandbox
