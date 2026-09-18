// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_ASSOCIATED_SERVICES_H_
#define MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_ASSOCIATED_SERVICES_H_

#include <memory>

#include "mojo/public/rust/system/scoped_handle_interop.h"

namespace bindings_unittests::mojom {

// Binds handle to an AssociatedSenderImpl, which handles requests to send or
// create associated MathService endpoints backed by PlusSevenMathService.
void CreateCppAssociatedSender(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper);

// Binds handle to an AssociatedSenderInteropTestImpl, which forwards received
// associated endpoints back to Rust via BindRustMathServiceReceiver.
void CreateAssociatedSenderInteropTest(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper);

}  // namespace bindings_unittests::mojom

#endif  // MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_ASSOCIATED_SERVICES_H_
