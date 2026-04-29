// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Regression test for: BoolExpr-in-C++-ternary always-true bug in
// SpeechRecognitionProcessPolicy. The C++ conditional operator at
// bpf_speech_recognition_policy_linux.cc:38-40 used a bpf_dsl::BoolExpr
// (== std::shared_ptr<const internal::BoolExprImpl>, which is never null)
// as the ternary condition. shared_ptr's contextual conversion to bool is
// therefore always true, so EvaluateSyscall(__NR_mmap) returned Allow()
// unconditionally and the compiled BPF filter for mmap was
// SECCOMP_RET_ALLOW with no inspection of arg[3]. This bypassed
// RestrictMmapFlags() and let a compromised speech-recognition utility
// process call mmap() with normally-forbidden flags such as MAP_GROWSDOWN
// and MAP_HUGETLB.
//
// See crbug.com/502023400 for details.

#include "sandbox/policy/linux/bpf_speech_recognition_policy_linux.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(__NR_mmap)

#ifndef MAP_GROWSDOWN
#define MAP_GROWSDOWN 0x0100
#endif
#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000
#endif

namespace sandbox::policy {
namespace {

using bpf_dsl::Allow;
using bpf_dsl::ResultExpr;

// -----------------------------------------------------------------------------
// Part 1 — Static analysis of the production policy AST.
//
// SpeechRecognitionProcessPolicy::EvaluateSyscall(__NR_mmap) builds and
// returns a ResultExpr at C++ compile/run time *before* the BPF program is
// emitted. If the policy were correctly written with bpf_dsl::If().Else(),
// the returned node would be an IfThenResultExprImpl whose IsAllow() is
// false.
// -----------------------------------------------------------------------------
TEST(SpeechRecognitionPolicyMmapBypass, PolicyReturnsConditionalForMmap) {
  SpeechRecognitionProcessPolicy speech_policy;

  ResultExpr mmap_expr = speech_policy.EvaluateSyscall(__NR_mmap);
  ASSERT_TRUE(mmap_expr);

  // ********** FIXED **********
  // IsAllow() is only true for the Allow() leaf node; an If/Else conditional
  // node returns false. The fact that this is false proves that the BPF
  // program for __NR_mmap is no longer an unconditional Allow().
  EXPECT_FALSE(mmap_expr->IsAllow());

  // Control 1: the else-branch the author *intended* to fall through to.
  // BPFBasePolicy::EvaluateSyscall(__NR_mmap) chains to RestrictMmapFlags(),
  // which is an If/Else node — NOT an unconditional Allow.
  BPFBasePolicy base_policy;
  ResultExpr base_expr = base_policy.EvaluateSyscall(__NR_mmap);
  ASSERT_TRUE(base_expr);
  EXPECT_FALSE(base_expr->IsAllow());

  // Control 2: same file, correctly-written prctl handler uses If().Else()
  // and is NOT an unconditional Allow.
  ResultExpr prctl_expr = speech_policy.EvaluateSyscall(__NR_prctl);
  ASSERT_TRUE(prctl_expr);
  EXPECT_FALSE(prctl_expr->IsAllow());
}

// -----------------------------------------------------------------------------
// Part 2 — Live kernel verification under the production policy expression.
//
// We cannot apply the full SpeechRecognitionProcessPolicy in a unit test
// because its `default:` branch reaches into SandboxLinux::GetInstance()
// (broker not initialised here). Instead we wrap a trivial Policy that
// delegates *only* __NR_mmap to the real production policy and allows
// everything else. This means the seccomp program installed in the forked
// test process for __NR_mmap is *byte-identical* to the one Chrome installs
// in the speech-recognition utility process.
// -----------------------------------------------------------------------------
class SpeechRecognitionMmapPolicyWrapper : public bpf_dsl::Policy {
 public:
  SpeechRecognitionMmapPolicyWrapper() = default;
  ~SpeechRecognitionMmapPolicyWrapper() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_mmap) {
      // Exercise the production code path. This is the same ResultExpr that
      // ships in the kSpeechRecognition seccomp filter.
      return real_policy_.EvaluateSyscall(sysno);
    }
    return Allow();
  }

 private:
  SpeechRecognitionProcessPolicy real_policy_;
};

class BaselineMmapPolicyWrapper : public bpf_dsl::Policy {
 public:
  BaselineMmapPolicyWrapper() = default;
  ~BaselineMmapPolicyWrapper() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    if (sysno == __NR_mmap) {
      // The else-branch the author intended.
      return RestrictMmapFlags();
    }
    return Allow();
  }
};

// REGRESSION TEST: under the fixed speech-recognition policy
// expression, mmap with MAP_GROWSDOWN and MAP_HUGETLB is now correctly blocked.
BPF_DEATH_TEST_C(SpeechRecognitionPolicyMmapBypass,
                 ForbiddenGrowsdownBlocked,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 SpeechRecognitionMmapPolicyWrapper) {
  mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
       MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);
}

BPF_DEATH_TEST_C(SpeechRecognitionPolicyMmapBypass,
                 ForbiddenHugetlbBlocked,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 SpeechRecognitionMmapPolicyWrapper) {
  mmap(nullptr, 2 * 1024 * 1024, PROT_READ | PROT_WRITE,
       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
}

BPF_DEATH_TEST_C(SpeechRecognitionPolicyMmapBypass,
                 ForbiddenGrowsdownWithPopulateBlocked,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 SpeechRecognitionMmapPolicyWrapper) {
  // MAP_POPULATE | MAP_GROWSDOWN: even with the required MAP_POPULATE flag,
  // forbidden flags like MAP_GROWSDOWN must still be blocked.
  mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
       MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_GROWSDOWN, -1, 0);
}

// Ensure that normal mmap with MAP_POPULATE still works (this was the goal
// of the original code).
BPF_TEST_C(SpeechRecognitionPolicyMmapBypass,
           MapPopulateAllowed,
           SpeechRecognitionMmapPolicyWrapper) {
  errno = 0;
  void* p = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  BPF_ASSERT_NE(MAP_FAILED, p);
  munmap(p, 4096);
}

// CONTROL: under the *intended* baseline restriction, the very same
// MAP_GROWSDOWN call is caught by seccomp and the process dies with the
// canonical "**CRASHING**:seccomp-bpf failure in syscall" SIGSYS message.
BPF_DEATH_TEST_C(SpeechRecognitionPolicyMmapBypass,
                 BaselineBlocksGrowsdown,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselineMmapPolicyWrapper) {
  mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
       MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);
  // Not reached — CrashSIGSYS() runs first.
}

}  // namespace
}  // namespace sandbox::policy

#endif  // defined(__NR_mmap)
