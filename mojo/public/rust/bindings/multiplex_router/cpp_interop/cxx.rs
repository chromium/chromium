// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
  "//base:scoped_refptr";
  "//base:sequenced_task_runner";
  "//mojo/public/rust/system";
}

use scoped_refptr::{CxxRefCounted, CxxRefCountedThreadSafe};

use super::cpp_router_handle::{run_rust_disconnect_handler, run_rust_incoming_handler};
use super::cxx_shim::{allocate_interface_id, register_interface_id};
use crate::multiplex_router::multiplex_router::MultiplexRouter;
use crate::multiplex_router::EndpointInfo;

#[cxx::bridge(namespace = "mojo::rust::bindings")]
pub mod ffi {
    #[namespace = "mojo"]
    unsafe extern "C++" {
        include!("mojo/public/cpp/bindings/message.h");
        type Message;
    }

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
        type MultiplexRouter;

        /// Notifies the Rust MultiplexRouter that an endpoint has been dropped.
        fn notify_dropped(self: &MultiplexRouter, interface_id: u32);

        /// Notifies the Rust MultiplexRouter that the peer of an endpoint was
        /// closed.
        fn notify_peer_closed(self: &MultiplexRouter, interface_id: u32);

        /// Allocates a new interface ID on the Rust router.
        fn allocate_interface_id(router: &MultiplexRouter) -> u32;

        /// Registers a received interface ID on the Rust router before it is
        /// bound. Returns true on success, or false if the ID was already
        /// registered.
        fn register_interface_id(router: &MultiplexRouter, interface_id: u32) -> bool;

        /// Called by C++ when an incoming message arrives for a Rust-bound
        /// associated endpoint. Returns true if the message was successfully
        /// dispatched to the Rust handler, or false on deserialization failure.
        ///
        /// # Safety
        /// `handles` must contain only live, unowned raw handle values.
        /// Ownership of those handles is transferred to Rust.
        /// Its type really should be Vec<UntypedHandle>, but cxx won't have it
        unsafe fn run_rust_incoming_handler(
            info: &EndpointInfo,
            payload: &[u8],
            handles: Vec<usize>,
            raw_message_handle: UniquePtr<ScopedMessageHandleWrapper>,
            responder: UniquePtr<MojoResponderWrapper>,
        ) -> bool;

        /// Called by C++ to invoke the user-provided Rust disconnect handler
        fn run_rust_disconnect_handler(info: Box<EndpointInfo>);
    }

    unsafe extern "C++" {
        include!("mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h");
        include!("mojo/public/rust/bindings/multiplex_router/cpp_interop/rust_associated_group_controller.h");
        type AssociatedEndpointRustAdapter;
        type RustAssociatedGroupController;

        /// Creates the C++ group controller for a Rust router, which will be
        /// destroyed on `runner`'s sequence. The returned pointer owns one
        /// ref-count.
        fn CreateGroupControllerForRustRouter(
            router: Box<MultiplexRouter>,
            runner: Pin<&mut SequencedTaskRunner>,
        ) -> *mut RustAssociatedGroupController;

        /// Increments the group controller's ref-count.
        fn AddRef(self: &RustAssociatedGroupController);

        // TODO(crbug.com/472552387): Tweak `cxx` to make this `allow` obsolete.
        #[allow(clippy::missing_safety_doc)]
        /// Decrements the group controller's ref-count, possibly destroying it.
        ///
        /// # Safety
        /// The caller must own a ref-count of this controller, and must not
        /// dereference it afterwards unless it owns another one.
        unsafe fn Release(self: &RustAssociatedGroupController);

        /// Constructs a fresh C++ `mojo::Message` with the given payload,
        /// and attaches the handles to it.
        ///
        /// # Safety
        /// `handles` must contain only live, unowned raw handle values.
        /// Ownership of those handles is transferred to C++.
        /// Its type really should be Vec<UntypedHandle>, but cxx won't have it
        #[allow(clippy::missing_safety_doc)]
        unsafe fn CreateOutgoingMessage(payload: &[u8], handles: Vec<usize>) -> UniquePtr<Message>;

        /// Constructs a C++ `mojo::Message` from an existing incoming message,
        /// from which we already extracted the attached handles.
        ///
        /// # Safety
        /// `handles` must contain only live, unowned raw handle values.
        /// Ownership of those handles is transferred to C++.
        /// Its type really should be Vec<UntypedHandle>, but cxx won't have it
        #[allow(clippy::missing_safety_doc)]
        unsafe fn CreateIncomingMessage(
            raw_handle: UniquePtr<ScopedMessageHandleWrapper>,
            handles: Vec<usize>,
        ) -> UniquePtr<Message>;

        /// Creates a pair of entangled C++ pending associated endpoints.
        fn CreatePairPendingAssociation(
            self_out: &mut UniquePtr<AssociatedEndpointRustAdapter>,
            peer_out: &mut UniquePtr<AssociatedEndpointRustAdapter>,
        );

        /// Binds the endpoint to a sequence and starts routing incoming
        /// messages to Rust.
        fn Bind(
            self: Pin<&mut AssociatedEndpointRustAdapter>,
            runner: Pin<&mut SequencedTaskRunner>,
            info: Box<EndpointInfo>,
        );

        /// Returns the associated interface ID assigned to this endpoint.
        fn GetInterfaceId(self: &AssociatedEndpointRustAdapter) -> u32;

        /// Sends an outgoing Mojom IPC message through the C++ endpoint.
        fn SendMessage(self: &AssociatedEndpointRustAdapter, message: UniquePtr<Message>);

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
        /// May only be called once.
        /// The name is counterintuitive, but that's the C++ naming scheme.
        fn Accept(self: &MojoResponderWrapper, message: UniquePtr<Message>);

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
// SAFETY: All `&self` methods on `AssociatedEndpointRustAdapter` are
// thread-safe. `SendMessage()` automatically bounces to the bound sequence if
// called off-sequence. `Bind()` mutates the adapter and is safely protected by
// taking `&mut self`.
unsafe impl Sync for ffi::AssociatedEndpointRustAdapter {}

// SAFETY: `MojoResponderWrapper` wraps a C++ `base::SequenceBound`, so
// it can be safely transferred across threads.
unsafe impl Send for ffi::MojoResponderWrapper {}

// SAFETY: `RustAssociatedGroupController` is an `AssociatedGroupController`,
// so ref-counting is the only mechanism managing its lifetime.
unsafe impl CxxRefCounted for ffi::RustAssociatedGroupController {
    fn add_ref(&self) {
        self.AddRef();
    }

    // SAFETY: The trait imposes the same requirements as `Release`.
    unsafe fn release(&self) {
        // SAFETY: Same requirements as the function.
        unsafe { self.Release() };
    }
}

// SAFETY: `AssociatedGroupController` uses an atomic ref-count, and its
// `Release` always posts to the sequence it's bound to.
unsafe impl CxxRefCountedThreadSafe for ffi::RustAssociatedGroupController {}
// SAFETY: The controller's only field is a `Box<MultiplexRouter>`, which is
// thread-safe, and the `AssociatedGroupController` methods it inherits only
// touch the ref-count.
unsafe impl Send for ffi::RustAssociatedGroupController {}
// SAFETY: As above.
unsafe impl Sync for ffi::RustAssociatedGroupController {}
