// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_speech_recognition_policy_linux.h"

#include <sys/mman.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerProcess;

namespace sandbox {
namespace policy {

SpeechRecognitionProcessPolicy::SpeechRecognitionProcessPolicy() = default;
SpeechRecognitionProcessPolicy::~SpeechRecognitionProcessPolicy() = default;

ResultExpr SpeechRecognitionProcessPolicy::EvaluateSyscall(
    int system_call_number) const {
  switch (system_call_number) {
#if defined(__NR_mmap)
    case __NR_mmap: {
      // The speech recognition sandbox requires the MAP_POPULATE flag in
      // addition to the default flags.
      const uint64_t kAllowedMask = MAP_POPULATE;
      const bpf_dsl::Arg<int> flags(3);
      return (flags & ~kAllowedMask) == 0
                 ? Allow()
                 : BPFBasePolicy::EvaluateSyscall(system_call_number);
    }
#endif
    // Required by the Speech On-Device API (SODA) binary to find the
    // appropriate configuration file to use within a language pack directory.
#if defined(__NR_getdents64)
    case __NR_getdents64:
      return Allow();
#endif
#if defined(__NR_getdents)
    case __NR_getdents:
      return Allow();
#endif
    default:
      auto* sandbox_linux = SandboxLinux::GetInstance();
      if (sandbox_linux->ShouldBrokerHandleSyscall(system_call_number))
        return sandbox_linux->HandleViaBroker(system_call_number);

      // Default on the content baseline policy.
      return BPFBasePolicy::EvaluateSyscall(system_call_number);
  }
}

}  // namespace policy
}  // namespace sandbox
