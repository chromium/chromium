// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_CXX_SHIM_H_
#define MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_CXX_SHIM_H_

#include "mojo/public/c/system/types.h"
#include "mojo/public/rust/bindings/test/cpp/add_seven_service.h"
#include "mojo/public/rust/system/scoped_handle_interop.h"

namespace bindings_unittests::mojom {

std::unique_ptr<PlusSevenMathService> CreatePlusSevenMathService(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> handle);
void TestRemoteFromCpp(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> handle);

// Creates a PlusSevenMathService, binds it to a new pipe, and returns both
// the service (to keep it alive) and the remote end (wrapped for Rust)
// via out-parameters.
void CreatePlusSevenMathServiceAndRemote(
    std::unique_ptr<PlusSevenMathService>& service_out,
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper>& remote_out);

}  // namespace bindings_unittests::mojom

#endif  // MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_CXX_SHIM_H_
