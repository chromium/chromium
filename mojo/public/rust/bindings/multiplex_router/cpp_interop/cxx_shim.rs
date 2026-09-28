// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains shim functions that operate on a `MultiplexRouter`.
//!
//! As with most shims, they exist to let C++ call the relevant functions on
//! the router without having to deal with the full complexities of the Rust
//! code. That is, it provides functions with arguments types that are easier
//! for C++ to provide.

chromium::import! {
  "//base:sequenced_task_runner";
  "//mojo/public/rust/system";
}

use std::sync::Arc;

use cxx::UniquePtr;
use sequenced_task_runner::SequencedTaskRunnerHandle;
use system::message::ReadableBytesOnlyMessage;
use system::mojo_types::{RawMojoHandle, UntypedHandle};
use system::scoped_handle_interop::ScopedMessageHandleWrapper;

use super::cxx::ffi;
use crate::message::MojomMessage;
use crate::message_header::MessageHeader;
use crate::multiplex_router::endpoint_registry::{EndpointInfo, INVALID_INTERFACE_ID};
use crate::multiplex_router::multiplex_router::MultiplexRouter;
use crate::multiplex_router::response_sender::ResponseSender;

// Packages a Rust MojomMessage into a C++ `mojo::Message` for transmission or
// dispatch through C++ bindings.
impl From<MojomMessage> for cxx::UniquePtr<ffi::Message> {
    fn from(mut msg: MojomMessage) -> Self {
        let handle_values: Vec<RawMojoHandle> =
            msg.handles.into_iter().map(|h| h.into_raw_value()).collect();
        // A message handle is present if and only if this is an incoming
        // message.
        if let Some(raw) = msg.raw_message_handle.take() {
            // We forward the underlying buffer as-is, so any edit made to
            // `header` or `payload` on the Rust side would be silently lost.
            debug_assert!(
                matches_raw_buffer(&raw, &msg.header, &msg.payload),
                "Edits to a forwarded incoming message are not preserved"
            );
            let raw_wrapper = ScopedMessageHandleWrapper::from_message_handle(raw.into());
            // SAFETY: `handle_values` was created by destructing `handles`,
            // which by definition contained live handles that it alone owned
            unsafe { ffi::CreateIncomingMessage(raw_wrapper, handle_values) }
        } else {
            let mut payload = msg.header.serialize();
            payload.extend(msg.payload);
            // SAFETY: `handle` values was created by destructing `handles`,
            // which by definition contained live handles that it alone owned
            unsafe { ffi::CreateOutgoingMessage(&payload, handle_values) }
        }
    }
}

/// Returns true if `header` and `payload` still match the contents of the
/// underlying C++ message buffer.
fn matches_raw_buffer(
    raw: &ReadableBytesOnlyMessage,
    header: &MessageHeader,
    payload: &[u8],
) -> bool {
    let Ok(bytes) = raw.read_bytes() else {
        return false;
    };
    let Ok((remaining, raw_header)) = MessageHeader::deserialize(bytes) else {
        return false;
    };
    raw_header == *header && remaining == payload
}

/// Unpacks an incoming C++ message whose handles were already extracted by C++
/// into a Rust MojomMessage, taking ownership of the raw message handle.
pub fn create_incoming_message_rust(
    payload: &[u8],
    handles: Vec<UntypedHandle>,
    raw_wrapper: UniquePtr<ScopedMessageHandleWrapper>,
) -> Option<MojomMessage> {
    let mut message_handle: ReadableBytesOnlyMessage =
        ScopedMessageHandleWrapper::into_message_handle(raw_wrapper)?.into();
    let (remaining_bytes, header) = match crate::message_header::MessageHeader::deserialize(payload)
    {
        Ok(data) => data,
        Err(err) => {
            let _ = message_handle.report_bad_message(&err.to_string());
            return None;
        }
    };
    Some(MojomMessage {
        header,
        payload: remaining_bytes.to_vec(),
        handles,
        raw_message_handle: Some(message_handle),
    })
}

/// Allocates a new associated interface ID on the Rust router for a C++
/// endpoint. Returns `kInvalidInterfaceId` on failure.
pub fn allocate_interface_id(router: &MultiplexRouter) -> u32 {
    router.add_associated_interface(None, None).unwrap_or(INVALID_INTERFACE_ID)
}

/// Registers a received interface ID on the Rust router before it is bound.
/// Returns true on success, or false if the interface ID was invalid or already
/// registered.
pub fn register_interface_id(router: &MultiplexRouter, interface_id: u32) -> bool {
    router.add_associated_interface(Some(interface_id), None).is_some()
}

/// Transmits an outgoing message originating from a C++ associated endpoint
/// through the Rust router's underlying primary pipe. Returns true if the
/// message was successfully queued, or false if the pipe or endpoint is closed.
pub fn send_message_from_cpp(
    router: &MultiplexRouter,
    interface_id: u32,
    message_wrapper: UniquePtr<ScopedMessageHandleWrapper>,
) -> bool {
    let Some(handle) = ScopedMessageHandleWrapper::into_message_handle(message_wrapper) else {
        return false;
    };
    router.send_raw_message(interface_id, handle.into())
    // Note that if we fail, the C++ side will handle cleanup of any associated
    // interface IDs that were in the message, so we don't have to do it here.
}

/// Helper function: dreferences the endpoint client held by the handlers below
fn client_ref(
    client: &UniquePtr<ffi::RustAssociatedEndpointClient>,
) -> &ffi::RustAssociatedEndpointClient {
    client.as_ref().expect("client must not be null")
}

/// Binds a C++ associated endpoint client to the Rust router, setting up the
/// handlers to call out to `client` and run on the specified task runner.
pub fn attach_cpp_endpoint(
    router: &MultiplexRouter,
    interface_id: u32,
    client: UniquePtr<ffi::RustAssociatedEndpointClient>,
    runner: &sequenced_task_runner::ffi::SequencedTaskRunner,
) {
    let runner_handle = SequencedTaskRunnerHandle::clone_from_ref(runner);

    // TODO(crbug.com/524990003): Currently `client` is dropped when
    // `disconnect_handler` runs. Once MultiplexRouter tracks disconnected
    // endpoints in its map, `client` should be held by the entry until the
    // endpoint is detached or closed by C++.
    let client = Arc::new(client);
    let weak_client = Arc::downgrade(&client);

    // C++ creates its own equivalent of `_sender` so we can ignore it here.
    let incoming_handler = Arc::new(move |msg: MojomMessage, _sender: ResponseSender| {
        if let Some(client) = weak_client.upgrade() {
            // It would be nice to tear down the pipe if this fails, like C++
            // does, but that's much harder in Rust since we only
            // have a const ref here. We'll still report any bad
            // messages so this is mostly an inconvenience.
            let _ = ffi::run_cpp_incoming_handler(client_ref(&client), msg.into());
        }
    });

    let disconnect_handler = Box::new(move || {
        ffi::run_cpp_disconnect_handler(client_ref(&client));
    });

    let endpoint_info = EndpointInfo {
        runner: runner_handle,
        incoming_message_handler: incoming_handler,
        disconnect_handler: Some(disconnect_handler),
    };

    router.bind_interface(interface_id, endpoint_info);
}
