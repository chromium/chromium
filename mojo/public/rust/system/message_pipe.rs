// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::ptr;
use std::vec;

use std::convert::TryInto;

use crate::ffi;
use crate::handle;
use crate::handle::{CastHandle, Handle};
// This full import is intentional; nearly every type in mojo_types needs to be
// used.
use crate::mojo_types::*;

use ffi::c_void;

use super::UntypedHandle;

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct CreateMessageFlags: u32 {
            /// Do not enforce size restrictions on this message, allowing its serialized
    /// payload to grow arbitrarily large. If this flag is NOT specified, Mojo will
    /// throw an assertion failure at serialization time when the message exceeds a
    /// globally configured maximum size.
    const UNLIMITED_SIZE = 1 << 0;
    }
}

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct AppendMessageFlags: u32 {
    /// If set, this comments the resulting (post-append) message size as the final
    /// size of the message payload, in terms of both bytes and attached handles.
    const COMMIT_SIZE = 1 << 0;
    }
}

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct ReadMessageFlags: u32 {
            /// Ignores attached handles when retrieving message data. This leaves any
    /// attached handles intact and owned by the message object.
        const IGNORE_HANDLES = 1 << 0;
    }
}

/// Creates a message pipe in Mojo and gives back two
/// MessageEndpoints which represent the endpoints of the
/// message pipe
pub fn create() -> Result<(MessageEndpoint, MessageEndpoint), MojoResult> {
    let mut handle0 = UntypedHandle::invalid();
    let mut handle1 = UntypedHandle::invalid();
    let opts = ffi::MojoCreateMessagePipeOptions::new(0);
    match MojoResult::from_code(unsafe {
        ffi::MojoCreateMessagePipe(opts.inner_ptr(), handle0.as_mut_ptr(), handle1.as_mut_ptr())
    }) {
        MojoResult::Okay => {
            Ok((MessageEndpoint { handle: handle0 }, MessageEndpoint { handle: handle1 }))
        }
        e => Err(e),
    }
}

/// Represents the one endpoint of a message pipe.
/// This data structure wraps a handle and acts
/// effectively as a typed handle.
pub struct MessageEndpoint {
    handle: handle::UntypedHandle,
}

impl MessageEndpoint {
    /// Read the next message from the endpoint. Messages in Mojo
    /// are some set of bytes plus a bunch of handles, so we
    /// return both a vector of bytes and a vector of untyped handles.
    ///
    /// Because the handles are untyped, it is up to the user of this
    /// library to know what type the handle actually is and to use
    /// from_untyped in order to convert the handle to the correct type.
    /// This is abstracted away, however, when using the Mojo bindings
    /// generator where you may specify your interface in Mojom.
    ///
    /// If an empty message (that is, it has neither data nor handles)
    /// is received, it will show up as an Err() containing MojoResult::Okay.
    pub fn read(&self) -> Result<(vec::Vec<u8>, vec::Vec<handle::UntypedHandle>), MojoResult> {
        // Read the message, yielding a message object we can copy data from.
        let message_handle = {
            let mut h = 0;
            match MojoResult::from_code(unsafe {
                ffi::MojoReadMessage(
                    self.handle.get_native_handle(),
                    ptr::null(),
                    &mut h as *mut ffi::types::MojoMessageHandle,
                )
            }) {
                MojoResult::Okay => h,
                e => return Err(e),
            }
        };

        let mut buffer: *mut c_void = ptr::null_mut();
        let mut num_bytes: u32 = 0;
        let mut num_handles: u32 = 0;
        let result_prelim = MojoResult::from_code(unsafe {
            ffi::MojoGetMessageData(
                message_handle,
                ptr::null(),
                &mut buffer as *mut _,
                &mut num_bytes as *mut _,
                ptr::null_mut(),
                &mut num_handles as *mut _,
            )
        });
        if result_prelim != MojoResult::Okay && result_prelim != MojoResult::ResourceExhausted {
            return Err(result_prelim);
        }

        let mut handles: vec::Vec<UntypedHandle> = vec::Vec::with_capacity(num_handles as usize);
        if num_handles > 0 {
            let result = MojoResult::from_code(unsafe {
                ffi::MojoGetMessageData(
                    message_handle,
                    ptr::null(),
                    &mut buffer as *mut _,
                    &mut num_bytes as *mut _,
                    UntypedHandle::slice_as_mut_ptr(&mut handles),
                    &mut num_handles as *mut _,
                )
            });
            if result != MojoResult::Okay {
                return Err(result);
            }
        }

        let data: Vec<u8> = if num_bytes > 0 {
            assert_ne!(buffer, ptr::null_mut());
            // Will not panic if usize has at least 32 bits, which is true for our targets
            let buffer_size: usize = num_bytes.try_into().unwrap();
            // MojoGetMessageData points us to the data with a c_void pointer and a length.
            // This is only available until we destroy the message. We want to
            // copy this into our own Vec. Read the buffer as a slice, which is
            // safe.
            unsafe {
                let buffer_slice = std::slice::from_raw_parts(buffer.cast(), buffer_size);
                buffer_slice.to_vec()
            }
        } else {
            Vec::new()
        };

        unsafe {
            ffi::MojoDestroyMessage(message_handle);
        }

        Ok((data, handles))
    }

    /// Write a message to the endpoint. Messages in Mojo
    /// are some set of bytes plus a bunch of handles, so we
    /// return both a vector of bytes and a vector of untyped handles.
    ///
    /// Because the handles are untyped, it is up to the user of this
    /// library to know what type the handle actually is and to use
    /// from_untyped in order to convert the handle to the correct type.
    /// This is abstracted away, however, when using the Mojo bindings
    /// generator where you may specify your interface in Mojom.
    ///
    /// Additionally, the handles passed in are consumed. This is because
    /// Mojo handles operate on move semantics much like Rust data types.
    /// When a handle is sent through a message pipe it is invalidated and
    /// may not even be represented by the same integer on the other side,
    /// so care must be taken to design your application with this in mind.
    pub fn write(&self, bytes: &[u8], handles: vec::Vec<handle::UntypedHandle>) -> MojoResult {
        // Create the message object we will write data into then send.
        let message_handle = unsafe {
            let mut h = 0;
            let result_code = ffi::MojoCreateMessage(std::ptr::null(), &mut h as *mut _);
            assert_eq!(MojoResult::Okay, MojoResult::from_code(result_code));
            h
        };

        // "Append" to the message, getting a buffer to copy our data to.
        let raw_handles_ptr: *const MojoHandle =
            if handles.len() == 0 { ptr::null() } else { UntypedHandle::slice_as_ptr(&handles) };

        let mut buffer_ptr: *mut c_void = std::ptr::null_mut();
        let mut buffer_size: u32 = 0;

        let append_message_options =
            ffi::MojoAppendMessageDataOptions::new(AppendMessageFlags::COMMIT_SIZE.bits());

        let result = MojoResult::from_code(unsafe {
            ffi::MojoAppendMessageData(
                message_handle,
                bytes.len() as u32,
                raw_handles_ptr,
                handles.len() as u32,
                append_message_options.inner_ptr(),
                &mut buffer_ptr as *mut _,
                &mut buffer_size as *mut _,
            )
        });

        if result != MojoResult::Okay {
            return result;
        }

        // Copy into the message storage
        if bytes.len() > 0 {
            // Will not panic if usize has at least 32 bits, which is true for our targets
            let buffer_size: usize = buffer_size.try_into().unwrap();
            assert!(bytes.len() <= buffer_size);
            assert_ne!(buffer_ptr, ptr::null_mut());
            // MojoAppendMessageData tells us where to write with a c_void pointer and a
            // length. This is only available until we destroy or send the
            // message. We can view this through a slice and copy our `bytes`
            // into it.
            unsafe {
                // We know `bytes.len() <= buffer_size`, and `buffer_size` is the limit of the
                // provided buffer.
                let buffer_slice = std::slice::from_raw_parts_mut(buffer_ptr.cast(), bytes.len());
                buffer_slice.copy_from_slice(bytes);
            }
        }

        // Send the message. This takes ownership of the message object.
        let write_message_options = ffi::MojoWriteMessageOptions::new(0);
        return MojoResult::from_code(unsafe {
            ffi::MojoWriteMessage(
                self.handle.get_native_handle(),
                message_handle,
                write_message_options.inner_ptr(),
            )
        });
    }
}

impl CastHandle for MessageEndpoint {
    /// Generates a MessageEndpoint from an untyped handle wrapper
    /// See crate::handle for information on untyped vs. typed
    unsafe fn from_untyped(handle: handle::UntypedHandle) -> Self {
        MessageEndpoint { handle: handle }
    }

    /// Consumes this object and produces a plain handle wrapper
    /// See crate::handle for information on untyped vs. typed
    fn as_untyped(self) -> handle::UntypedHandle {
        self.handle
    }
}

impl Handle for MessageEndpoint {
    /// Returns the native handle wrapped by this structure.
    ///
    /// See crate::handle for information on handle wrappers
    fn get_native_handle(&self) -> MojoHandle {
        self.handle.get_native_handle()
    }
}
