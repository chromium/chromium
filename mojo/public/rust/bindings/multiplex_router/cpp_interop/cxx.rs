// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
  "//base:sequenced_task_runner";
  "//mojo/public/rust/system";
}

use super::cpp_router_handle::{cxx_disconnect_handler, cxx_incoming_handler};
use crate::multiplex_router::EndpointInfo;

#[cxx::bridge(namespace = "mojo::rust::bindings")]
pub mod ffi {
    #[namespace = "mojo::rust"]
    unsafe extern "C++" {
        include!("mojo/public/rust/system/scoped_handle_interop.h");
        type ScopedMessageHandleWrapper =
            super::system::scoped_handle_interop::ScopedMessageHandleWrapper;
    }

    #[namespace = "base"]
    unsafe extern "C++" {
        include!("base/task/sequenced_task_runner_rust_shim.h");
        type SequencedTaskRunner = super::sequenced_task_runner::ffi::SequencedTaskRunner;
    }

    extern "Rust" {
        type EndpointInfo;

        /// Called by C++ to invoke the user-provided Rust message handler
        fn cxx_incoming_handler(
            info: &EndpointInfo,
            wrapper: UniquePtr<ScopedMessageHandleWrapper>,
            responder: UniquePtr<MojoResponderWrapper>,
        ) -> bool;

        /// Called by C++ to invoke the user-provided Rust disconnect handler
        fn cxx_disconnect_handler(info: Box<EndpointInfo>);
    }

    unsafe extern "C++" {
        include!("mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h");
        type AssociatedEndpointRustAdapter;

        /// Creates a pair of entangled C++ pending associated endpoints.
        fn CreatePairPendingAssociation(
            self_out: &mut UniquePtr<AssociatedEndpointRustAdapter>,
            peer_out: &mut UniquePtr<AssociatedEndpointRustAdapter>,
        );

        /// Binds the endpoint to a sequence and starts routing incoming
        /// messages to Rust.
        fn Bind(
            self: Pin<&mut AssociatedEndpointRustAdapter>,
            runner: &SequencedTaskRunner,
            info: Box<EndpointInfo>,
        );

        /// Returns the associated interface ID assigned to this endpoint.
        fn GetInterfaceId(self: &AssociatedEndpointRustAdapter) -> u32;

        /// Closes `self` and notifies its peer that the interface is closed
        fn Close(self: Pin<&mut AssociatedEndpointRustAdapter>);

        /// Sends an outgoing Mojom IPC message through the C++ endpoint.
        fn SendMessage(
            self: &AssociatedEndpointRustAdapter,
            message_wrapper: UniquePtr<ScopedMessageHandleWrapper>,
        );

        /// Registers a nested associated interface endpoint with the C++ group
        /// controller.
        fn RegisterNewEndpoint(
            self: &AssociatedEndpointRustAdapter,
            interface_id: u32,
        ) -> UniquePtr<AssociatedEndpointRustAdapter>;
    }

    unsafe extern "C++" {
        include!("mojo/public/rust/bindings/multiplex_router/cpp_interop/mojo_responder_wrapper.h");
        type MojoResponderWrapper;

        /// Sends a reply message through the C++ responder object.
        /// The name is counterintuitive, but that's the C++ naming scheme.
        fn Accept(
            self: &MojoResponderWrapper,
            message_wrapper: UniquePtr<ScopedMessageHandleWrapper>,
        ) -> bool;

        /// Creates a copy of the responder that cannot send messages, but can
        /// still register new associated endpoints.
        fn CloneAsRegistrar(self: &MojoResponderWrapper) -> UniquePtr<MojoResponderWrapper>;

        /// Returns true if this responder can send messages.
        fn CanSendResponse(self: &MojoResponderWrapper) -> bool;

        /// Registers an associated endpoint with the responder's underlying
        /// router.
        fn RegisterNewEndpoint(
            self: &MojoResponderWrapper,
            interface_id: u32,
        ) -> UniquePtr<AssociatedEndpointRustAdapter>;
    }
}

// SAFETY: Neither of the fields of `AssociatedEndpointRustAdapter` care about
// which thread they're on.
unsafe impl Send for ffi::AssociatedEndpointRustAdapter {}
// SAFETY: All `&self` methods on `AssociatedEndpointRustAdapter`
// (`SendMessage`, `Close`, `RegisterNewEndpoint`) are thread-safe or
// sequence-bound. The only danger is that `Bind` must not be called
// concurrently with anything else, which is enforced by taking &mut.
unsafe impl Sync for ffi::AssociatedEndpointRustAdapter {}

// SAFETY: `MojoResponderWrapper` wraps a C++ `base::SequenceBound`, so
// all its methods are thread-safe by design.
unsafe impl Send for ffi::MojoResponderWrapper {}
// SAFETY: As Above
unsafe impl Sync for ffi::MojoResponderWrapper {}
