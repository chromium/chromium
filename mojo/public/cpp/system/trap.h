// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_TRAP_H_
#define MOJO_PUBLIC_CPP_SYSTEM_TRAP_H_

#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// A strongly-typed representation of a |MojoHandle| for a trap.
class TrapHandle : public Handle {
 public:
  TrapHandle() = default;
  explicit TrapHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

static_assert(sizeof(TrapHandle) == sizeof(Handle),
              "Bad size for C++ TrapHandle");

typedef ScopedHandleBase<TrapHandle> ScopedTrapHandle;
static_assert(sizeof(ScopedTrapHandle) == sizeof(TrapHandle),
              "Bad size for C++ ScopedTrapHandle");

MOJO_CPP_SYSTEM_EXPORT MojoResult CreateTrap(MojoTrapEventHandler handler,
                                             ScopedTrapHandle* trap_handle);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_TRAP_H_
