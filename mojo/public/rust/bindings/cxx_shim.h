// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the C++ side of the cxx bindings defined in cxx.rs. See that file
// for documentation.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_CXX_SHIM_H_
#define MOJO_PUBLIC_RUST_BINDINGS_CXX_SHIM_H_

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace rust_mojo_bindings_api_bridge {

inline int64_t CurrentTimeTicksInMicroseconds() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
}

inline bool ValidateMessageHeader(::rust::Slice<const uint8_t> data) {
  // SAFETY: `rust::Slice` guarantees that `data()` points to `size()` readable
  // bytes.
  auto bytes = UNSAFE_BUFFERS(base::span(data.data(), data.size()));
  // This won't report a failure, just return false.
  return mojo::internal::ValidateMessageHeader(bytes);
}

}  // namespace rust_mojo_bindings_api_bridge

#endif  // MOJO_PUBLIC_RUST_BINDINGS_CXX_SHIM_H_
