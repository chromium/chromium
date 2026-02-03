// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines (mostly) safe Rust wrappers around the Mojo message pipe
//! API. Note that this is distinct from the _message_ API in the `message`
//! module.
//!
//! Not all C API functions are included yet. More can be added as-needed by
//! following the example of existing wrappers.

chromium::import! {
  "//mojo/public/rust/system:mojo_c_system_bindings" as raw_ffi;
}

use crate::handles::*;
use crate::result::*;

/// Create a new message pipe, which allows bidirectional communication by
/// sending message between its endpoints. See the `message` module for more
/// information about messages.
///
/// On success, returns handles for the pipe's two endpoints. Note that unlike
/// data pipes, which are unidirectional, message pipe endpoints are
/// interchangeable.
///
/// # Possible Error Codes:
/// - `ResourceExhausted`: if some quota has been reached (system, process, etc)
///   that prevents new handles from being created.
pub fn MojoCreateMessagePipe() -> MojoResult<(UntypedHandle, UntypedHandle)> {
    let mut handle1: raw_ffi::MojoHandle = 0;
    let mut handle2: raw_ffi::MojoHandle = 0;

    // SAFETY: the options pointer is allowed to be null;
    // the other two pointers are derived from references
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoCreateMessagePipe(
            std::ptr::null(), // This function has no options
            std::ptr::from_mut(&mut handle1),
            std::ptr::from_mut(&mut handle2),
        )
    });

    ret.map(|_| {
        (
            // SAFETY: We just got these handles from Mojo
            unsafe { UntypedHandle::wrap_raw_value(handle1) },
            unsafe { UntypedHandle::wrap_raw_value(handle2) },
        )
    })
}

/// Read the next message from the message pipe endpoint. This removes it from
/// the pipe's queue of messages. See the `message` module for more information
/// about using the returned message handle.
///
/// # Requirements:
/// - `message_pipe` must actually be a message pipe endpoint handle.
///
/// # Possible Error Codes:
/// - `InvalidArgument`: If the requirements were violated.
/// - `ShouldWait`: If there are no messages waiting to be read, but the other
///   end of the pipe is still open.
/// - `FailedPrecondition`: If there are no messages waiting to be read, and the
///   other end of the pipe has been closed.
pub fn MojoReadMessage(message_pipe: &UntypedHandle) -> MojoResult<MessageHandle> {
    let mut message_handle: raw_ffi::MojoMessageHandle = 0;

    // SAFETY: the options pointer is allowed to be null;
    // the pointers is derived from a reference;
    // The `UntypedHandle` type guarantees its handle is live.
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoReadMessage(
            message_pipe.handle_value.into(),
            std::ptr::null(), // This function has no options
            std::ptr::from_mut(&mut message_handle),
        )
    });

    // SAFETY: We just got this handle from Mojo
    ret.map(|_| unsafe { MessageHandle::wrap_raw_value(message_handle) })
}

/// Send `message` through the message pipe, so it can later be read from the
/// other endpoint.
///
/// Note that the message object is consumed even if the write fails.
///
/// # Requirements:
/// - `message_pipe` must actually be a message pipe endpoint handle.
///
/// # Possible Error Codes:
/// - `InvalidArgument`: If the requirements were violated.
/// - `NotFound`: If the message is not in a sendable state (serialized or with
///   a context attached).
/// - `FailedPrecondition`: If the other end of the pipe has been closed.
pub fn MojoWriteMessage(message_pipe: &UntypedHandle, message: MessageHandle) -> MojoResult<()> {
    // SAFETY: the options pointer is allowed to be null;
    // The `UntypedHandle` and `MessageHandle` types guarantee that their handles
    // are live.
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoWriteMessage(
            message_pipe.handle_value.into(),
            message.handle_value.into(),
            std::ptr::null(), // This function has no options
        )
    });

    if ret.is_ok() {
        // This message was sent, so ownership of the handle
        // has been transferred to the recipient.
        std::mem::forget(message);
    };

    ret
}
