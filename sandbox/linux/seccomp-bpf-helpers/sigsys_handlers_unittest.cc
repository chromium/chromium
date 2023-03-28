// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"

#include <sys/socket.h>
#include <sys/syscall.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

// NOTE: most of the SIGSYS handlers are tested in
// baseline_policy_unittest.cc and syscall_parameters_restrictions_unittests.cc.

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

// On x86, socket-related syscalls typically go through socketcall() instead of
// using the direct syscall interface which was only introduced to x86 in
// Linux 4.3. There's no way to force the libc to use the direct syscall
// interface (bionic in particular does not support this) even if it exists.
// socketcall() can't be filtered so there will be no crashing.
#if !defined(__i386__)
class DisallowSocketPolicy : public bpf_dsl::Policy {
 public:
  DisallowSocketPolicy() = default;
  ~DisallowSocketPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_socket:
        return CrashSIGSYSSocket();
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
  socket(0, 0, 0);
}

class DisallowSockoptPolicy : public bpf_dsl::Policy {
 public:
  DisallowSockoptPolicy() = default;
  ~DisallowSockoptPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_setsockopt:
      case __NR_getsockopt:
        return CrashSIGSYSSockopt();
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
  getsockopt(0, 0, 0, nullptr, nullptr);
}

#endif  // !defined(__i386__)

}  // namespace
}  // namespace sandbox
