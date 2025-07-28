// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"

#include <linux/net.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#include "base/check_op.h"
#include "base/strings/safe_sprintf.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

// NOTE: most of the SIGSYS handlers are tested in
// baseline_policy_unittest.cc and syscall_parameters_restrictions_unittests.cc.

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

class DisallowSocketPolicy : public bpf_dsl::Policy {
 public:
  DisallowSocketPolicy() {
#if defined(__NR_socketcall)
    // If the socket(2) syscall isn't available, then socketcall(2) might be
    // used which can't be filtered with seccomp.
    if (!CanRewriteSocketcall()) {
      UnitTests::IgnoreThisTest();
      return;
    }
#endif  // defined(__NR_socketcall)
  }
  ~DisallowSocketPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_socket: {
        const Arg<int> domain(0);
        return If(domain == AF_INET, CrashSIGSYSSocket()).Else(Allow());
      }
#if defined(__NR_socketcall)
      case __NR_socketcall:
        return RewriteSocketcallSIGSYS();
#endif  // defined(__NR_socketcall)
      default:
        return Allow();
    }
  }
};

BPF_DEATH_TEST_C(
    SigsysHandlers,
    SocketPrintsCorrectMessage,
    DEATH_SEGV_MESSAGE(sandbox::GetSocketErrorMessageContentForTests()),
    DisallowSocketPolicy) {
  socket(AF_INET, 0, 0);
}

class DisallowSockoptPolicy : public bpf_dsl::Policy {
 public:
  DisallowSockoptPolicy() {
#if defined(__NR_socketcall)
    // If the setsockopt(2) or getsockopt(2) syscalls aren't available, then
    // socketcall(2) might be used which can't be filtered with seccomp.
    if (!CanRewriteSocketcall()) {
      UnitTests::IgnoreThisTest();
      return;
    }
#endif  // defined(__NR_socketcall)
  }
  ~DisallowSockoptPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_setsockopt:
      case __NR_getsockopt:
        return CrashSIGSYSSockopt();
#if defined(__NR_socketcall)
      case __NR_socketcall:
        return RewriteSocketcallSIGSYS();
#endif  // defined(__NR_socketcall)
      default:
        return Allow();
    }
  }
};

BPF_DEATH_TEST_C(
    SigsysHandlers,
    SetsockoptPrintsCorrectMessage,
    DEATH_SEGV_MESSAGE(sandbox::GetSockoptErrorMessageContentForTests()),
    DisallowSockoptPolicy) {
  setsockopt(0, 0, 0, nullptr, 0);
}

BPF_DEATH_TEST_C(
    SigsysHandlers,
    GetSockoptPrintsCorrectMessage,
    DEATH_SEGV_MESSAGE(sandbox::GetSockoptErrorMessageContentForTests()),
    DisallowSockoptPolicy) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
  getsockopt(0, 0, 0, nullptr, nullptr);
#pragma GCC diagnostic pop
}

const char kSigsysMessage[] =
    "nr=0x42 arg1=0xffffffffffffffff arg2=0x0 arg3=0xabcdef arg4=0xffffffff";

SANDBOX_DEATH_TEST(SigsysHandlers,
                   SigsysErrorDetails,
                   DEATH_SEGV_MESSAGE(kSigsysMessage)) {
  arch_seccomp_data args = {.nr = 0x42,
                            .args = {static_cast<uint64_t>(-1), 0, 0xabcdef,
                                     static_cast<uint32_t>(-1)}};
  CrashSIGSYS_Handler(args, nullptr);
}

#if defined(__NR_socketcall)
// Test the socketcall(2) rewrites below.

const int kDirectSocketSyscallRetVal = 37;
const size_t kNumArgsToCopy = 6;
uint64_t g_syscall_args[kNumArgsToCopy];
int g_syscall_no;

SANDBOX_EXPORT intptr_t
SIGSYSDirectSocketSyscallHandler(const struct arch_seccomp_data& args, void*) {
  // Record syscall args.
  memcpy(g_syscall_args, args.args, kNumArgsToCopy * sizeof(uint64_t));
  g_syscall_no = args.nr;
  return kDirectSocketSyscallRetVal;
}

class RewriteSocketcallPolicy : public bpf_dsl::Policy {
 public:
  RewriteSocketcallPolicy() {
    if (!CanRewriteSocketcall()) {
      UnitTests::IgnoreThisTest();
      return;
    }
  }
  ~RewriteSocketcallPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_socketcall:
        return RewriteSocketcallSIGSYS();
      case __NR_socket:
      case __NR_bind:
      case __NR_connect:
      case __NR_listen:
      case __NR_getsockname:
      case __NR_getpeername:
      case __NR_socketpair:
      case __NR_sendto:
      case __NR_recvfrom:
      case __NR_shutdown:
      case __NR_setsockopt:
      case __NR_getsockopt:
      case __NR_sendmsg:
      case __NR_recvmsg:
      case __NR_accept4:
      case __NR_recvmmsg:
      case __NR_sendmmsg:
        return bpf_dsl::Trap(&SIGSYSDirectSocketSyscallHandler, nullptr);
      default:
        return Allow();
    }
  }
};

template <size_t N>
void CheckArgsMatch(long current_socketcall,
                    int sysno,
                    unsigned long (&expected_args)[N]) {
  BPF_ASSERT_EQ(sysno, g_syscall_no);
  // If the args don't match, crash to fail the test.
  for (size_t i = 0; i < N; i++) {
    unsigned long rewritten_socketcall_arg =
        *reinterpret_cast<unsigned long*>(&g_syscall_args[i]);
    CHECK_EQ(rewritten_socketcall_arg, expected_args[i])
        << "Socketcall " << current_socketcall << " differs at argument " << i;
  }
}

BPF_TEST_C(SigsysHandlers,
           DirectSocketSyscallArgsMatch,
           RewriteSocketcallPolicy) {
  // The arguments to every socketcall will be 1,2,3,4,5,6 and the return value
  // will always be `kDirectSocketSyscallRetVal`.
  // Some socketcalls have fewer than 6 arguments but it doesn't hurt to have a
  // longer array, the extra arguments will just be ignored.
  unsigned long socketcall_args[6] = {1, 2, 3, 4, 5, 6};
  // socketcalls range from SYS_SOCKET (1) to SYS_SENDMMSG (20).
  for (long i = 1; i <= 20; i++) {
    // For every socketcall, call the __NR_socketcall version and it should call
    // the direct version, which our BPF policy will catch and record the
    // syscall arguments in `g_syscall_args`. Then check the args are as
    // expected.
    switch (i) {
      case SYS_SOCKET: {
        unsigned long expected_args[3] = {1, 2, 3};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_socket, expected_args);
        break;
      }
      case SYS_BIND: {
        unsigned long expected_args[3] = {1, 2, 3};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_bind, expected_args);
        break;
      }
      case SYS_CONNECT: {
        unsigned long expected_args[3] = {1, 2, 3};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_connect, expected_args);
        break;
      }
      case SYS_LISTEN: {
        unsigned long expected_args[2] = {1, 2};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_listen, expected_args);
        break;
      }
      case SYS_ACCEPT: {
        // Should call accept4 with flags == 0.
        unsigned long expected_args[4] = {1, 2, 3, 0};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_accept4, expected_args);
        break;
      }
      case SYS_GETSOCKNAME: {
        unsigned long expected_args[3] = {1, 2, 3};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_getsockname, expected_args);
        break;
      }
      case SYS_GETPEERNAME: {
        unsigned long expected_args[3] = {1, 2, 3};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_getpeername, expected_args);
        break;
      }
      case SYS_SOCKETPAIR: {
        unsigned long expected_args[4] = {1, 2, 3, 4};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_socketpair, expected_args);
        break;
      }
      case SYS_SEND: {
        // Should call sendto() with last two args equal to 0.
        unsigned long expected_args[6] = {1, 2, 3, 4, 0, 0};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_sendto, expected_args);
        break;
      }
      case SYS_RECV: {
        // Should call recvfrom() with last two args equal to 0.
        unsigned long expected_args[6] = {1, 2, 3, 4, 0, 0};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_recvfrom, expected_args);
        break;
      }
      case SYS_SENDTO: {
        unsigned long expected_args[6] = {1, 2, 3, 4, 5, 6};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_sendto, expected_args);
        break;
      }
      case SYS_RECVFROM: {
        unsigned long expected_args[6] = {1, 2, 3, 4, 5, 6};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_recvfrom, expected_args);
        break;
      }
      case SYS_SHUTDOWN: {
        unsigned long expected_args[2] = {1, 2};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_shutdown, expected_args);
        break;
      }
      case SYS_SETSOCKOPT: {
        unsigned long expected_args[5] = {1, 2, 3, 4, 5};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_setsockopt, expected_args);
        break;
      }
      case SYS_GETSOCKOPT: {
        unsigned long expected_args[5] = {1, 2, 3, 4, 5};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_getsockopt, expected_args);
        break;
      }
      case SYS_SENDMSG: {
        unsigned long expected_args[3] = {1, 2, 3};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_sendmsg, expected_args);
        break;
      }
      case SYS_RECVMSG: {
        unsigned long expected_args[3] = {1, 2, 3};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_recvmsg, expected_args);
        break;
      }
      case SYS_ACCEPT4: {
        unsigned long expected_args[4] = {1, 2, 3, 4};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_accept4, expected_args);
        break;
      }
      case SYS_RECVMMSG: {
        unsigned long expected_args[5] = {1, 2, 3, 4, 5};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_recvmmsg, expected_args);
        break;
      }
      case SYS_SENDMMSG: {
        unsigned long expected_args[4] = {1, 2, 3, 4};
        BPF_ASSERT_EQ(syscall(__NR_socketcall, i, socketcall_args),
                      kDirectSocketSyscallRetVal);
        CheckArgsMatch(i, __NR_sendmmsg, expected_args);
        break;
      }
    }
  }
}

#endif  // defined(__NR_socketcall)

}  // namespace
}  // namespace sandbox
