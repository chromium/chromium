// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_POLICY_H_
#define SANDBOX_LINUX_BPF_DSL_POLICY_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace bpf_dsl {

// Interface to implement to define a BPF sandbox policy.
class SANDBOX_EXPORT Policy {
 public:
  Policy() {}

  Policy(const Policy&) = delete;
  Policy& operator=(const Policy&) = delete;

  virtual ~Policy() {}

  // User extension point for writing custom sandbox policies.
  // The returned ResultExpr will control how the kernel responds to the
  // specified system call number.
  virtual ResultExpr EvaluateSyscall(int sysno) const = 0;

  // Optional overload for specifying alternate behavior for invalid
  // system calls.  The default is to return ENOSYS.
  virtual ResultExpr InvalidSyscall() const;
};

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_POLICY_H_
