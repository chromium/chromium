// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines (mostly) safe Rust wrappers around the Mojo message API.
//! Note that this is distinct from the message _pipe_ API in the `message_pipe`
//! module.
//!
//! Not all C API functions are included yet. More can be added as-needed by
//! following the example of existing wrappers.

chromium::import! {
  "//mojo/public/rust/system:mojo_c_system_bindings" as raw_ffi;
}

use crate::internal_options::declare_mojo_options;

use crate::handles::*;
use crate::result::*;

bitflags::bitflags! {
    /// The possible flags that can be passed to MojoCreateMessage
    #[derive(Clone, Copy, Default)]
    pub struct CreateMessageFlags: u32 {
        /// Do not enforce size restrictions on this message.
        const UNLIMITED_SIZE = 1 << 0;
    }
}
declare_mojo_options!(MojoCreateMessageOptions, flags: raw_ffi::MojoCreateMessageFlags);

/// Create a new message object which can be sent over a message pipe.
///
/// In its initial state the message object cannot be successfully written to a
/// message pipe, but must first have either an opaque context or
/// some serialized data attached (see `MojoSetMessageContext` and
/// `MojoAppendMessageData`).
///
/// Each message object contains a *payload* with two components: a chunk of raw
/// bytes and a list of `UntypedHandle`s. Data can be written to the payload
/// using `MojoAppendMessageData` and read using `MojoGetMessageData`.
/// Appending/reading handles transfers ownership of those handlers to/from the
/// message.
///
/// # Flags:
/// - UNLIMITED_SIZE: If passed, the message's payload may grow to an arbitrary
///   size. Otherwise, Mojo will assert-fail if its payload grows past a
///   globally configured threshold.
pub fn MojoCreateMessage(flags: CreateMessageFlags) -> MessageHandle {
    let options = MojoCreateMessageOptions::new(flags.bits());
    let mut message_handle: raw_ffi::MojoMessageHandle = 0;

    // SAFETY: Both pointers are to stack data
    // Surprisingly, according to its comments, this function will not fail
    // unless the message pointer is null, which it isn't.
    unsafe {
        raw_ffi::MojoCreateMessage(options.as_ptr(), std::ptr::from_mut(&mut message_handle))
    };

    // SAFETY: we just got this handle from Mojo
    unsafe { MessageHandle::wrap_raw_value(message_handle) }
}

bitflags::bitflags! {
    /// The possible flags that can be passed to MojoAppendMessageData
    #[derive(Clone, Copy, Default)]
    pub struct AppendMessageDataFlags: u32 {
        /// Indicates that the message is now complete and its size will no longer increase.
        const COMMIT_SIZE = 1 << 0;
    }
}
declare_mojo_options!(MojoAppendMessageDataOptions, flags: raw_ffi::MojoAppendMessageDataFlags);

/// Append data to the message's payload. Returns the number of bytes written.
///
/// This will append the contents of `bytes` and `handles` to the message's
/// internal payload buffer. Ownership of the handles is transferred to the
/// message object if this operation succeeds; otherwise, the handles are
/// returned to the caller unchanged (either all handles are appended or none).
///
/// This function will panic if you try to append more than 2^32 handles at
/// once. If you absolutely must do so, append them in chunks.
///
/// # Flags:
/// - COMMIT_SIZE: Indicate that this is the final write operation for the
///   message. This is one way of marking the message as ready to be sent.
///
/// # Possible Error Codes:
/// - `ResourceExhausted`: if the additional payload size exceeds some
///   implementation- or embedder-defined maximum.
/// - `FailedPrecondition` if the message already has a context attached
pub fn MojoAppendMessageData(
    message: &MessageHandle,
    flags: AppendMessageDataFlags,
    bytes: &[u8],
    handles: Vec<UntypedHandle>,
) -> Result<usize, (MojoError, Vec<UntypedHandle>)> {
    let options = MojoAppendMessageDataOptions::new(flags.bits());
    let raw_handles_ptr: *const raw_ffi::MojoHandle = UntypedHandle::slice_as_ptr(&handles);
    let mut buffer_ptr: *mut std::ffi::c_void = std::ptr::null_mut();
    let mut buffer_size: u32 = 0;

    // SAFETY: All pointers are to stack data, except possibly `raw_handles_ptr`.
    // `slice_as_ptr` guarantees that which is null iff `handles.len()` is 0; which
    // is permitted by the function. We're passing `handles.len()` as the number
    // of handles, so we won't read past the end of `handles`.
    let result = MojoError::result_from_code(unsafe {
        raw_ffi::MojoAppendMessageData(
            message.handle_value.into(),
            bytes.len().try_into().unwrap_or(u32::MAX), // Payload size
            raw_handles_ptr,
            handles.len().try_into().unwrap(), // Number of handles
            options.as_ptr(),
            std::ptr::from_mut(&mut buffer_ptr),
            std::ptr::from_mut(&mut buffer_size),
        )
    });

    match result {
        Ok(()) => {
            // If `MojoAppendMessageData` succeeds, ownership of all handles is
            // transferred to the C object, so prevent Rust from double-free-ing them.
            for handle in handles.into_iter() {
                std::mem::forget(handle);
            }
        }
        Err(err) => {
            return Err((err, handles));
        }
    };

    // `MojoAppendMessageData` copies the handles for us, but returns a pointer to
    // its buffer that we need to write to ourselves.
    if !bytes.is_empty() {
        // Will not panic if usize has at least 32 bits, which is true for Chromium
        // targets
        let buffer_size: usize = buffer_size.try_into().unwrap();
        // This may be unnecessary, but `MojoAppendMessageData` doesn't explicitly
        // promise that the returned capacity is at most the requested amount.
        let bytes_to_write = std::cmp::min(bytes.len(), buffer_size);

        // SAFETY: The API call guarantees that the buffer pointer is valid
        // and has at least `buffer_size` capacity, and `bytes_to_write` is
        // at most `buffer_size`.
        let buffer_slice: &mut [u8] =
            unsafe { std::slice::from_raw_parts_mut(buffer_ptr.cast(), bytes_to_write) };

        buffer_slice.copy_from_slice(&bytes[..bytes_to_write]);
        return Ok(bytes_to_write);
    };

    Ok(0)
}

bitflags::bitflags! {
    /// The possible flags that can be passed to MojoGetMessageData
    /// This struct is private because it's passed implicitly based on whether
    /// the `handles` argument to `MojoGetMessageData` is `Some` or `None`.
    #[derive(Clone, Copy, Default)]
    struct GetMessageDataFlags: u32 {
        /// Only retrieve the "raw bytes" parts of the payload
        const IGNORE_HANDLES = 1 << 0;
    }
}
declare_mojo_options!(MojoGetMessageDataOptions, flags: raw_ffi::MojoGetMessageDataFlags);

pub enum GetMessageDataStatus<'a> {
    Success { bytes: &'a [u8], num_handles_written: usize },
    NotEnoughCapacity { num_handles_attached: u32 },
    Error(MojoError),
}

/// Retrieve the message's payload.
///
/// If `handles` is `Some` and this function is successful, it will return a
/// reference to the message's underlying bytes buffer, copy the attached
/// handles into `handles`, and return the number of handles written. This
/// transfers ownership of any attached handles to the caller. Because of the
/// ownership transfer, this function may be (successfully) called at most once
/// per message without the `IGNORE_HANDLES` flag.
///
/// If `handles` is `Some` and does not have enough capacity to store all
/// attached handles, the function will return `NotEnoughCapacity` containing
/// the number of handles attached to the message.
///
/// If `handles` is `None`, then no handles will be copied and this function
/// will never return `NotEnoughCapacity`.
///
/// If something else goes wrong, the function will return a `MojoError`, or
/// panic if `handles` has more than 2^32 elements.
///
/// # Safety Guarantees
/// - If this function returns `Success`, then the first `num_handles_written`
///   elements of `handles` have been initialized.
///
/// # Possible Error Codes:
/// - `FailedPrecondition`: If the message is not fully serialized (use
///   `MojoSerializeMessage` or `MojoAppendMessageData` to mark it as ready for
///   transfer/reading)
/// - `NotFound`: if the message's handles have already been extracted and
///   `handles` was not `None`
/// - `Aborted`: if the message is in an unrecoverable state.
// FOR_RELEASE: Add a fully-safe equivalent that allocates memory for the
// handles instead of taking a buffer as an out-parameter, and/or one that takes
// in an initialized slice to start with.
// FOR_RELEASE: Add a mutable equivalent (takes &mut, returns &mut)
pub fn MojoGetMessageData<'a>(
    message: &'a MessageHandle,
    handles: Option<&mut [std::mem::MaybeUninit<UntypedHandle>]>,
) -> GetMessageDataStatus<'a> {
    let flags = if handles.is_none() {
        GetMessageDataFlags::IGNORE_HANDLES
    } else {
        GetMessageDataFlags::empty()
    };
    let options = MojoGetMessageDataOptions::new(flags.bits());

    let mut buffer: *mut std::ffi::c_void = std::ptr::null_mut();
    let mut num_bytes: u32 = 0;
    let (handles_ptr, mut num_handles) = match handles {
        // SAFETY: This cast is sound because `MaybeUninit<UntypedHandle>` has the
        // same memory layout as `UntypedHandle`, which has the same memory layout
        // as `MojoHandle`.
        Some(slice) => (std::ptr::from_mut(slice).cast(), slice.len().try_into().unwrap()),
        None => (std::ptr::null_mut(), 0),
    };

    // SAFETY: All pointers are either to stack variables, or validly null.
    // If `num_handles` is nonzero, then it contains the length of `handles_ptr`,
    // so this function will not write past the end of `handles`.
    let result = MojoError::result_from_code(unsafe {
        raw_ffi::MojoGetMessageData(
            message.handle_value.into(),
            options.as_ptr(),
            std::ptr::from_mut(&mut buffer),
            std::ptr::from_mut(&mut num_bytes),
            handles_ptr,
            std::ptr::from_mut(&mut num_handles),
        )
    });

    match result {
        Ok(()) => {
            debug_assert!(!buffer.is_null());
            // Will not panic if usize has at least 32 bits, which is true for Chromium
            // targets
            let buffer_size: usize = num_bytes.try_into().unwrap();
            // SAFETY: `buffer` and `buffer_size` were obtained by calling
            // `MojoGetMessageData`, so we trust that they are valid, aligned,
            // readable, etc.
            // The buffer is readable as long as the message is alive, and the type
            // of this function ensures that the lifetime of the resulting reference
            // is the same as that of `message`.
            let buffer_slice = unsafe { std::slice::from_raw_parts(buffer.cast(), buffer_size) };
            return GetMessageDataStatus::Success {
                bytes: buffer_slice,
                num_handles_written: num_handles.try_into().unwrap(),
            };
        }
        Err(MojoError::ResourceExhausted) => {
            // ResourceExhausted indicates that the provided handles buffer wasn't
            // large enough. In this case, `num_handles` has been filled in.
            return GetMessageDataStatus::NotEnoughCapacity { num_handles_attached: num_handles };
        }
        Err(e) => return GetMessageDataStatus::Error(e),
    }
}

/// Tell the underlying mojo system that this message was malformed, which
/// will trigger the error handler which was set up during mojo
/// initialization.
///
/// Typically this will result in the process that created the message being
/// terminated. Note that reporting a bad message does not stop the _current_
/// process from running.
pub fn MojoNotifyBadMessage(message: &MessageHandle, error_msg: &str) {
    // SAFETY: The option pointer is allowed to be be null. The string parts were
    // constructed from an &str, and will not be retained by C code after the
    // function returns.
    let ret = unsafe {
        raw_ffi::MojoNotifyBadMessage(
            message.handle_value.into(),
            error_msg.as_ptr().cast(),
            error_msg.len().try_into().unwrap(),
            std::ptr::null(), // Options pointer
        )
    };
    // This should only fail if the message handle is invalid and we guarantee that
    // it's valid as part of this type.
    debug_assert!(MojoError::result_from_code(ret).is_ok())
}
