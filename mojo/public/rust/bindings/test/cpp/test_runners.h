// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_TEST_RUNNERS_H_
#define MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_TEST_RUNNERS_H_

#include <memory>

#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h"
#include "mojo/public/rust/system/scoped_handle_interop.h"

// This file is for functions which contain testing logic;
// that is, they run tests themselves, and are called from tests.rs.
//
// This is needed because we need to test interop, which means ensuring
// that C++ can call Rusty things when necessary.

namespace bindings_unittests::mojom {

// A thin wrapper around a Rust `Remote` so we can use it from C++.
struct RustAssociatedSender;

class AssociatedSenderTestRemote;

// Creates a MathService remote from the given handle, and verifies
// that it works by calling `Add`.
void TestRemoteFromCpp(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper);

// Tests sending an associated receiver to Rust across a C++ primary remote,
// and verifies by calling `Add(500, 300)`.
void TestSendReceiverAndAddCppRemote(AssociatedSenderTestRemote& remote);

// Tests requesting an associated remote from a Rust primary remote, and
// verifying by calling `Add(100, 200)`.
void TestRequestRemoteAndAddRustRemote(RustAssociatedSender& sender);

// Tests sending an associated HandleService receiver to Rust across a C++
// primary remote, and verifies by calling `PassHandles`.
void TestSendHandleReceiverAndPassHandlesCppRemote(
    AssociatedSenderTestRemote& remote);

// Tests requesting an associated HandleService remote from a Rust primary
// remote, and verifying by calling `PassHandles`.
void TestRequestHandleRemoteAndPassHandlesRustRemote(
    RustAssociatedSender& sender);

// Tests sending a malformed message from a C++ associated remote across a C++
// primary pipe to a Rust associated receiver.
void TestBadMessageToRustReceiver();

// Returns true if both adapters are associated with the same
// `AssociatedGroupController`. Consumes both adapters, closing their endpoints.
bool HaveSameGroupController(
    mojo::rust::bindings::CxxPendingAssociatedEndpoint first,
    mojo::rust::bindings::CxxPendingAssociatedEndpoint second);

}  // namespace bindings_unittests::mojom

#endif  // MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_TEST_RUNNERS_H_
