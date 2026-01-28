//Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::mojo_types::{Handle, MojoResult, Trappable, UntypedHandle};
use mojo_ffi::types::{MojoHandle, MojoMessageHandle};
use std::ffi::c_void;
use std::ptr;

chromium::import! {
  pub "//mojo/public/rust:mojo_ffi";
}

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct GetMessageDataFlags: u32 {
        /// If set, this read ignores handles; it doesn't read or write the handle-related
        /// arguments and doesn't fail if handles have already been extracted.
        /// FOR_RELEASE(https://crbug.com/458796903): It'd be nicer to access
        /// MOJO_GET_MESSAGE_DATA_FLAG_IGNORE_HANDLES  here, but the bindings we
        /// have right now don't export that. We should change that.
        const IGNORE_HANDLES = 1 << 0;
    }
}

/// An object representing a Mojo message that can be (or has been) sent through
/// a pipe. This type is not thread-safe, and must be used from a single thread
/// at a time.
///
/// Logically, the message has two components: a payload composed of raw bytes,
/// and zero or more untyped mojo handles which are sent alongside the payload.
/// These components can be accessed and manipulated using functions on the
/// message.
///
/// FOR_RELEASE: This is defined in message_pipe.rs so that the write and read
/// functions can have direct access to the contents (e.g. to construct one from
/// an incoming message without calling `new`). It might be nicer for it to be
/// in a different file though if we can get the visibility to work out.
pub struct RawMojoMessage {
    message_handle: MojoMessageHandle,
    // This member is equivalent to `impl !Sync`, which is currently unstable
    _phantom_unsync: std::marker::PhantomData<std::cell::Cell<()>>,
}

/// Dropping a message handle means we need to tell the underlying mojo system
/// to clean it up. Note that sending a message does not and should not drop it.
impl Drop for RawMojoMessage {
    fn drop(&mut self) {
        // SAFETY: The handle is still alive because we're in the middle of dropping it
        let result: MojoResult =
            unsafe { mojo_ffi::MojoDestroyMessage(self.message_handle).into() };
        debug_assert!(result == MojoResult::Okay);
    }
}

impl RawMojoMessage {
    pub fn new() -> Self {
        let mut message_handle: MojoMessageHandle = 0;
        // SAFETY: MojoCreateMessage allows the first argument to be null.
        // Second argument is a pointer to a local variable on the stack.
        let result: MojoResult =
            unsafe { mojo_ffi::MojoCreateMessage(std::ptr::null(), &mut message_handle as *mut _) }
                .into();
        assert_eq!(MojoResult::Okay, result);
        RawMojoMessage { message_handle, _phantom_unsync: std::marker::PhantomData }
    }

    /// Construct a new message with the specified contents. If this operation
    /// succeeds, ownership of the handles is transferred to the message object;
    /// otherwise, they are returned as part of the result.
    pub fn new_with_data(
        bytes: &[u8],
        handles: Vec<UntypedHandle>,
    ) -> Result<Self, (MojoResult, Vec<UntypedHandle>)> {
        let mut msg = Self::new();
        msg.append_data(bytes, handles).map(|_| msg)
    }

    /// Construct a new message with the given bytes as its payload, and no
    /// associated handles.
    pub fn new_with_bytes(bytes: &[u8]) -> Result<Self, MojoResult> {
        let mut msg = Self::new();
        msg.append_bytes(bytes).map(|_| msg)
    }

    /// Append the bytes in `bytes` and the handles in `handles` to the
    /// message's payload. Ownership of the handles is transferred to the
    /// other side of the pipe if the operation is successful; otherwise,
    /// the handles are retuned to the caller as part of the Result.
    ///
    /// FOR_RELEASE: Figure out if we should just panic instead of returning a
    /// result here. We may be able to guarantee that the only failure
    /// condition is e.g. running out of memory, which isn't really recoverable
    /// anyway, so might as well spare our users the need to unwrap.
    pub fn append_data(
        &mut self,
        bytes: &[u8],
        handles: Vec<UntypedHandle>,
    ) -> Result<(), (MojoResult, Vec<UntypedHandle>)> {
        let raw_handles_ptr: *const MojoHandle = UntypedHandle::slice_as_ptr(&handles);
        let mut buffer_ptr: *mut c_void = std::ptr::null_mut();
        let mut buffer_size: u32 = 0;

        // SAFETY: MojoAppendMessageData consumes the data pointed to by
        // raw_handles_ptr and writes the output to buffer_ptr. The options
        // ptr is permitted to be null.
        let result = MojoResult::from_code(unsafe {
            mojo_ffi::MojoAppendMessageData(
                self.message_handle,
                bytes.len().try_into().unwrap(),
                raw_handles_ptr,
                handles.len().try_into().unwrap(),
                std::ptr::null(), // Options ptr
                &mut buffer_ptr as *mut _,
                &mut buffer_size as *mut _,
            )
        });

        if result != MojoResult::Okay {
            return Err((result, handles));
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

            // FOR_RELEASE: I'm pretty sure these are guaranteed by the mojo call
            debug_assert!(bytes.len() <= buffer_size);
            debug_assert_ne!(buffer_ptr, ptr::null_mut());

            let buffer_slice: &mut [u8] =
            // SAFETY: MojoAppendMessageData tells us where to write with a
            // c_void pointer and a length. This is only available until we
            // destroy or send the message. We can view this through a slice and
            // copy Chromium's `bytes` into it.
            unsafe {
                // FOR_RELEASE: the mojo docs are unclear whether buffer_ptr
                // points to the beginning of the buffer or the point where
                // we should begin writing; it's probably the latter, but
                // we should verify this.

                // SAFETY: We know `bytes.len() <= buffer_size`, and
                // `buffer_size` is the limit of the provided buffer.
                std::slice::from_raw_parts_mut(buffer_ptr.cast(), bytes.len())
            };
            buffer_slice.copy_from_slice(bytes);
        };

        Ok(())
    }

    /// Append bytes to the message's payload.
    pub fn append_bytes(&mut self, bytes: &[u8]) -> Result<(), MojoResult> {
        self.append_data(bytes, Vec::new()).map_err(|(result, _)| result)
    }

    /// Append untyped handles to the message's payload. Ownership of the
    /// handles is transferred to the message only if the operation succeeds.
    pub fn append_handles(
        &mut self,
        handles: Vec<UntypedHandle>,
    ) -> Result<(), (MojoResult, Vec<UntypedHandle>)> {
        self.append_data(&[], handles)
    }

    /// Helper function for the `read_data` and `read_bytes`. Wraps the given
    /// buffer in a slice of the appropriate length with the same lifetime as
    /// `self`.
    ///
    /// SAFETY: The arguments must have been filled in by a call to
    /// `MojoGetMessage` using the `self.message_handle`.
    unsafe fn wrap_slice<'a>(&'a self, buffer: *mut c_void, num_bytes: u32) -> &'a [u8] {
        if num_bytes > 0 {
            assert_ne!(buffer, ptr::null_mut());
            // Will not panic if usize has at least 32 bits, which is true for Chromium
            // targets
            let buffer_size: usize = num_bytes.try_into().unwrap();
            // SAFETY: `buffer` and `buffer_size` were obtained by calling
            // `MojoGetMessageData`, so we trust that they are valid, aligned,
            // readable, etc.
            // The buffer is readable as long as the message is alive, and the type of this
            // function ensures that the lifetime of the resulting reference is the
            // same as &self.
            // `cast()` between `*const c_void` and `*const u8`
            // is safe (e.g. wrt alignment, allowed bit patterns, etc.).
            let buffer_slice = unsafe { std::slice::from_raw_parts(buffer.cast(), buffer_size) };
            buffer_slice
        } else {
            &[]
        }
    }

    /// Retrieve the payload, if present, and attached handles from this message
    /// object.
    ///
    /// Because this function transfers ownership of the handles to the caller,
    /// it may only be called ONCE per message object. Any further calls will
    /// return MOJO_RESULT_NOT_FOUND.
    ///
    /// If you need to read only the payload, use `read_bytes` instead,
    /// which does not have this restriction and does not prevent `read_data`
    /// function from being called later.
    ///
    /// FOR_RELEASE: See if we can avoid returning a MojoResult here by handling
    /// error cases internally (i.e. panicking)
    pub fn read_data(&self) -> Result<(&[u8], Vec<UntypedHandle>), MojoResult> {
        let mut buffer: *mut c_void = ptr::null_mut();
        let mut num_bytes: u32 = 0;
        let mut num_handles: u32 = 0;
        // SAFETY: `options` may always be null, and `handles` may be null because
        // `*num_handles` is 0.
        let result_prelim = MojoResult::from_code(unsafe {
            mojo_ffi::MojoGetMessageData(
                self.message_handle,
                ptr::null(), // Options pointer
                &mut buffer as *mut _,
                &mut num_bytes as *mut _,
                ptr::null_mut(), // Handles pointer
                &mut num_handles as *mut _,
            )
        });
        // ResourceExhausted indicates that there are still handles attached, which is
        // expected because we didn't yet provide memory to copy them to.
        if result_prelim != MojoResult::Okay && result_prelim != MojoResult::ResourceExhausted {
            return Err(result_prelim);
        }

        // Copy the handles from the message into a vector. This prevents further
        // reads of the handles.
        let mut handles: Vec<UntypedHandle> = Vec::with_capacity(num_handles as usize);
        if num_handles > 0 {
            // SAFETY: The options pointer may be null. `handles` has enough
            // capacity to hold `num_handles` handles, and its pointer is valid.
            let result = MojoResult::from_code(unsafe {
                mojo_ffi::MojoGetMessageData(
                    self.message_handle,
                    ptr::null(), // Options pointer
                    &mut buffer as *mut _,
                    &mut num_bytes as *mut _,
                    handles.as_mut_ptr() as *mut _,
                    &mut num_handles as *mut _,
                )
            });

            // SAFETY: The capacity is exactly `num_handles`, and `MojoGetMessageData`
            // just initialized that many elements.
            unsafe { handles.set_len(num_handles as usize) };

            if result != MojoResult::Okay {
                return Err(result);
            }
        }

        // SAFETY: `buffer` and `num_bytes` were filled in by
        // `MojoGetMessageData(self.message_handle, ...)`
        let data = unsafe { self.wrap_slice(buffer, num_bytes) };

        Ok((data, handles))
    }

    /// Read just the bytes of this message's payload, ignoring any attached
    /// handles.
    ///
    /// FOR_RELEASE: See if we can avoid returning a MojoResult here by handling
    /// error cases internally (i.e. panicking)
    pub fn read_bytes(&self) -> Result<&[u8], MojoResult> {
        let mut buffer: *mut c_void = ptr::null_mut();
        let mut num_bytes: u32 = 0;
        let options =
            mojo_ffi::MojoGetMessageDataOptions::new(GetMessageDataFlags::IGNORE_HANDLES.bits());

        // SAFETY: `num_handles` and `handles` may be null because we're passing the
        // IGNORE_HANDLES flag.
        let result_prelim = MojoResult::from_code(unsafe {
            mojo_ffi::MojoGetMessageData(
                self.message_handle,
                options.as_ptr(),
                &mut buffer as *mut _,
                &mut num_bytes as *mut _,
                ptr::null_mut(), // Handles pointer
                ptr::null_mut(), // Num_handles pointer
            )
        });
        if result_prelim != MojoResult::Okay {
            return Err(result_prelim);
        }

        // SAFETY: `buffer` and `num_bytes` were filled in by
        // `MojoGetMessageData(self.message_handle, ...)`
        let data = unsafe { self.wrap_slice(buffer, num_bytes) };

        Ok(data)
    }

    /// Tell the underlying mojo system that this message was malformed, which
    /// will trigger the error handler which was set up during mojo
    /// initialization. Typically this will result in the process that
    /// created the message being terminated.
    pub fn report_bad_message(&self) {
        todo!()
    }
}

/// One end of a message pipe
pub struct MessageEndpoint {
    handle: UntypedHandle,
}

impl Handle for MessageEndpoint {
    /// Returns the native handle wrapped by this structure.
    fn get_native_handle(&self) -> MojoHandle {
        self.handle.get_native_handle()
    }

    fn from_untyped(handle: UntypedHandle) -> Self {
        Self { handle }
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
    /// Create a new pair of endpoints corresponding to a new mojo message pipe.
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

    /// Read the next message from the endpoint, if one exists.
    /// FOR_RELEASE: Catalogue the possible MojoResults here.
    pub fn read(&self) -> Result<RawMojoMessage, MojoResult> {
        // Read the message, yielding a message object we can copy data from.
        let message_handle: mojo_ffi::types::MojoMessageHandle = {
            let mut h = 0;
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

        Ok(RawMojoMessage { message_handle, _phantom_unsync: std::marker::PhantomData })
    }

    /// Write the given message to the pipe, sending it to the other side.
    /// FOR_RELEASE: Return a Result<(), Error> instead of MojoResult here.
    pub fn write(&self, msg: RawMojoMessage) -> Result<(), MojoResult> {
        // First we need to do one last append operation to "commit" the message,
        // which finalizes it so it can be send through the pipe.
        let append_message_options =
            mojo_ffi::MojoAppendMessageDataOptions::new(AppendMessageFlags::COMMIT_SIZE.bits());

        // SAFETY: The number of handles is 0, so the handle ptr may be null.
        // The other pointers may always be null.
        let result = MojoResult::from_code(unsafe {
            mojo_ffi::MojoAppendMessageData(
                msg.message_handle,
                0,                // Number of bytes we want to write
                std::ptr::null(), // Handle ptr
                0,                // Number of handles
                append_message_options.as_ptr(),
                std::ptr::null_mut(), // Buffer ptr
                std::ptr::null_mut(), // Buffer size ptr
            )
        });

        if result != MojoResult::Okay {
            return Err(result);
        }

        // Send the message. This transfers ownership of the message_handle
        // object to the receiving process.
        // SAFETY: All handles are alive; the options ptr may be null.
        let result = MojoResult::from_code(unsafe {
            mojo_ffi::MojoWriteMessage(
                self.handle.get_native_handle(),
                msg.message_handle,
                std::ptr::null(), // Options ptr
            )
        });

        // This message was sent, so ownership of the handle has been transferred to the
        // recipient.
        std::mem::forget(msg);

        if result != MojoResult::Okay {
            return Err(result);
        } else {
            return Ok(());
        }
    }
}
