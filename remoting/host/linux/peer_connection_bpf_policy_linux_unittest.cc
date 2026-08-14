// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/peer_connection_bpf_policy_linux.h"

#include <sys/syscall.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

using sandbox::bpf_dsl::ResultExpr;

struct SyscallTestParam {
  const char* name;
  int sysno;
};

// -----------------------------------------------------------------------------
// Denied Syscalls Test Suite
// -----------------------------------------------------------------------------

class PeerConnectionBpfDeniedSyscallsTest
    : public testing::TestWithParam<SyscallTestParam> {};

TEST_P(PeerConnectionBpfDeniedSyscallsTest, IsDenied) {
  PeerConnectionBpfPolicyLinux policy;
  ResultExpr expr = policy.EvaluateSyscall(GetParam().sysno);
  ASSERT_TRUE(expr);
  EXPECT_TRUE(expr->IsDeny())
      << "Syscall " << GetParam().name << " was expected to be denied.";
  EXPECT_FALSE(expr->IsAllow());
}

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionBpfPolicyLinuxTest,
    PeerConnectionBpfDeniedSyscallsTest,
    testing::Values(
#if defined(__NR_execveat)
        SyscallTestParam{"execveat", __NR_execveat},
#endif
#if defined(__NR_fork)
        SyscallTestParam{"fork", __NR_fork},
#endif
#if defined(__NR_vfork)
        SyscallTestParam{"vfork", __NR_vfork},
#endif
#if defined(__NR_clone3)
        SyscallTestParam{"clone3", __NR_clone3},
#endif
#if defined(__NR_unshare)
        SyscallTestParam{"unshare", __NR_unshare},
#endif
#if defined(__NR_setns)
        SyscallTestParam{"setns", __NR_setns},
#endif
#if defined(__NR_mkdir)
        SyscallTestParam{"mkdir", __NR_mkdir},
#endif
#if defined(__NR_chmod)
        SyscallTestParam{"chmod", __NR_chmod},
#endif
#if defined(__NR_chown)
        SyscallTestParam{"chown", __NR_chown},
#endif
        SyscallTestParam{"execve", __NR_execve},
        SyscallTestParam{"ptrace", __NR_ptrace},
        SyscallTestParam{"mount", __NR_mount},
        SyscallTestParam{"chroot", __NR_chroot}),
    [](const testing::TestParamInfo<SyscallTestParam>& info) {
      return info.param.name;
    });

// -----------------------------------------------------------------------------
// Allowed Syscalls Test Suite
// -----------------------------------------------------------------------------

class PeerConnectionBpfAllowedSyscallsTest
    : public testing::TestWithParam<SyscallTestParam> {};

TEST_P(PeerConnectionBpfAllowedSyscallsTest, IsAllowed) {
  PeerConnectionBpfPolicyLinux policy;
  ResultExpr expr = policy.EvaluateSyscall(GetParam().sysno);
  ASSERT_TRUE(expr);
  EXPECT_TRUE(expr->IsAllow())
      << "Syscall " << GetParam().name << " was expected to be allowed.";
  EXPECT_FALSE(expr->IsDeny());
}

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionBpfPolicyLinuxTest,
    PeerConnectionBpfAllowedSyscallsTest,
    testing::Values(
#if defined(__NR_epoll_wait)
        SyscallTestParam{"epoll_wait", __NR_epoll_wait},
#endif
#if defined(__NR_open)
        SyscallTestParam{"open", __NR_open},
#endif
#if defined(__NR_openat)
        SyscallTestParam{"openat", __NR_openat},
#endif
#if defined(__NR_unlink)
        SyscallTestParam{"unlink", __NR_unlink},
#endif
#if defined(__NR_unlinkat)
        SyscallTestParam{"unlinkat", __NR_unlinkat},
#endif
        SyscallTestParam{"read", __NR_read},
        SyscallTestParam{"write", __NR_write},
        SyscallTestParam{"close", __NR_close},
        SyscallTestParam{"futex", __NR_futex}),
    [](const testing::TestParamInfo<SyscallTestParam>& info) {
      return info.param.name;
    });

// -----------------------------------------------------------------------------
// Conditional Syscalls Test Suite
// -----------------------------------------------------------------------------

TEST(PeerConnectionBpfPolicyLinuxTest, ConditionalSyscallsAreNotUnconditional) {
  PeerConnectionBpfPolicyLinux policy;

  ResultExpr clone_expr = policy.EvaluateSyscall(__NR_clone);
  ASSERT_TRUE(clone_expr);
  EXPECT_FALSE(clone_expr->IsAllow());
  EXPECT_FALSE(clone_expr->IsDeny());

  ResultExpr socket_expr = policy.EvaluateSyscall(__NR_socket);
  ASSERT_TRUE(socket_expr);
  EXPECT_FALSE(socket_expr->IsAllow());
  EXPECT_FALSE(socket_expr->IsDeny());
}

}  // namespace
}  // namespace remoting
