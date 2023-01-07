// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_LINUX_BPF_PRINT_BACKEND_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_PRINT_BACKEND_POLICY_LINUX_H_

#include "sandbox/policy/linux/bpf_network_policy_linux.h"

namespace sandbox {
namespace policy {

// This policy can be used by print backend utility processes.
// It is based upon NetworkProcessPolicy because print backend talks to CUPS
// servers over network.
class PrintBackendProcessPolicy : public NetworkProcessPolicy {
 public:
  PrintBackendProcessPolicy();
  PrintBackendProcessPolicy(const PrintBackendProcessPolicy&) = delete;
  PrintBackendProcessPolicy& operator=(const PrintBackendProcessPolicy&) =
      delete;
  ~PrintBackendProcessPolicy() override;

  // Currently no need to override EvaluateSyscall() because network base class
  // already provides sufficient capabilities.
  // TODO(crbug.com/809738) Provide more specific policy allowances once
  // network receives refined restrictions.
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_LINUX_BPF_PRINT_BACKEND_POLICY_LINUX_H_
