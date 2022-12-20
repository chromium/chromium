// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <map>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/linux/bpf_dsl/dump_bpf.h"
#include "sandbox/linux/bpf_dsl/golden/golden_files.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/bpf_dsl/test_trap_registry.h"
#include "sandbox/linux/bpf_dsl/verifier.h"
#include "sandbox/linux/system_headers/linux_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace bpf_dsl {
namespace {

// Helper function to construct fake arch_seccomp_data objects.
struct arch_seccomp_data FakeSyscall(int nr,
                                     uintptr_t p0 = 0,
                                     uintptr_t p1 = 0,
                                     uintptr_t p2 = 0,
                                     uintptr_t p3 = 0,
                                     uintptr_t p4 = 0,
                                     uintptr_t p5 = 0) {
  // Made up program counter for syscall address.
  const uint64_t kFakePC = 0x543210;

  struct arch_seccomp_data data = {
      nr,
      SECCOMP_ARCH,
      kFakePC,
      {
       p0, p1, p2, p3, p4, p5,
      },
  };

  return data;
}

class PolicyEmulator {
 public:
  PolicyEmulator(const golden::Golden& golden, const Policy& policy) {
    TestTrapRegistry traps;
    program_ = PolicyCompiler(&policy, &traps).Compile();

    // TODO(mdempsky): Generalize to more arches.
    const char* expected = nullptr;
#if defined(ARCH_CPU_X86)
    expected = golden.i386_dump;
#elif defined(ARCH_CPU_X86_64)
    expected = golden.x86_64_dump;
#endif

    if (expected != nullptr) {
      const std::string actual = DumpBPF::StringPrintProgram(program_);
      EXPECT_EQ(expected, actual);
    } else {
      LOG(WARNING) << "Missing golden file data entry";
    }
  }

  PolicyEmulator(const PolicyEmulator&) = delete;
  PolicyEmulator& operator=(const PolicyEmulator&) = delete;

  ~PolicyEmulator() = default;

  void ExpectAllow(const struct arch_seccomp_data& data) const {
    EXPECT_EQ(SECCOMP_RET_ALLOW, Emulate(data));
  }

  void ExpectErrno(uint16_t err, const struct arch_seccomp_data& data) const {
    EXPECT_EQ(SECCOMP_RET_ERRNO | err, Emulate(data));
  }

  void ExpectKill(const struct arch_seccomp_data& data) const {
    EXPECT_EQ(SECCOMP_RET_KILL, Emulate(data));
  }

 private:
  uint32_t Emulate(const struct arch_seccomp_data& data) const {
    const char* err = nullptr;
    uint32_t res = Verifier::EvaluateBPF(program_, data, &err);
    if (err) {
      ADD_FAILURE() << err;
      return 0;
    }
    return res;
  }

  CodeGen::Program program_;
};

class BasicPolicy : public Policy {
 public:
  BasicPolicy() {}

  BasicPolicy(const BasicPolicy&) = delete;
  BasicPolicy& operator=(const BasicPolicy&) = delete;

  ~BasicPolicy() override {}
  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_getpgid) {
      const Arg<pid_t> pid(0);
      return If(pid == 0, Error(EPERM)).Else(Error(EINVAL));
    }
    if (sysno == __NR_setuid) {
      const Arg<uid_t> uid(0);
      return If(uid != 42, Kill()).Else(Allow());
    }
    return Allow();
  }
};

TEST(BPFDSL, Basic) {
  PolicyEmulator emulator(golden::kBasicPolicy, BasicPolicy());

  emulator.ExpectErrno(EPERM, FakeSyscall(__NR_getpgid, 0));
  emulator.ExpectErrno(EINVAL, FakeSyscall(__NR_getpgid, 1));

  emulator.ExpectAllow(FakeSyscall(__NR_setuid, 42));
  emulator.ExpectKill(FakeSyscall(__NR_setuid, 43));
}

/* On IA-32, socketpair() is implemented via socketcall(). :-( */
#if !defined(ARCH_CPU_X86)
class BooleanLogicPolicy : public Policy {
 public:
  BooleanLogicPolicy() {}

  BooleanLogicPolicy(const BooleanLogicPolicy&) = delete;
  BooleanLogicPolicy& operator=(const BooleanLogicPolicy&) = delete;

  ~BooleanLogicPolicy() override {}
  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_socketpair) {
      const Arg<int> domain(0), type(1), protocol(2);
      return If(AllOf(domain == AF_UNIX,
                      AnyOf(type == SOCK_STREAM, type == SOCK_DGRAM),
                      protocol == 0),
                Error(EPERM))
          .Else(Error(EINVAL));
    }
    return Allow();
  }
};

TEST(BPFDSL, BooleanLogic) {
  PolicyEmulator emulator(golden::kBooleanLogicPolicy, BooleanLogicPolicy());

  const intptr_t kFakeSV = 0x12345;

  // Acceptable combinations that should return EPERM.
  emulator.ExpectErrno(
      EPERM, FakeSyscall(__NR_socketpair, AF_UNIX, SOCK_STREAM, 0, kFakeSV));
  emulator.ExpectErrno(
      EPERM, FakeSyscall(__NR_socketpair, AF_UNIX, SOCK_DGRAM, 0, kFakeSV));

  // Combinations that are invalid for only one reason; should return EINVAL.
  emulator.ExpectErrno(
      EINVAL, FakeSyscall(__NR_socketpair, AF_INET, SOCK_STREAM, 0, kFakeSV));
  emulator.ExpectErrno(EINVAL, FakeSyscall(__NR_socketpair, AF_UNIX,
                                           SOCK_SEQPACKET, 0, kFakeSV));
  emulator.ExpectErrno(EINVAL, FakeSyscall(__NR_socketpair, AF_UNIX,
                                           SOCK_STREAM, IPPROTO_TCP, kFakeSV));

  // Completely unacceptable combination; should also return EINVAL.
  emulator.ExpectErrno(
      EINVAL, FakeSyscall(__NR_socketpair, AF_INET, SOCK_SEQPACKET, IPPROTO_UDP,
                          kFakeSV));
}
#endif  // !ARCH_CPU_X86

class MoreBooleanLogicPolicy : public Policy {
 public:
  MoreBooleanLogicPolicy() {}

  MoreBooleanLogicPolicy(const MoreBooleanLogicPolicy&) = delete;
  MoreBooleanLogicPolicy& operator=(const MoreBooleanLogicPolicy&) = delete;

  ~MoreBooleanLogicPolicy() override {}
  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_setresuid) {
      const Arg<uid_t> ruid(0), euid(1), suid(2);
      return If(AnyOf(ruid == 0, euid == 0, suid == 0), Error(EPERM))
          .ElseIf(AllOf(ruid == 1, euid == 1, suid == 1), Error(EAGAIN))
          .Else(Error(EINVAL));
    }
    return Allow();
  }
};

TEST(BPFDSL, MoreBooleanLogic) {
  PolicyEmulator emulator(golden::kMoreBooleanLogicPolicy,
                          MoreBooleanLogicPolicy());

  // Expect EPERM if any set to 0.
  emulator.ExpectErrno(EPERM, FakeSyscall(__NR_setresuid, 0, 5, 5));
  emulator.ExpectErrno(EPERM, FakeSyscall(__NR_setresuid, 5, 0, 5));
  emulator.ExpectErrno(EPERM, FakeSyscall(__NR_setresuid, 5, 5, 0));

  // Expect EAGAIN if all set to 1.
  emulator.ExpectErrno(EAGAIN, FakeSyscall(__NR_setresuid, 1, 1, 1));

  // Expect EINVAL for anything else.
  emulator.ExpectErrno(EINVAL, FakeSyscall(__NR_setresuid, 5, 1, 1));
  emulator.ExpectErrno(EINVAL, FakeSyscall(__NR_setresuid, 1, 5, 1));
  emulator.ExpectErrno(EINVAL, FakeSyscall(__NR_setresuid, 1, 1, 5));
  emulator.ExpectErrno(EINVAL, FakeSyscall(__NR_setresuid, 3, 4, 5));
}

static const uintptr_t kDeadBeefAddr =
    static_cast<uintptr_t>(0xdeadbeefdeadbeefULL);

class ArgSizePolicy : public Policy {
 public:
  ArgSizePolicy() {}

  ArgSizePolicy(const ArgSizePolicy&) = delete;
  ArgSizePolicy& operator=(const ArgSizePolicy&) = delete;

  ~ArgSizePolicy() override {}
  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_uname) {
      const Arg<uintptr_t> addr(0);
      return If(addr == kDeadBeefAddr, Error(EPERM)).Else(Allow());
    }
    return Allow();
  }
};

TEST(BPFDSL, ArgSizeTest) {
  PolicyEmulator emulator(golden::kArgSizePolicy, ArgSizePolicy());

  emulator.ExpectAllow(FakeSyscall(__NR_uname, 0));
  emulator.ExpectErrno(EPERM, FakeSyscall(__NR_uname, kDeadBeefAddr));
}

class NegativeConstantsPolicy : public Policy {
 public:
  NegativeConstantsPolicy() {}

  NegativeConstantsPolicy(const NegativeConstantsPolicy&) = delete;
  NegativeConstantsPolicy& operator=(const NegativeConstantsPolicy&) = delete;

  ~NegativeConstantsPolicy() override {}
  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_fcntl) {
      const Arg<int> fd(0);
      return If(fd == -314, Error(EPERM)).Else(Allow());
    }
    return Allow();
  }
};

TEST(BPFDSL, NegativeConstantsTest) {
  PolicyEmulator emulator(golden::kNegativeConstantsPolicy,
                          NegativeConstantsPolicy());

  emulator.ExpectAllow(FakeSyscall(__NR_fcntl, -5, F_DUPFD));
  emulator.ExpectAllow(FakeSyscall(__NR_fcntl, 20, F_DUPFD));
  emulator.ExpectErrno(EPERM, FakeSyscall(__NR_fcntl, -314, F_DUPFD));
}

#if 0
// TODO(mdempsky): This is really an integration test.

class TrappingPolicy : public Policy {
 public:
  TrappingPolicy() {}

TrappingPolicy(const TrappingPolicy&) = delete;
TrappingPolicy& operator=(const TrappingPolicy&) = delete;

  ~TrappingPolicy() override {}
  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_uname) {
      return Trap(UnameTrap, &count_);
    }
    return Allow();
  }

 private:
  static intptr_t count_;

  static intptr_t UnameTrap(const struct arch_seccomp_data& data, void* aux) {
    BPF_ASSERT_EQ(&count_, aux);
    return ++count_;
  }
};

intptr_t TrappingPolicy::count_;

BPF_TEST_C(BPFDSL, TrapTest, TrappingPolicy) {
  ASSERT_SYSCALL_RESULT(1, uname, NULL);
  ASSERT_SYSCALL_RESULT(2, uname, NULL);
  ASSERT_SYSCALL_RESULT(3, uname, NULL);
}
#endif

class MaskingPolicy : public Policy {
 public:
  MaskingPolicy() {}

  MaskingPolicy(const MaskingPolicy&) = delete;
  MaskingPolicy& operator=(const MaskingPolicy&) = delete;

  ~MaskingPolicy() override {}
  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_setuid) {
      const Arg<uid_t> uid(0);
      return If((uid & 0xf) == 0, Error(EINVAL)).Else(Error(EACCES));
    }
    if (sysno == __NR_setgid) {
      const Arg<gid_t> gid(0);
      return If((gid & 0xf0) == 0xf0, Error(EINVAL)).Else(Error(EACCES));
    }
    if (sysno == __NR_setpgid) {
      const Arg<pid_t> pid(0);
      return If((pid & 0xa5) == 0xa0, Error(EINVAL)).Else(Error(EACCES));
    }
    return Allow();
  }
};

TEST(BPFDSL, MaskTest) {
  PolicyEmulator emulator(golden::kMaskingPolicy, MaskingPolicy());

  for (uid_t uid = 0; uid < 0x100; ++uid) {
    const int expect_errno = (uid & 0xf) == 0 ? EINVAL : EACCES;
    emulator.ExpectErrno(expect_errno, FakeSyscall(__NR_setuid, uid));
  }

  for (gid_t gid = 0; gid < 0x100; ++gid) {
    const int expect_errno = (gid & 0xf0) == 0xf0 ? EINVAL : EACCES;
    emulator.ExpectErrno(expect_errno, FakeSyscall(__NR_setgid, gid));
  }

  for (pid_t pid = 0; pid < 0x100; ++pid) {
    const int expect_errno = (pid & 0xa5) == 0xa0 ? EINVAL : EACCES;
    emulator.ExpectErrno(expect_errno, FakeSyscall(__NR_setpgid, pid, 0));
  }
}

class ElseIfPolicy : public Policy {
 public:
  ElseIfPolicy() {}

  ElseIfPolicy(const ElseIfPolicy&) = delete;
  ElseIfPolicy& operator=(const ElseIfPolicy&) = delete;

  ~ElseIfPolicy() override {}
  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_setuid) {
      const Arg<uid_t> uid(0);
      return If((uid & 0xfff) == 0, Error(0))
          .ElseIf((uid & 0xff0) == 0, Error(EINVAL))
          .ElseIf((uid & 0xf00) == 0, Error(EEXIST))
          .Else(Error(EACCES));
    }
    return Allow();
  }
};

TEST(BPFDSL, ElseIfTest) {
  PolicyEmulator emulator(golden::kElseIfPolicy, ElseIfPolicy());

  emulator.ExpectErrno(0, FakeSyscall(__NR_setuid, 0));

  emulator.ExpectErrno(EINVAL, FakeSyscall(__NR_setuid, 0x0001));
  emulator.ExpectErrno(EINVAL, FakeSyscall(__NR_setuid, 0x0002));

  emulator.ExpectErrno(EEXIST, FakeSyscall(__NR_setuid, 0x0011));
  emulator.ExpectErrno(EEXIST, FakeSyscall(__NR_setuid, 0x0022));

  emulator.ExpectErrno(EACCES, FakeSyscall(__NR_setuid, 0x0111));
  emulator.ExpectErrno(EACCES, FakeSyscall(__NR_setuid, 0x0222));
}

class SwitchPolicy : public Policy {
 public:
  SwitchPolicy() = default;

  SwitchPolicy(const SwitchPolicy&) = delete;
  SwitchPolicy& operator=(const SwitchPolicy&) = delete;

  ~SwitchPolicy() override = default;
  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_fcntl) {
      const Arg<int> cmd(1);
      const Arg<unsigned long> long_arg(2);
      return Switch(cmd)
          .Cases({F_GETFL, F_GETFD}, Error(ENOENT))
          .Case(F_SETFD, If(long_arg == O_CLOEXEC, Allow()).Else(Error(EINVAL)))
          .Case(F_SETFL, Error(EPERM))
          .Default(Error(EACCES));
    }
    return Allow();
  }
};

TEST(BPFDSL, SwitchTest) {
  PolicyEmulator emulator(golden::kSwitchPolicy, SwitchPolicy());

  const int kFakeSockFD = 42;

  emulator.ExpectErrno(ENOENT, FakeSyscall(__NR_fcntl, kFakeSockFD, F_GETFD));
  emulator.ExpectErrno(ENOENT, FakeSyscall(__NR_fcntl, kFakeSockFD, F_GETFL));

  emulator.ExpectAllow(
      FakeSyscall(__NR_fcntl, kFakeSockFD, F_SETFD, O_CLOEXEC));
  emulator.ExpectErrno(EINVAL,
                       FakeSyscall(__NR_fcntl, kFakeSockFD, F_SETFD, 0));

  emulator.ExpectErrno(EPERM,
                       FakeSyscall(__NR_fcntl, kFakeSockFD, F_SETFL, O_RDONLY));

  emulator.ExpectErrno(EACCES,
                       FakeSyscall(__NR_fcntl, kFakeSockFD, F_DUPFD, 0));
}

static intptr_t DummyTrap(const struct arch_seccomp_data& data, void* aux) {
  return 0;
}

TEST(BPFDSL, IsAllowDeny) {
  ResultExpr allow = Allow();
  EXPECT_TRUE(allow->IsAllow());
  EXPECT_FALSE(allow->IsDeny());

  ResultExpr error = Error(ENOENT);
  EXPECT_FALSE(error->IsAllow());
  EXPECT_TRUE(error->IsDeny());

  ResultExpr trace = Trace(42);
  EXPECT_FALSE(trace->IsAllow());
  EXPECT_FALSE(trace->IsDeny());

  ResultExpr trap = Trap(DummyTrap, nullptr);
  EXPECT_FALSE(trap->IsAllow());
  EXPECT_TRUE(trap->IsDeny());

  ResultExpr user_notify = UserNotify();
  EXPECT_FALSE(user_notify->IsAllow());
  EXPECT_TRUE(user_notify->IsDeny());

  const Arg<int> arg(0);
  ResultExpr maybe = If(arg == 0, Allow()).Else(Error(EPERM));
  EXPECT_FALSE(maybe->IsAllow());
  EXPECT_FALSE(maybe->IsDeny());
}

TEST(BPFDSL, HasUnsafeTraps) {
  ResultExpr allow = Allow();
  EXPECT_FALSE(allow->HasUnsafeTraps());

  ResultExpr safe = Trap(DummyTrap, nullptr);
  EXPECT_FALSE(safe->HasUnsafeTraps());

  ResultExpr unsafe = UnsafeTrap(DummyTrap, nullptr);
  EXPECT_TRUE(unsafe->HasUnsafeTraps());

  ResultExpr user_notify = UserNotify();
  EXPECT_FALSE(allow->HasUnsafeTraps());

  const Arg<int> arg(0);
  ResultExpr maybe = If(arg == 0, allow).Else(unsafe);
  EXPECT_TRUE(maybe->HasUnsafeTraps());
}

}  // namespace
}  // namespace bpf_dsl
}  // namespace sandbox
