// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_BASE_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_BASE_POLICY_LINUX_H_

#include <memory>

#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/policy/export.h"

namespace sandbox {
namespace policy {

// The "baseline" BPF policy. Any other seccomp-bpf policy should inherit
// from it.
// It implements the main Policy interface. Due to its nature
// as a "kernel attack surface reduction" layer, it's implementation-defined.
class SANDBOX_POLICY_EXPORT BPFBasePolicy : public bpf_dsl::Policy {
 public:
  BPFBasePolicy();

  BPFBasePolicy(const BPFBasePolicy&) = delete;
  BPFBasePolicy& operator=(const BPFBasePolicy&) = delete;

  ~BPFBasePolicy() override;

  // bpf_dsl::Policy:
  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;
  bpf_dsl::ResultExpr InvalidSyscall() const override;

  // Get the errno(3) to return for filesystem errors.
  static int GetFSDeniedErrno();

  pid_t GetPolicyPid() const { return baseline_policy_->policy_pid(); }

 private:
  // Compose the BaselinePolicy from sandbox/.
  std::unique_ptr<BaselinePolicy> baseline_policy_;
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_BASE_POLICY_LINUX_H_
