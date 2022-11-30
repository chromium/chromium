// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_HELPERS_BASELINE_POLICY_H_
#define SANDBOX_LINUX_SECCOMP_BPF_HELPERS_BASELINE_POLICY_H_

#include <sys/types.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// This is a helper to build seccomp-bpf policies, i.e. policies for a sandbox
// that reduces the Linux kernel's attack surface. Given its nature, it doesn't
// have a clear semantics and is mostly "implementation-defined".
//
// This class implements the Policy interface with a "baseline"
// policy for use within Chromium.
// The "baseline" policy is somewhat arbitrary. All Chromium policies are an
// alteration of it, and it represents a reasonable common ground to run most
// code in a sandboxed environment.
// A baseline policy is only valid for the process for which this object was
// instantiated (so do not fork() and use it in a child).
class SANDBOX_EXPORT BaselinePolicy : public bpf_dsl::Policy {
 public:
  BaselinePolicy();
  // |fs_denied_errno| is the errno returned when a filesystem access system
  // call is denied.
  explicit BaselinePolicy(int fs_denied_errno);

  BaselinePolicy(const BaselinePolicy&) = delete;
  BaselinePolicy& operator=(const BaselinePolicy&) = delete;

  ~BaselinePolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
  bpf_dsl::ResultExpr InvalidSyscall() const override;
  pid_t policy_pid() const { return policy_pid_; }

 private:
  int fs_denied_errno_;

  // The PID that the policy applies to (should be equal to the current pid).
  pid_t policy_pid_;
};

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SECCOMP_BPF_HELPERS_BASELINE_POLICY_H_
