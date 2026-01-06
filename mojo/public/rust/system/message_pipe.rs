//Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::mojo_types::{Handle, MojoResult, Trappable, UntypedHandle};
use mojo_ffi::types::MojoHandle;
use std::ffi::c_void;
use std::ptr;

chromium::import! {
  pub "//mojo/public/rust:mojo_ffi";
}

pub struct MessageEndpoint {
    handle: UntypedHandle,
}

impl Handle for MessageEndpoint {
    /// Returns the native handle wrapped by this structure.
    fn get_native_handle(&self) -> MojoHandle {
        self.handle.get_native_handle()
    }
}

impl Trappable for MessageEndpoint {}

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct AppendMessageFlags: u32 {
        /// If set, this comments the resulting (post-append) message size as the final
        /// size of the message payload, in terms of both bytes and attached handles.
        /// FOR_RELEASE(https://crbug.com/458796903): It'd be nicer to access
        /// MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE here, but the bindings we
        /// have right now don't export that. We should change that.
        const COMMIT_SIZE = 1 << 0;
    }
}

impl MessageEndpoint {
    /// Read the next message from the endpoint. Messages in Mojo
    /// are some set of bytes plus a bunch of handles, so we
    /// return both a vector of bytes and a vector of untyped handles.
    ///
    /// Because the handles are untyped, it is up to the user of this
    /// library to know what type the handle actually is and to use
    /// from_untyped in order to convert the handle to the correct type.
    ///
    /// // FOR_RELEASE: In the v1 version of the Mojo code, we should ensure
    /// this is enforced by typing stronger than UntypedHandle (perhaps by
    /// having the Mojo bindings generator handle this when deriving code
    /// from a Mojom interface?).
    pub fn create_pipe() -> Result<(MessageEndpoint, MessageEndpoint), MojoResult> {
        let mut handle0 = UntypedHandle::invalid();
        let mut handle1 = UntypedHandle::invalid();
        let opts = mojo_ffi::MojoCreateMessagePipeOptions::new(0);
        // FOR_RELEASE(https://crbug.com/457918863): Replace this with an into_rust_result helper.
        // SAFETY: This is safe; MojoCreateMessagePipe creates a new pipe
        // and does not keep ownership of either handle.
        match MojoResult::from_code(unsafe {
            mojo_ffi::MojoCreateMessagePipe(
                opts.as_ptr(),
                handle0.as_mut_ptr(),
                handle1.as_mut_ptr(),
            )
        }) {
            MojoResult::Okay => {
                Ok((MessageEndpoint { handle: handle0 }, MessageEndpoint { handle: handle1 }))
            }
            e => Err(e),
        }
    }

    pub fn read(&self) -> Result<(Vec<u8>, Vec<UntypedHandle>), MojoResult> {
        // Read the message, yielding a message object we can copy data from.
        let message_handle = {
            // FOR_RELEASE: Even though `message_handle` is temporary and
            // self-contained, probably better to encapsulate it in a MessageHandle
            // type once we have one.
            let mut h: mojo_ffi::types::MojoMessageHandle = 0;
            // SAFETY: We have just created the message handle here, so we know
            // it's safe to write to it via the underlying MojoReadMessage.
            match MojoResult::from_code(unsafe {
                mojo_ffi::MojoReadMessage(
                    self.handle.get_native_handle(),
                    ptr::null(),
                    &mut h as *mut _,
                )
            }) {
                MojoResult::Okay => h,
                e => return Err(e),
            }
        };

        let mut buffer: *mut c_void = ptr::null_mut();
        let mut num_bytes: u32 = 0;
        let mut num_handles: u32 = 0;
        // SAFETY: We just read a message into `message_handle` above.
        // We've just initialized buffer and num_bytes, so they're safe to
        // be written to by MojoGetMessageData.
        // The null_mut and 0 num_handles indicates no handles will be received.
        let result_prelim = MojoResult::from_code(unsafe {
            mojo_ffi::MojoGetMessageData(
                message_handle,
                ptr::null(),
                &mut buffer as *mut _,
                &mut num_bytes as *mut _,
                ptr::null_mut(),
                &mut num_handles as *mut _,
            )
        });
        if result_prelim != MojoResult::Okay {
            return Err(result_prelim);
        }

        let mut handles: Vec<UntypedHandle> = Vec::with_capacity(num_handles as usize);
        if num_handles > 0 {
            // SAFETY: This requires passing various mutable values (a buffer,
            // a num_bytes, etc) to the underlying C function, so the message data
            // can be written to the specified location.
            let result = MojoResult::from_code(unsafe {
                mojo_ffi::MojoGetMessageData(
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
            // Will not panic if usize has at least 32 bits, which is true for Chromium
            // targets
            let buffer_size: usize = num_bytes.try_into().unwrap();
            // SAFETY: Depending on `MojoGetMessageData` to return
            // `buffer` and `buffer_size` that points to a readable slice of
            // memory.  The slice lifetime is until the end of this scope, so
            // shorter than the lifetime of `message_handle`.
            // `cast()` between `*const c_void` and `*const u8` is safe
            // (e.g. wrt alignment, allowed bit patterns, etc.).
            let buffer_slice = unsafe { std::slice::from_raw_parts(buffer.cast(), buffer_size) };
            buffer_slice.to_vec()
        } else {
            Vec::new()
        };

        unsafe {
            // SAFETY: No other references to message_handle exist & this one
            // will be destroyed at function end so this is safe.
            mojo_ffi::MojoDestroyMessage(message_handle);
        }

        Ok((data, handles))
    }

    // FOR_RELEASE: Return a Result<(), Error> instead of MojoResult here.
    pub fn write(&self, bytes: &[u8], handles: Vec<UntypedHandle>) -> MojoResult {
        // Create the message object we will write data into then send.
        // In Mojo, a message is a set of bytes plus some number of handles.

        // FOR_RELEASE: Even though `message_handle` is temporary and
        // self-contained, probably better to encapsulate it in a MessageHandle
        // type once we have one.
        let message_handle = {
            let mut mojohandle = 0;
            // SAFETY: MojoCreateMessage allows the first argument to be null.
            // Second argument is a pointer to a local variable on the stack.
            let result_code: u32 =
                unsafe { mojo_ffi::MojoCreateMessage(std::ptr::null(), &mut mojohandle as *mut _) };
            assert_eq!(MojoResult::Okay, MojoResult::from_code(result_code));
            mojohandle
        };

        // "Append" to the message, getting a buffer to copy Chromium's data to.
        let raw_handles_ptr: *const MojoHandle = UntypedHandle::slice_as_ptr(&handles);

        let mut buffer_ptr: *mut c_void = std::ptr::null_mut();
        let mut buffer_size: u32 = 0;

        let append_message_options =
            mojo_ffi::MojoAppendMessageDataOptions::new(AppendMessageFlags::COMMIT_SIZE.bits());

        // SAFETY: MojoAppendMessageData consumes the data pointed to by
        // raw_handles_ptr and writes the output to buffer_ptr.
        let result = MojoResult::from_code(unsafe {
            mojo_ffi::MojoAppendMessageData(
                message_handle,
                bytes.len().try_into().unwrap(),
                raw_handles_ptr,
                handles.len().try_into().unwrap(),
                append_message_options.as_ptr(),
                &mut buffer_ptr as *mut _,
                &mut buffer_size as *mut _,
            )
        });

        if result != MojoResult::Okay {
            return result;
        }

        // If MojoAppendMessageData succeeds, ownership of all handles are transferred
        // to the C object, so prevent Rust from double-free-ing them.
        // FOR_RELEASE: This is a slightly awkward place to `forget`.
        // Consider if there's some nicer way to encapsulate/force
        // "ManuallyDrop every time we pass handles to [C function du jour]"
        for handle in handles.into_iter() {
            std::mem::forget(handle);
        }

        // Copy into the message storage
        if bytes.len() > 0 {
            // Will not panic if usize has at least 32 bits, which is true for Chromium
            // targets
            let buffer_size: usize = buffer_size.try_into().unwrap();
            assert!(bytes.len() <= buffer_size);
            assert_ne!(buffer_ptr, ptr::null_mut());

            let buffer_slice: &mut [u8];
            // SAFETY: MojoAppendMessageData tells us where to write with a
            // c_void pointer and a length. This is only available until we
            // destroy or send the message. We can view this through a slice and
            // copy Chromium's `bytes` into it.
            unsafe {
                // SAFETY: We know `bytes.len() <= buffer_size`, and
                // `buffer_size` is the limit of the provided buffer.
                buffer_slice = std::slice::from_raw_parts_mut(buffer_ptr.cast(), bytes.len());
            }
            buffer_slice.copy_from_slice(bytes);
        }

        // Send the message. This transfers ownership of the message_handle
        // object to the receiving process.
        let write_message_options = mojo_ffi::MojoWriteMessageOptions::new(0);
        return MojoResult::from_code(unsafe {
            // SAFETY: message_handle and write_message_options were created
            // solely within this function and thus we will lose ownership of
            // them when the function returns—which is what we want; ownership
            // transfers to the receiver at that point. However FOR_RELEASE
            // we may want to wrap these in Rust handle types to offer stronger
            // guarantees of ownership whenever we handle things of this shape.
            mojo_ffi::MojoWriteMessage(
                self.handle.get_native_handle(),
                message_handle,
                write_message_options.as_ptr(),
            )
        });
    }
}
