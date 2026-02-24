// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
  "//mojo/public/rust/system:ffi_bindings" as mojo_ffi;
}

use mojo_ffi::message;
use mojo_ffi::{MessageHandle, MojoError, MojoResult, UntypedHandle};

/// An object representing a Mojo message that can be (or has been) sent through
/// a pipe. This type is not thread-safe, and must be used from a single thread
/// at a time.
///
/// Logically, the message has two components: a payload composed of raw bytes,
/// and zero or more untyped mojo handles which are sent alongside the payload.
/// These components can be accessed and manipulated using the message's
/// methods.
///
/// FOR_RELEASE: Messages have various conditions which determine which
/// operations are permitted. We can probably encapsulate this using a typestate
/// pattern. More research on the possible states is needed, but it would
/// probably look something like `WriteableMessage` -> `SendableMessage` ->
/// <pipe> -> `FullyReadableMessage` -> `BytesOnlyReadableMessage`.
///
/// But, y'know, with better names.
pub struct RawMojoMessage {
    pub(crate) message_handle: MessageHandle,
}

impl From<MessageHandle> for RawMojoMessage {
    fn from(message_handle: MessageHandle) -> Self {
        Self { message_handle }
    }
}

impl From<RawMojoMessage> for MessageHandle {
    fn from(message: RawMojoMessage) -> Self {
        message.message_handle
    }
}

impl Default for RawMojoMessage {
    fn default() -> Self {
        Self::new()
    }
}

impl RawMojoMessage {
    /// Construct an empty RawMojoMessage
    pub fn new() -> Self {
        message::MojoCreateMessage(message::CreateMessageFlags::empty()).into()
    }

    /// Construct a new message with the specified contents. If this operation
    /// succeeds, ownership of the handles is transferred to the message object;
    /// otherwise, they are returned as part of the result.
    pub fn new_with_data(
        bytes: &[u8],
        handles: Vec<UntypedHandle>,
    ) -> Result<Self, (MojoError, Vec<UntypedHandle>)> {
        let mut msg = Self::new();
        msg.append_data(bytes, handles).map(|_| msg)
    }

    /// Construct a new message with the given bytes as its payload, and no
    /// associated handles.
    pub fn new_with_bytes(bytes: &[u8]) -> MojoResult<Self> {
        let mut msg = Self::new();
        msg.append_bytes(bytes).map(|bytes_written| {
            // FOR_RELEASE: Handle this more gracefully
            assert!(bytes_written == bytes.len());
            msg
        })
    }

    /// Append the bytes in `bytes` and the handles in `handles` to the
    /// message's payload. Ownership of the handles is transferred to the
    /// other side of the pipe if the operation is successful. If the operation
    /// fails, any handles whose ownership was not transferred are returned to
    /// the caller.
    ///
    /// If successful, returns the number of bytes successfully written.
    /// FOR_RELEASE: Maybe have this write all the bytes (in a loop) instead.
    ///
    /// # Possible Error Codes:
    /// - `ResourceExhausted`: if the additional payload size exceeds some
    ///   implementation- or embedder-defined maximum.
    /// - `FailedPrecondition` if the message already has a context attached
    pub fn append_data(
        &mut self,
        bytes: &[u8],
        handles: Vec<UntypedHandle>,
    ) -> Result<usize, (MojoError, Vec<UntypedHandle>)> {
        message::MojoAppendMessageData(
            &self.message_handle,
            message::AppendMessageDataFlags::empty(),
            bytes,
            handles,
        )
    }

    /// Append bytes to the message's payload. If successful, returns the number
    /// of bytes written.
    ///
    /// See `append_data` for possible error codes.
    pub fn append_bytes(&mut self, bytes: &[u8]) -> MojoResult<usize> {
        self.append_data(bytes, Vec::new()).map_err(|(result, _)| result)
    }

    /// Append untyped handles to the message's payload. Ownership of the
    /// handles is transferred to the message only if the operation succeeds.
    ///
    /// See `append_data` for possible error codes.
    pub fn append_handles(
        &mut self,
        handles: Vec<UntypedHandle>,
    ) -> Result<(), (MojoError, Vec<UntypedHandle>)> {
        self.append_data(&[], handles).map(|_| ())
    }

    /// Retrieve the payload, if present, and attached handles from this message
    /// object.
    ///
    /// Because this function transfers ownership of the handles to the caller,
    /// it may only be called *once* per message object. Any further calls will
    /// return `Err(NotFound)`, unless the message never had any handles
    /// attached in the first place.
    ///
    /// If you need to read only the payload, use `read_bytes` instead,
    /// which does not have this restriction and does not prevent `read_data`
    /// function from being called later.
    ///
    /// FOR_RELEASE: We might be able to avoid returning a MojoResult here if we
    /// manage to handle all possible errors internally (e.g. by reporting a bad
    /// message). If not, catalogue the possible errors.
    pub fn read_data(&self) -> MojoResult<(&[u8], Vec<UntypedHandle>)> {
        // First, query the message to see how many handles are attached.
        let num_handles = match message::MojoGetMessageData(
            &self.message_handle,
            Some(Vec::new().spare_capacity_mut()),
        ) {
            // If the call succeeded, then no handles were attached, and we're done!
            message::GetMessageDataStatus::Success { bytes, .. } => {
                return Ok((bytes, vec![]));
            }
            message::GetMessageDataStatus::NotEnoughCapacity { num_handles_attached } => {
                num_handles_attached
            }
            message::GetMessageDataStatus::Error(err) => {
                return Err(err);
            }
        };

        // Copy the handles from the message into a vector. This prevents further
        // reads of the handles.
        let mut handles: Vec<UntypedHandle> = Vec::with_capacity(num_handles as usize);

        match message::MojoGetMessageData(&self.message_handle, Some(handles.spare_capacity_mut()))
        {
            // If the call succeeded, then no handles were attached, and we're done!
            message::GetMessageDataStatus::Success { bytes, num_handles_written } => {
                // SAFETY: `MojoGetMessageData` guarantees that the first `num_handles_written`
                // elements of `handles` are now initialized.
                unsafe { handles.set_len(num_handles_written) };
                return Ok((bytes, handles));
            }
            message::GetMessageDataStatus::NotEnoughCapacity { .. } => {
                // We just allocated enough capacity
                unreachable!()
            }
            message::GetMessageDataStatus::Error(err) => {
                return Err(err);
            }
        };
    }

    /// Read just the bytes of this message's payload, ignoring any attached
    /// handles.
    ///
    /// Unlike `read_data`, this function can be called multiple times.
    ///
    /// FOR_RELEASE: See if we can avoid returning a MojoError here by handling
    /// error cases internally. If not, catalogue the possible errors.
    pub fn read_bytes(&self) -> MojoResult<&[u8]> {
        match message::MojoGetMessageData(&self.message_handle, None) {
            // If the call succeeded, then no handles were attached, and we're done!
            message::GetMessageDataStatus::Success { bytes, .. } => {
                return Ok(bytes);
            }
            message::GetMessageDataStatus::NotEnoughCapacity { .. } => {
                // We passed `None` above so the function will ignore capacity
                unreachable!()
            }
            message::GetMessageDataStatus::Error(err) => {
                return Err(err);
            }
        };
    }

    /// Mark the message as ready to be sent.
    ///
    /// FOR_RELEASE: Figure out and document the implications of this
    /// (presumably you can't append more data afterwards?)
    ///
    /// FOR_RELEASE: Maybe use the type-state pattern here so we can't confuse
    /// ready and unready messages.
    pub fn finalize_for_sending(&self) {
        let ret = message::MojoAppendMessageData(
            &self.message_handle,
            message::AppendMessageDataFlags::COMMIT_SIZE,
            &[],
            vec![],
        );
        debug_assert!(ret.is_ok());
    }

    /// Tell the underlying mojo system that this message was malformed, which
    /// will trigger the error handler which was set up during mojo
    /// initialization. Typically this will result in the process that
    /// created the message being terminated.
    ///
    /// Note that reporting a bad message does not stop the _current_ process
    /// from running. However, you should almost always abort whatever you're
    /// doing, rather than try to continue processing the known-bad message.
    /// To make it harder to forget to do so, this function will always return
    /// an error result so the compiler will warn if you try to treat it like a
    /// panic and ignore its return value.
    pub fn report_bad_message(&self, error_msg: &str) -> Result<(), BadMessageError> {
        // Ignore the MojoError; this can only fail if the message_handle is invalid,
        // and we guarantee that it's valid as part of this type.
        // SAFETY: We guarantee that our contained handle is alive.
        message::MojoNotifyBadMessage(&self.message_handle, error_msg);
        Err(BadMessageError)
    }
}

/// Error that is always returned from [`report_bad_message`], to encourage
/// the caller to stop any further processing and propagate the error up.
#[derive(Debug)]
pub struct BadMessageError;

impl std::fmt::Display for BadMessageError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(
            f,
            "This error is a reminder to the programmer to stop processing \
             after calling `report_bad_message`"
        )?;
        Ok(())
    }
}

impl std::error::Error for BadMessageError {}
