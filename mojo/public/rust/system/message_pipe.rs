// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
  "//mojo/public/rust/system:ffi_bindings" as mojo_ffi;
}

use crate::message::RawMojoMessage;
use crate::mojo_types::declare_trappable_typed_handle;
use mojo_ffi::message_pipe;
use mojo_ffi::{MojoResult, UntypedHandle};

declare_trappable_typed_handle!(MessageEndpoint);

impl MessageEndpoint {
    /// Create a new pair of endpoints corresponding to a new mojo message pipe.
    ///
    /// # Possible Error Codes:
    /// - `ResourceExhausted`: If some system/process/etc limit has been
    ///   reached.
    pub fn create_pipe() -> MojoResult<(MessageEndpoint, MessageEndpoint)> {
        message_pipe::MojoCreateMessagePipe().map(|(e1, e2)| (e1.into(), e2.into()))
    }

    /// Read the next message from the endpoint, if one exists.
    ///
    /// # Possible Error Codes:
    /// - `ShouldWait`: If there's no message but the other end of the pipe is
    ///   still open
    /// - `FailedPrecondition`: If there's no message and the other end of the
    ///   pipe is closed
    // FOR_RELEASE: Maybe replace the return type  with a dedicated enum instead
    // of a MojoResult
    pub fn read(&self) -> MojoResult<RawMojoMessage> {
        message_pipe::MojoReadMessage(&self.handle).map(|handle| handle.into())
    }

    /// Write the given message to the pipe, sending it to the other side.
    ///
    /// # Possible Error Codes:
    /// - `FailedPrecondition`: If the other end of the message pipe is closed.
    pub fn write(&self, msg: RawMojoMessage) -> MojoResult<()> {
        msg.finalize_for_sending();
        message_pipe::MojoWriteMessage(&self.handle, msg.message_handle)
    }
}
