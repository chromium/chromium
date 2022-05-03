// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The old Mojo SDK had a different API than Chromium's Mojo right now. The Rust
// bindings refer to several functions that no longer exist in the core C API.
// However, most of the functionality exists, albeit in a different form, in the
// C++ bindings. This file re-implements the old C API functions in terms of the
// new C++ helpers to ease the transition. In the long term, the Rust bindings
// must be updated properly for the changes to Mojo.

#ifndef MOJO_PUBLIC_RUST_SUPPORT_H_
#define MOJO_PUBLIC_RUST_SUPPORT_H_

#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/types.h"

typedef uint32_t MojoCreateWaitSetFlags;
typedef uint32_t MojoWaitSetAddFlags;
typedef uintptr_t MojoWaitSetHandle;

struct MOJO_ALIGNAS(8) MojoCreateWaitSetOptions {
  uint32_t struct_size;
  MojoCreateWaitSetFlags flags;
};

struct MOJO_ALIGNAS(8) MojoWaitSetAddOptions {
  uint32_t struct_size;
  MojoWaitSetAddFlags flags;
};

struct MOJO_ALIGNAS(8) MojoWaitSetResult {
  uint64_t cookie;
  MojoResult wait_result;
  uint32_t reserved;
  struct MojoHandleSignalsState signals_state;
};

#ifdef __cplusplus
extern "C" {
#endif

// Similar to the above, wait sets could wait for one of several handles to be
// signalled. Unlike WaitMany they maintained a set of handles in their state.
// They are long gone as first-class objects of the Mojo API. Previously they
// were owned through MojoHandle just like pipes and buffers. Here this is
// changed to support casting between wait set handles and pointers.
//
// Reimplementing these was a bit more complex since the new C++ API is similar
// but different in many respects. For example, the old API referred to handles
// in the set by 64-bit integer handles. The C++ WaitSet does not, so we need to
// maintain maps in both directions.

MojoResult MojoCreateWaitSet(const struct MojoCreateWaitSetOptions*,
                             MojoWaitSetHandle* wait_set_handle);

MojoResult MojoWaitSetAdd(MojoWaitSetHandle wait_set_handle,
                          MojoHandle handle,
                          MojoHandleSignals signals,
                          uint64_t cookie,
                          const struct MojoWaitSetAddOptions*);

MojoResult MojoWaitSetRemove(MojoWaitSetHandle wait_set_handle,
                             uint64_t cookie);

MojoResult MojoWaitSetWait(MojoWaitSetHandle wait_set_handle,
                           uint32_t* num_results,
                           struct MojoWaitSetResult* results);

#ifdef __cplusplus
}
#endif

#endif  // MOJO_PUBLIC_RUST_SUPPORT_H_
