// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the C++ side of the cxx bindings defined in cxx.rs. See that file
// for documentation.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_CXX_SHIM_H_
#define MOJO_PUBLIC_RUST_BINDINGS_CXX_SHIM_H_

#include "base/time/time.h"

namespace rust_mojo_bindings_api_bridge {

int64_t CurrentTimeTicksInMicroseconds() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
}

}  // namespace rust_mojo_bindings_api_bridge

#endif  // MOJO_PUBLIC_RUST_BINDINGS_CXX_SHIM_H_
