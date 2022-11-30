// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_SPEECH_RECOGNITION_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_SPEECH_RECOGNITION_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox {
namespace policy {

// The process policy for the sandboxed utility process that loads the Speech
// On-Device API (SODA). This policy allows the syscalls used by the libsoda.so
// binary to transcribe audio into text.
class SANDBOX_POLICY_EXPORT SpeechRecognitionProcessPolicy
    : public BPFBasePolicy {
 public:
  SpeechRecognitionProcessPolicy();

  SpeechRecognitionProcessPolicy(const SpeechRecognitionProcessPolicy&) =
      delete;
  SpeechRecognitionProcessPolicy& operator=(
      const SpeechRecognitionProcessPolicy&) = delete;

  ~SpeechRecognitionProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_SPEECH_RECOGNITION_POLICY_LINUX_H_
