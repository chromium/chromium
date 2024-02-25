// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__
#define SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/memory/raw_ptr_exclusion.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/linux/system_headers/linux_signal.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// The Trap class allows a BPF filter program to branch out to user space by
// raising a SIGSYS signal.
// N.B.: This class does not perform any synchronization operations. If
//   modifications are made to any of the traps, it is the caller's
//   responsibility to ensure that this happens in a thread-safe fashion.
//   Preferably, that means that no other threads should be running at that
//   time. For the purposes of our sandbox, this assertion should always be
//   true. Threads are incompatible with the seccomp sandbox anyway.
class SANDBOX_EXPORT Trap : public bpf_dsl::TrapRegistry {
 public:
  // Copying and assigning is unimplemented. It doesn't make sense for a
  // singleton.
  Trap(const Trap&) = delete;
  Trap& operator=(const Trap&) = delete;

  uint16_t Add(const Handler& handler) override;
  bool EnableUnsafeTraps() override;

  // Registry returns the trap registry used by Trap's SIGSYS handler,
  // creating it if necessary.
  static bpf_dsl::TrapRegistry* Registry();

  // SandboxDebuggingAllowedByUser returns whether the
  // "CHROME_SANDBOX_DEBUGGING" environment variable is set.
  static bool SandboxDebuggingAllowedByUser();

 private:
  using HandlerToIdMap = std::map<TrapRegistry::Handler, uint16_t>;

  // Our constructor is private. A shared global instance is created
  // automatically as needed.
  Trap();

  // The destructor is unimplemented as destroying this object would
  // break subsequent system calls that trigger a SIGSYS.
  ~Trap() = delete;

  static void SigSysAction(int nr, LinuxSigInfo* info, void* void_context);

  // Make sure that SigSys is not inlined in order to get slightly better crash
  // dumps.
  void SigSys(int nr, LinuxSigInfo* info, ucontext_t* ctx)
      __attribute__((noinline));

  // We have a global singleton that handles all of our SIGSYS traps. This
  // variable must never be deallocated after it has been set up initially, as
  // there is no way to reset in-kernel BPF filters that generate SIGSYS
  // events.
  static Trap* global_trap_;

  HandlerToIdMap trap_ids_;  // Maps from Handlers to numeric ids

  // Array of handlers indexed by ids.
  //
  // RAW_PTR_EXCLUSION: An owning pointer, and needs to be safe for signal
  // handlers.
  RAW_PTR_EXCLUSION TrapRegistry::Handler* trap_array_ = nullptr;
  size_t trap_array_size_ = 0;      // Currently used size of array
  size_t trap_array_capacity_ = 0;  // Currently allocated capacity of array
  bool has_unsafe_traps_ = false;   // Whether unsafe traps have been enabled
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__
