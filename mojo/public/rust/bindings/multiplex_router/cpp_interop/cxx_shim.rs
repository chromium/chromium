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
  "//mojo/public/rust/system";
}

use cxx::UniquePtr;
use system::message::ReadableBytesOnlyMessage;
use system::mojo_types::{RawMojoHandle, UntypedHandle};
use system::scoped_handle_interop::ScopedMessageHandleWrapper;

use super::cxx::ffi;
use crate::message::MojomMessage;
use crate::message_header::MessageHeader;
use crate::multiplex_router::endpoint_registry::INVALID_INTERFACE_ID;
use crate::multiplex_router::multiplex_router::MultiplexRouter;

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
