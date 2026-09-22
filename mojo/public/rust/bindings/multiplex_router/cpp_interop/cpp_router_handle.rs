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

use system::mojo_types::{RawMojoHandle, UntypedHandle};

use crate::message::MojomMessage;
use crate::multiplex_router::response_sender::ResponseSender;
use crate::multiplex_router::{EndpointInfo, InterfaceId, INVALID_INTERFACE_ID};

use super::cxx::ffi;

/// A handle to a router that goes through C++ (as opposed to directly holding a
/// Rust `MultiplexRouter`).
///
/// Really, this is just a wrapper around `AssociatedEndpointRustAdapter`, which
/// represents a "generic" router. It could be backed by either Rust or C++,
/// and can be used to create either a Rust or a C++ associated endpoint.
/// The type is implemented in C++, so this wrapper type lets us interact with
/// it from Rust.
pub struct CppRouterHandle {
    /// Invariant: this should never be null
    adapter: cxx::UniquePtr<ffi::AssociatedEndpointRustAdapter>,
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
    ///
    /// Note: Unlike `MultiplexRouter::send_message`, the interface ID and any
    /// serialized associated endpoints are already embedded in the message
    /// payload, so the underlying C++ `InterfaceEndpointClient` handles
    /// routing directly.
    pub fn send_message(&self, msg: MojomMessage) {
        self.adapter.SendMessage(msg.into());
    }

    /// Register a new nested associated endpoint with the C++ group controller.
    pub(crate) fn register_new_endpoint(
        &self,
        interface_id: Option<InterfaceId>,
        endpoint_info: Option<EndpointInfo>,
    ) -> Option<Self> {
        assert!(
            interface_id != Some(INVALID_INTERFACE_ID),
            "kInvalidInterfaceId is not a valid interface ID"
        );
        // Here, INVALID_INTERFACE_ID is used as a signal to request a new ID be
        // allocated.
        let id_val = interface_id.unwrap_or(INVALID_INTERFACE_ID);
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

    pub(crate) fn send_message(self, msg: MojomMessage) {
        assert!(
            !self.responder.is_null() && self.responder.CanSendResponse(),
            "Tried to send a response to a message that didn't expect one."
        );
        self.responder.Accept(msg.into());
    }

    pub(crate) fn register_new_endpoint(
        &self,
        interface_id: Option<InterfaceId>,
        endpoint_info: Option<EndpointInfo>,
    ) -> Option<CppRouterHandle> {
        if self.responder.is_null() {
            return None;
        }
        assert!(
            interface_id != Some(INVALID_INTERFACE_ID),
            "kInvalidInterfaceId is not a valid interface ID"
        );
        // Here, INVALID_INTERFACE_ID is used as a signal to request a new ID be
        // allocated.
        let id_val = interface_id.unwrap_or(INVALID_INTERFACE_ID);
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
///
/// # Safety
///  `handles` must contain only live, unowned raw handle values.
/// Ownership of those handles is transferred into the function.
pub(super) unsafe fn run_rust_incoming_handler(
    info: &EndpointInfo,
    payload: &[u8],
    handles: Vec<RawMojoHandle>,
    raw_message_handle: cxx::UniquePtr<ffi::ScopedMessageHandleWrapper>,
    responder: cxx::UniquePtr<ffi::MojoResponderWrapper>,
) -> bool {
    // SAFETY: The caller guarantees `handles` contains live, unowned handles.
    let untyped_handles =
        handles.into_iter().map(|h| unsafe { UntypedHandle::wrap_raw_value(h) }).collect();
    let Some(msg) =
        super::cxx_shim::create_incoming_message_rust(payload, untyped_handles, raw_message_handle)
    else {
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
pub(super) fn run_rust_disconnect_handler(info: Box<EndpointInfo>) {
    if let Some(handler) = info.disconnect_handler {
        let _ = info.runner.post_task(handler);
    }
}
