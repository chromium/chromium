// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the C++ side of the cxx bindings defined in cxx.rs. See that file
// for documentation.

#ifndef MOJO_PUBLIC_RUST_SYSTEM_TEST_UTIL_CXX_SHIM_H_
#define MOJO_PUBLIC_RUST_SYSTEM_TEST_UTIL_CXX_SHIM_H_

#include "base/functional/bind.h"
#include "mojo/public/cpp/system/functions.h"
#include "mojo/public/rust/system/test_util/test_util.rs.h"

namespace rustmojo_system_api {

void SetDefaultProcessErrorHandler(
    rust::Box<RustRepeatingStringCallback> handler) {
  mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
      &RustRepeatingStringCallback::run, base::OwnedRef(std::move(handler))));
}

}  // namespace rustmojo_system_api

#endif  // MOJO_PUBLIC_RUST_SYSTEM_TEST_UTIL_CXX_SHIM_H_
