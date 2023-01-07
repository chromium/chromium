// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/trap.h"

#include "mojo/public/c/system/functions.h"

namespace mojo {

MojoResult CreateTrap(MojoTrapEventHandler handler,
                      ScopedTrapHandle* trap_handle) {
  MojoHandle handle;
  MojoResult rv = MojoCreateTrap(handler, nullptr, &handle);
  if (rv == MOJO_RESULT_OK)
    trap_handle->reset(TrapHandle(handle));
  return rv;
}

}  // namespace mojo
