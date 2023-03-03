// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_PRINT_BACKEND_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_PRINT_BACKEND_POLICY_LINUX_H_

#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"

namespace sandbox::policy {

// This policy can be used by print backend utility processes.
// It is based upon NetworkProcessPolicy because print backend talks to CUPS
// servers over network.
class SANDBOX_POLICY_EXPORT PrintBackendProcessPolicy : public BPFBasePolicy {
 public:
  PrintBackendProcessPolicy();
  PrintBackendProcessPolicy(const PrintBackendProcessPolicy&) = delete;
  PrintBackendProcessPolicy& operator=(const PrintBackendProcessPolicy&) =
      delete;
  ~PrintBackendProcessPolicy() override;

  bpf_dsl::ResultExpr EvaluateSyscall(int sysno) const override;
};

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_LINUX_BPF_PRINT_BACKEND_POLICY_LINUX_H_
