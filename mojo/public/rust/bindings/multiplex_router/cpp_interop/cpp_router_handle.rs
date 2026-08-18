// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the `CppRouterHandle` type, which fills the same role as
//! a `MultiplexRouterHandle` for associated interface endpoints whose message
//! pipe is managed by a C++ `AssociatedGroupController` rather than a Rust
//! `MultiplexRouter`.
//!
//! TODO(crbug.com/540359007): Reevaluate whether sequence management should
//! move to Rust once we have better sequenced task runner abstractions in Rust.

chromium::import! {
  "//base:sequenced_task_runner";
  "//mojo/public/rust/system";
}

use system::mojo_types::MessageHandle;
use system::scoped_handle_interop::ScopedMessageHandleWrapper;

use crate::message::{MojomMessage, ReadableWithHandlesMessage, SendableMessage};
use crate::multiplex_router::response_sender::ResponseSender;
use crate::multiplex_router::{EndpointInfo, InterfaceId};

use super::cxx::ffi;

/// A handle to a router that lives in C++ (as opposed to a Rust
/// `MultiplexRouter`).
///
/// Ultimately, this type is just a wrapper around
/// `AssociatedEndpointRustAdapter`, which handles the actual translation into
/// C++ concepts.
pub struct CppRouterHandle {
    adapter: cxx::UniquePtr<ffi::AssociatedEndpointRustAdapter>,
}

impl Drop for CppRouterHandle {
    fn drop(&mut self) {
        self.close();
    }
}

impl CppRouterHandle {
    /// Returns `None` if `adapter` is null.
    pub fn new(adapter: cxx::UniquePtr<ffi::AssociatedEndpointRustAdapter>) -> Option<Self> {
        if adapter.is_null() {
            None
        } else {
            Some(Self { adapter })
        }
    }

    /// Return the interface ID of this endpoint.
    pub fn interface_id(&self) -> InterfaceId {
        self.adapter.GetInterfaceId()
    }

    /// Send a message through this C++ associated endpoint.
    pub fn send_message(&self, msg: MojomMessage) {
        let sendable: SendableMessage = msg.into();
        let handle = MessageHandle::from(sendable);
        let wrapper = ScopedMessageHandleWrapper::from_message_handle(handle);
        self.adapter.SendMessage(wrapper);
    }

    /// Register a new nested associated endpoint with the C++ group controller.
    pub(crate) fn register_new_endpoint(
        &self,
        interface_id: Option<InterfaceId>,
        endpoint_info: Option<EndpointInfo>,
    ) -> Option<Self> {
        // `InterfaceId::MAX` is a signal for `RegisterNewEndpoint` to generate
        // a fresh interface ID.
        let id_val = interface_id.unwrap_or(InterfaceId::MAX);
        let new_adapter = self.adapter.RegisterNewEndpoint(id_val);
        let mut handle = Self::new(new_adapter)?;
        if let Some(info) = endpoint_info {
            handle.bind(info);
        }
        Some(handle)
    }

    /// Bind this endpoint to receive incoming messages and disconnect events.
    ///
    /// `endpoint_info` will be stashed behind a pointer and passed to
    /// `cxx_incoming_handler` and `cxx_disconnect_handler` when they're
    /// invoked; the latter is responsible for freeing it.
    pub(crate) fn bind(&mut self, endpoint_info: EndpointInfo) {
        let task_runner = endpoint_info.runner.clone();
        let info = Box::new(endpoint_info);
        let runner = task_runner.as_scoped_refptr();
        self.adapter.pin_mut().Bind(runner, info);
    }

    /// Close the endpoint and send disconnect notification over the IPC pipe.
    pub fn close(&mut self) {
        if let Some(adapter) = self.adapter.as_mut() {
            adapter.Close();
        }
    }
}

// Two `CppRouterHandles` are considered equal only if they are the same
// object, wrapping the same underlying adapter pointer.
impl PartialEq for CppRouterHandle {
    fn eq(&self, other: &Self) -> bool {
        match (self.adapter.as_ref(), other.adapter.as_ref()) {
            (Some(self_adapter), Some(other_adapter)) => std::ptr::eq(self_adapter, other_adapter),
            _ => false,
        }
    }
}

impl Eq for CppRouterHandle {}

/// Equivalent to the `ResponseSender` type, but for C++-owned pipes.
/// The C++ side does all the work here; it holds:
/// - An `AssociatedGroupController` to register new endpoints
/// - A responder to send reply messages, only if one is expected.
pub struct CppResponseSender {
    responder: cxx::UniquePtr<ffi::MojoResponderWrapper>,
}

impl CppResponseSender {
    pub(crate) fn new(responder: cxx::UniquePtr<ffi::MojoResponderWrapper>) -> Self {
        Self { responder }
    }

    /// Create a copy of this sender which can be used to register new
    /// endpoints, but can't be used to send messages (since the underlying
    /// C++ message sending part can't be cloned).
    pub(crate) fn registrar_only(&self) -> Self {
        Self::new(self.responder.CloneAsRegistrar())
    }

    pub(crate) fn send_message(&self, msg: MojomMessage) {
        assert!(
            self.responder.CanSendResponse(),
            "Tried to send a response to a message that didn't expect one \
            (or via a ResponseSender that was cloned as registrar-only)."
        );
        let sendable: SendableMessage = msg.into();
        let handle = MessageHandle::from(sendable);
        let wrapper = ScopedMessageHandleWrapper::from_message_handle(handle);
        self.responder.Accept(wrapper);
    }

    pub(crate) fn register_new_endpoint(
        &self,
        interface_id: Option<InterfaceId>,
        endpoint_info: Option<EndpointInfo>,
    ) -> Option<CppRouterHandle> {
        // `InterfaceId::MAX` is a signal for `RegisterNewEndpoint` to generate
        // a fresh interface ID.
        let id_val = interface_id.unwrap_or(InterfaceId::MAX);
        let new_adapter = self.responder.RegisterNewEndpoint(id_val);
        let mut handle = CppRouterHandle::new(new_adapter)?;
        if let Some(info) = endpoint_info {
            handle.bind(info);
        }
        Some(handle)
    }
}

// Note that the following functions don't need a specific ABI, since cxx
// handles that in the bridge.

/// This is the function called by C++ whenever it gets an incoming message. Its
/// job is to invoke the user-provided message handler that was passed to
/// `Bind`.
///
/// Returns `true` if the message was accepted and `false` otherwise (e.g.
/// if it was malformed).
///
/// Note that `responder` will always be capable of registering new endpoints,
/// and will only be capable of sending a response if one is expected.
pub(super) fn cxx_incoming_handler(
    info: &EndpointInfo,
    wrapper: cxx::UniquePtr<ffi::ScopedMessageHandleWrapper>,
    responder: cxx::UniquePtr<ffi::MojoResponderWrapper>,
) -> bool {
    let handle = ScopedMessageHandleWrapper::into_message_handle(wrapper)
        .expect("Message handle must not be null");
    let readable = ReadableWithHandlesMessage::from(handle);
    let Some(msg) = MojomMessage::parse_raw_or_report_bad_message(readable) else {
        return false;
    };
    let sender = ResponseSender::cpp(CppResponseSender::new(responder));
    (info.incoming_message_handler)(msg, sender);
    true
}

/// This is the function called by C++ when the other associated endpoint is
/// disconnected. Its job is just to call the user-provided disconnect handler,
/// and to drop `info`.
#[allow(clippy::boxed_local)]
pub(super) fn cxx_disconnect_handler(info: Box<EndpointInfo>) {
    if let Some(handler) = info.disconnect_handler {
        let _ = info.runner.post_task(handler);
    }
}
