// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__
#define SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/macros.h"
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
  uint16_t Add(TrapFnc fnc, const void* aux, bool safe) override;

  bool EnableUnsafeTraps() override;

  // Registry returns the trap registry used by Trap's SIGSYS handler,
  // creating it if necessary.
  static bpf_dsl::TrapRegistry* Registry();

  // SandboxDebuggingAllowedByUser returns whether the
  // "CHROME_SANDBOX_DEBUGGING" environment variable is set.
  static bool SandboxDebuggingAllowedByUser();

 private:
  struct TrapKey {
    TrapKey() : fnc(nullptr), aux(nullptr), safe(false) {}
    TrapKey(TrapFnc f, const void* a, bool s) : fnc(f), aux(a), safe(s) {}
    TrapFnc fnc;
    const void* aux;
    bool safe;
    bool operator<(const TrapKey&) const;
  };
  typedef std::map<TrapKey, uint16_t> TrapIds;

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

  TrapIds trap_ids_;            // Maps from TrapKeys to numeric ids
  TrapKey* trap_array_;         // Array of TrapKeys indexed by ids
  size_t trap_array_size_;      // Currently used size of array
  size_t trap_array_capacity_;  // Currently allocated capacity of array
  bool has_unsafe_traps_;       // Whether unsafe traps have been enabled

  // Copying and assigning is unimplemented. It doesn't make sense for a
  // singleton.
  DISALLOW_COPY_AND_ASSIGN(Trap);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__
