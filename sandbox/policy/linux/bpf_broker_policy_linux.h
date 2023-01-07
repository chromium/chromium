// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_BROKER_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_BROKER_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox {
namespace policy {

// A broker policy is one for a privileged syscall broker that allows
// a limited set of filesystem calls.
class SANDBOX_POLICY_EXPORT BrokerProcessPolicy : public BPFBasePolicy {
 public:
  explicit BrokerProcessPolicy(
      const syscall_broker::BrokerCommandSet& allowed_command_set);

  BrokerProcessPolicy(const BrokerProcessPolicy&) = delete;
  BrokerProcessPolicy& operator=(const BrokerProcessPolicy&) = delete;

  ~BrokerProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int system_call_number) const override;

 private:
  const syscall_broker::BrokerCommandSet allowed_command_set_;
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_BROKER_POLICY_LINUX_H_
