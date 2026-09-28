// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_ASSOCIATED_SERVICES_H_
#define MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_ASSOCIATED_SERVICES_H_

#include <memory>

#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h"
#include "mojo/public/rust/bindings/test/cpp/add_seven_service.h"
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

// Binds an associated receiver from Rust to a PlusSevenMathService, C++ keeps
// ownership.
void BindPlusSevenAssociatedReceiver(
    mojo::rust::bindings::CxxPendingAssociatedEndpoint adapter);

// Binds an associated receiver from Rust to a CppHandleServiceImpl, C++ keeps
// ownership.
void BindCppHandleServiceReceiver(
    mojo::rust::bindings::CxxPendingAssociatedEndpoint adapter);

// Binds an associated receiver from Rust to an AssociatedSenderImpl, C++ keeps
// ownership.
void BindPlusSevenAssociatedSender(
    mojo::rust::bindings::CxxPendingAssociatedEndpoint adapter);

// Binds an associated receiver from Rust to a new PlusSevenMathService and
// returns ownership.
std::unique_ptr<PlusSevenMathService> CreatePlusSevenAssociatedReceiver(
    mojo::rust::bindings::CxxPendingAssociatedEndpoint adapter);

// Sets a disconnect handler on a PlusSevenMathService that notifies Rust.
void SetPlusSevenDisconnectCallback(PlusSevenMathService& service,
                                    int32_t handler_type);

}  // namespace bindings_unittests::mojom

#endif  // MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_ASSOCIATED_SERVICES_H_
