// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_TRAP_REGISTRY_H_
#define SANDBOX_LINUX_BPF_DSL_TRAP_REGISTRY_H_

#include <stdint.h>

#include "base/macros.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

namespace bpf_dsl {

// TrapRegistry provides an interface for registering "trap handlers"
// by associating them with non-zero 16-bit trap IDs. Trap IDs should
// remain valid for the lifetime of the trap registry.
class SANDBOX_EXPORT TrapRegistry {
 public:
  // TrapFnc is a pointer to a function that fulfills the trap handler
  // function signature.
  //
  // Trap handlers follow the calling convention of native system
  // calls; e.g., to report an error, they return an exit code in the
  // range -1..-4096 instead of directly modifying errno. However,
  // modifying errno is harmless, as the original value will be
  // restored afterwards.
  //
  // Trap handlers are executed from signal context and possibly an
  // async-signal context, so they must be async-signal safe:
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html
  typedef intptr_t (*TrapFnc)(const struct arch_seccomp_data& args, void* aux);

  // Add registers the specified trap handler tuple and returns a
  // non-zero trap ID that uniquely identifies the tuple for the life
  // time of the trap registry. If the same tuple is registered
  // multiple times, the same value will be returned each time.
  virtual uint16_t Add(TrapFnc fnc, const void* aux, bool safe) = 0;

  // EnableUnsafeTraps tries to enable unsafe traps and returns
  // whether it was successful. This is a one-way operation.
  //
  // CAUTION: Enabling unsafe traps effectively defeats the security
  // guarantees provided by the sandbox policy. TrapRegistry
  // implementations should ensure unsafe traps are only enabled
  // during testing.
  virtual bool EnableUnsafeTraps() = 0;

 protected:
  TrapRegistry() {}

  // TrapRegistry's destructor is intentionally non-virtual so that
  // implementations can omit their destructor.  Instead we protect against
  // misuse by marking it protected.
  ~TrapRegistry() {}

  DISALLOW_COPY_AND_ASSIGN(TrapRegistry);
};

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_TRAP_REGISTRY_H_
