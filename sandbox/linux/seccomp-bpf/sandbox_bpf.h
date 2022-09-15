// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H_
#define SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H_

#include <stdint.h>

#include <memory>

#include "base/files/scoped_file.h"
#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/sandbox_export.h"

struct arch_seccomp_data;

namespace sandbox {

// This class can be used to apply a syscall sandboxing policy expressed in a
// bpf_dsl::Policy object to the current process.
// Syscall sandboxing policies get inherited by subprocesses and, once applied,
// can never be removed for the lifetime of the process.
class SANDBOX_EXPORT SandboxBPF {
 public:
  enum class SeccompLevel {
    SINGLE_THREADED,
    MULTI_THREADED,
  };

  // Ownership of |policy| is transfered here to the sandbox object.
  // nullptr is allowed for unit tests.
  explicit SandboxBPF(std::unique_ptr<bpf_dsl::Policy> policy);

  SandboxBPF(const SandboxBPF&) = delete;
  SandboxBPF& operator=(const SandboxBPF&) = delete;

  // NOTE: Setting a policy and starting the sandbox is a one-way operation.
  // The kernel does not provide any option for unloading a loaded sandbox. The
  // sandbox remains engaged even when the object is destructed.
  ~SandboxBPF();

  // Detect if the kernel supports the specified seccomp level.
  // See StartSandbox() for a description of these.
  static bool SupportsSeccompSandbox(SeccompLevel level);

  // This is the main public entry point. It sets up the resources needed by
  // the sandbox, and enters Seccomp mode.
  // The calling process must provide a |level| to tell the sandbox which type
  // of kernel support it should engage.
  // SINGLE_THREADED will only sandbox the calling thread. Since it would be a
  // security risk, the sandbox will also check that the current process is
  // single threaded and crash if it isn't the case.
  // MULTI_THREADED requires more recent kernel support and allows to sandbox
  // all the threads of the current process. Be mindful of potential races,
  // with other threads using disallowed system calls either before or after
  // the sandbox is engaged.
  //
  // It is possible to stack multiple sandboxes by creating separate "Sandbox"
  // objects and calling "StartSandbox()" on each of them. Please note, that
  // this requires special care, though, as newly stacked sandboxes can never
  // relax restrictions imposed by earlier sandboxes. Furthermore, installing
  // a new policy requires making system calls, that might already be
  // disallowed.
  // Finally, stacking does add more kernel overhead than having a single
  // combined policy. So, it should only be used if there are no alternatives.
  //
  // |enable_ibpb| controls if the sandbox will forcibly enable indirect branch
  // prediction barrier through prctl(2) to mitigate Spectre variant 2.
  [[nodiscard]] bool StartSandbox(SeccompLevel level, bool enable_ibpb = true);

  // The sandbox needs to be able to access files in "/proc/self/". If
  // this directory is not accessible when "StartSandbox()" gets called, the
  // caller must provide an already opened file descriptor by calling
  // "SetProcFd()".
  // The sandbox becomes the new owner of this file descriptor and will
  // close it when "StartSandbox()" executes or when the sandbox object
  // disappears.
  void SetProcFd(base::ScopedFD proc_fd);

  // Checks whether a particular system call number is valid on the current
  // architecture.
  static bool IsValidSyscallNumber(int sysnum);

  // UnsafeTraps require some syscalls to always be allowed.
  // This helper function returns true for these calls.
  static bool IsRequiredForUnsafeTrap(int sysno);

  // From within an UnsafeTrap() it is often useful to be able to execute
  // the system call that triggered the trap. The ForwardSyscall() method
  // makes this easy. It is more efficient than calling glibc's syscall()
  // function, as it avoid the extra round-trip to the signal handler. And
  // it automatically does the correct thing to report kernel-style error
  // conditions, rather than setting errno. See the comments for TrapFnc for
  // details. In other words, the return value from ForwardSyscall() is
  // directly suitable as a return value for a trap handler.
  static intptr_t ForwardSyscall(const struct arch_seccomp_data& args);

 private:
  friend class SandboxBPFTestRunner;

  // Assembles a BPF filter program from the current policy. After calling this
  // function, you must not call any other sandboxing function.
  CodeGen::Program AssembleFilter();

  // Assembles and installs a filter based on the policy that has previously
  // been configured with SetSandboxPolicy().
  void InstallFilter(bool must_sync_threads, bool enable_ibpb);

  // Disable indirect branch speculation by prctl. This will be done by
  // seccomp if SECCOMP_FILTER_FLAG_SPEC_ALLOW is not set. Seccomp will
  // disable indirect branch speculation and speculative store bypass
  // simultaneously. We use prctl in supplement to control the speculation
  // features separately.
  void DisableIBSpec();

  base::ScopedFD proc_fd_;
  bool sandbox_has_started_;
  std::unique_ptr<bpf_dsl::Policy> policy_;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H_
