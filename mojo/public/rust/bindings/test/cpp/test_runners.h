// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_TEST_RUNNERS_H_
#define MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_TEST_RUNNERS_H_

#include <memory>

#include "mojo/public/rust/system/scoped_handle_interop.h"

// This file is for functions which contain testing logic;
// that is, they run tests themselves, and are called from tests.rs.
//
// This is needed because we need to test interop, which means ensuring
// that C++ can call Rusty things when necessary.

namespace bindings_unittests::mojom {

// Creates a MathService remote from the given handle, and verifies
// that it works by calling `Add`.
void TestRemoteFromCpp(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper);

}  // namespace bindings_unittests::mojom

#endif  // MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_TEST_RUNNERS_H_
