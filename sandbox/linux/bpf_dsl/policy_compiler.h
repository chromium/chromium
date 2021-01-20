// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_POLICY_COMPILER_H_
#define SANDBOX_LINUX_BPF_DSL_POLICY_COMPILER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace bpf_dsl {
class Policy;

// PolicyCompiler implements the bpf_dsl compiler, allowing users to
// transform bpf_dsl policies into BPF programs to be executed by the
// Linux kernel.
class SANDBOX_EXPORT PolicyCompiler {
 public:
  using PanicFunc = bpf_dsl::ResultExpr (*)(const char* error);

  PolicyCompiler(const Policy* policy, TrapRegistry* registry);
  ~PolicyCompiler();

  // Compile registers any trap handlers needed by the policy and
  // compiles the policy to a BPF program, which it returns.
  CodeGen::Program Compile();

  // DangerousSetEscapePC sets the "escape PC" that is allowed to issue any
  // system calls, regardless of policy.
  void DangerousSetEscapePC(uint64_t escapepc);

  // SetPanicFunc sets the callback function used for handling faulty
  // system call conditions.  The default behavior is to immediately kill
  // the process.
  // TODO(mdempsky): Move this into Policy?
  void SetPanicFunc(PanicFunc panic_func);

  // UnsafeTraps require some syscalls to always be allowed.
  // This helper function returns true for these calls.
  static bool IsRequiredForUnsafeTrap(int sysno);

  // Functions below are meant for use within bpf_dsl itself.

  // Return returns a CodeGen::Node that returns the specified seccomp
  // return value.
  CodeGen::Node Return(uint32_t ret);

  // Trap returns a CodeGen::Node to indicate the system call should
  // instead invoke a trap handler.
  CodeGen::Node Trap(TrapRegistry::TrapFnc fnc, const void* aux, bool safe);

  // MaskedEqual returns a CodeGen::Node that represents a conditional branch.
  // Argument "argno" (1..6) will be bitwise-AND'd with "mask" and compared
  // to "value"; if equal, then "passed" will be executed, otherwise "failed".
  // If "width" is 4, the argument must in the range of 0x0..(1u << 32 - 1)
  // If it is outside this range, the sandbox treats the system call just
  // the same as any other ABI violation (i.e., it panics).
  CodeGen::Node MaskedEqual(int argno,
                            size_t width,
                            uint64_t mask,
                            uint64_t value,
                            CodeGen::Node passed,
                            CodeGen::Node failed);

 private:
  struct Range;
  typedef std::vector<Range> Ranges;

  // Used by MaskedEqualHalf to track which half of the argument it's
  // emitting instructions for.
  enum class ArgHalf {
    LOWER,
    UPPER,
  };

  // Compile the configured policy into a complete instruction sequence.
  CodeGen::Node AssemblePolicy();

  // Return an instruction sequence that checks the
  // arch_seccomp_data's "arch" field is valid, and then passes
  // control to |passed| if so.
  CodeGen::Node CheckArch(CodeGen::Node passed);

  // If |has_unsafe_traps_| is true, returns an instruction sequence
  // that allows all system calls from |escapepc_|, and otherwise
  // passes control to |rest|. Otherwise, simply returns |rest|.
  CodeGen::Node MaybeAddEscapeHatch(CodeGen::Node rest);

  // Return an instruction sequence that loads and checks the system
  // call number, performs a binary search, and then dispatches to an
  // appropriate instruction sequence compiled from the current
  // policy.
  CodeGen::Node DispatchSyscall();

  // Return an instruction sequence that checks the system call number
  // (expected to be loaded in register A) and if valid, passes
  // control to |passed| (with register A still valid).
  CodeGen::Node CheckSyscallNumber(CodeGen::Node passed);

  // Finds all the ranges of system calls that need to be handled. Ranges are
  // sorted in ascending order of system call numbers. There are no gaps in the
  // ranges. System calls with identical CodeGen::Nodes are coalesced into a
  // single
  // range.
  void FindRanges(Ranges* ranges);

  // Returns a BPF program snippet that implements a jump table for the
  // given range of system call numbers. This function runs recursively.
  CodeGen::Node AssembleJumpTable(Ranges::const_iterator start,
                                  Ranges::const_iterator stop);

  // CompileResult compiles an individual result expression into a
  // CodeGen node.
  CodeGen::Node CompileResult(const ResultExpr& res);

  // Returns a BPF program that evaluates half of a conditional expression;
  // it should only ever be called from CondExpression().
  CodeGen::Node MaskedEqualHalf(int argno,
                                size_t width,
                                uint64_t full_mask,
                                uint64_t full_value,
                                ArgHalf half,
                                CodeGen::Node passed,
                                CodeGen::Node failed);

  // Returns the fatal CodeGen::Node that is used to indicate that somebody
  // attempted to pass a 64bit value in a 32bit system call argument.
  CodeGen::Node Unexpected64bitArgument();

  const Policy* policy_;
  TrapRegistry* registry_;
  uint64_t escapepc_;
  PanicFunc panic_func_;

  CodeGen gen_;
  bool has_unsafe_traps_;

  DISALLOW_COPY_AND_ASSIGN(PolicyCompiler);
};

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_POLICY_COMPILER_H_
