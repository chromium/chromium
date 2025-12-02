//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FOR_RELEASE: Probably we should shard the functionality here out into
// sensibly-named modules rather than having them all in `mojo_types`. However,
// for the sake of standing up something simple to start, let's leave it all
// in this file for now.

// FOR_RELEASE: Ensure all uses of `unsafe` have two comments: one on the
// top-level function, and one at the actual `unsafe` block itself. (And make
// sure they describe distinct things: "how to use this function safely" vs
// "why this instance is OK.")

// FOR_RELEASE: Define `enum MojoErrorCode` (**without** `Okay` variant) and
// use `Result<T, MojoErrorCode>` in all public APIs.  Maybe also provide some
// internal convenience helpers and a public
// `pub type MojoResult<T> = std::result::Result<T, MojoErrorCode>`.

use std::ffi::c_void;
use std::fmt;
use std::ptr;

chromium::import! {
  // FOR_RELEASE: everything brought in from mojo_ffi should be pub(crate) at
  // most. We don't want any of these types exposed outside of
  // mojo/public/rust/system.
  pub "//mojo/public/rust:mojo_ffi";
}

use mojo_ffi::types;

/// MojoResult represents anything that can happen as a result of performing
/// some operation in Mojo.
///
/// Its implementation matches that found in the Mojo C API
/// (//mojo/public/c/system/types.h) so this enum can be used across the FFI
/// boundary simply by using "as u32".
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[repr(u32)]
pub enum MojoResult {
    Okay = 0,
    Cancelled = 1,
    Unknown = 2,
    InvalidArgument = 3,
    DeadlineExceeded = 4,
    NotFound = 5,
    AlreadyExists = 6,
    PermissionDenied = 7,
    ResourceExhausted = 8,
    FailedPrecondition = 9,
    Aborted = 10,
    OutOfRange = 11,
    Unimplemented = 12,
    Internal = 13,
    Unavailable = 14,
    DataLoss = 15,
    Busy = 16,
    ShouldWait = 17,
    InvalidResult = 18,
}

impl MojoResult {
    /// Convert a raw u32 code given by the C Mojo functions into a MojoResult.
    // FOR_RELEASE: (https://crbug.com/457917334): Stop hardcoding the numbers
    // below.
    pub(crate) fn from_code(code: mojo_ffi::MojoResultCode) -> MojoResult {
        match code {
            0 => MojoResult::Okay,
            1 => MojoResult::Cancelled,
            2 => MojoResult::Unknown,
            3 => MojoResult::InvalidArgument,
            4 => MojoResult::DeadlineExceeded,
            5 => MojoResult::NotFound,
            6 => MojoResult::AlreadyExists,
            7 => MojoResult::PermissionDenied,
            8 => MojoResult::ResourceExhausted,
            9 => MojoResult::FailedPrecondition,
            10 => MojoResult::Aborted,
            11 => MojoResult::OutOfRange,
            12 => MojoResult::Unimplemented,
            13 => MojoResult::Internal,
            14 => MojoResult::Unavailable,
            15 => MojoResult::DataLoss,
            16 => MojoResult::Busy,
            17 => MojoResult::ShouldWait,
            _ => MojoResult::InvalidResult,
        }
    }

    pub fn as_str(&self) -> &'static str {
        // TODO(https://crbug.com/456535277): Deduplicate MojoResult string
        // definitions across different language APIs.
        match *self {
            MojoResult::Okay => "OK",
            MojoResult::Cancelled => "Cancelled",
            MojoResult::Unknown => "Unknown",
            MojoResult::InvalidArgument => "Invalid Argument",
            MojoResult::DeadlineExceeded => "Deadline Exceeded",
            MojoResult::NotFound => "Not Found",
            MojoResult::AlreadyExists => "Already Exists",
            MojoResult::PermissionDenied => "Permission Denied",
            MojoResult::ResourceExhausted => "Resource Exhausted",
            MojoResult::FailedPrecondition => "Failed Precondition",
            MojoResult::Aborted => "Aborted",
            MojoResult::OutOfRange => "Out Of Range",
            MojoResult::Unimplemented => "Unimplemented",
            MojoResult::Internal => "Internal",
            MojoResult::Unavailable => "Unavailable",
            MojoResult::DataLoss => "Data Loss",
            MojoResult::Busy => "Busy",
            MojoResult::ShouldWait => "Should Wait",
            MojoResult::InvalidResult => "Something went very wrong",
        }
    }
}

impl fmt::Display for MojoResult {
    /// Allow a MojoResult to be displayed in a sane manner.
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    #[repr(transparent)]
    pub struct HandleSignals: types::MojoHandleSignals {
        /// FOR_RELEASE(https://crbug.com/458796903): It'd be nicer to access
        /// MOJO_HANDLE_SIGNAL_READABLE here, but the bindings we
        /// have right now don't export that. We should change that.
        const READABLE = 1 << 0;
        const WRITABLE = 1 << 1;
        const PEER_CLOSED = 1 << 2;
        const NEW_DATA_READABLE = 1 << 3;
        const PEER_REMOTE = 1 << 4;
        const QUOTA_EXCEEDED = 1 << 5;
    }
}

/// Represents the signals state of a handle: which signals are satisfied,
/// and which are satisfiable.
#[repr(transparent)]
#[derive(Clone, Copy, Debug)]
pub struct SignalsState(pub types::MojoHandleSignalsState);

impl SignalsState {
    /// Generates a new SignalsState
    pub fn new(satisfied: HandleSignals, satisfiable: HandleSignals) -> SignalsState {
        SignalsState(types::MojoHandleSignalsState {
            satisfied_signals: satisfied.bits(),
            satisfiable_signals: satisfiable.bits(),
        })
    }

    /// Returns the bitfield of the satisfied signals
    pub fn satisfied(&self) -> HandleSignals {
        HandleSignals::from_bits_truncate(self.0.satisfied_signals)
    }

    /// Returns the bitfield of the satisfiable signals
    pub fn satisfiable(&self) -> HandleSignals {
        HandleSignals::from_bits_truncate(self.0.satisfiable_signals)
    }

    /// Return the wrapped Mojo FFI struct.
    pub fn to_raw(self) -> types::MojoHandleSignalsState {
        self.0
    }

    /// Get a pointer to the inner struct for FFI calls.
    pub fn as_mut_ptr(&mut self) -> *mut types::MojoHandleSignalsState {
        &mut self.0 as *mut _
    }
}

impl std::default::Default for SignalsState {
    fn default() -> Self {
        SignalsState(types::MojoHandleSignalsState { satisfied_signals: 0, satisfiable_signals: 0 })
    }
}

/// The result of `wait`ing on a handle. There are three possible outcomes:
///     * A requested signal was satisfied
///     * A requested signal became unsatisfiable due to a change on the handle
///     * The handle was closed
#[derive(Clone, Copy, Debug)]
pub enum WaitResult {
    /// The handle had a signal satisfied.
    Satisfied(SignalsState),
    /// A requested signal became unsatisfiable for the handle.
    Unsatisfiable(SignalsState),
    /// The handle was closed.
    Closed,
}

/// THE MOJO HANDLE EXTENDED UNIVERSE
///
/// `MojoHandle` always refers to the C type. It is an opaque handle for some
/// Mojo object.
///
/// Users of the Rust system API should never touch MojoHandle directly; they
/// should only interface with the Rust handle types.
///
/// The Rust handle types are
/// * UntypedHandle: Corresponds to some MojoHandle without making any
///   guarantees about what the underlying Mojo object is.
/// * (FOR_RELEASE: Fill out the other Rust handle types!)
///
/// All Rust handles implement the Handle trait.

/// Implementing the Handle trait means we can access the native integer-based
/// MojoHandle underneath.
pub(crate) trait Handle {
    /// Returns the native handle that the structure implementing this trait is
    /// wrapped around.
    fn get_native_handle(&self) -> types::MojoHandle;

    // FOR_RELEASE: Implement query_signals_state.

    // FOR_RELEASE: Implement wait().
}

/// UntypedHandle is the basic handle that wraps a native MojoHandle. It is
/// untyped in that there are no guarantees about what type of Mojo object it
/// holds. Other wrappers can implement `Handle` and `CastHandle` while
/// providing type safety.
///
/// It must hold either a valid `MojoHandle` or be `UntypedHandle::invalid()`
/// (which corresponds to a 0 `MojoHandle`). The handle will be closed on `drop`
/// if it is not `invalid()`.
///
/// FOR_RELEASE: Consider making `UntypedHandle` private: it is necessary for
/// internal functions (some C APIs expect an invalid handle as an argument),
/// but I'm hoping if we construct these primitives properly, nothing in the
/// public Rust API should have to ever touch a handle that could possibly be
/// invalid.
pub struct UntypedHandle {
    // FOR_RELEASE: Let's use NonZeroU32 here and move enforcement of "is this
    // handle valid or not" outside of UntypedHandle (instead this type should
    // just always be valid).
    mojo_handle: types::MojoHandle,
}

impl Handle for UntypedHandle {
    fn get_native_handle(&self) -> types::MojoHandle {
        self.mojo_handle
    }
}

impl UntypedHandle {
    // Get an invalid handle.
    pub fn invalid() -> UntypedHandle {
        UntypedHandle { mojo_handle: 0 }
    }
    pub fn invalidate(&mut self) {
        self.mojo_handle = 0;
    }
    pub fn is_valid(&self) -> bool {
        self.mojo_handle != 0
    }

    /* Safety note for all *_ptr functions:

    The caller must ensure the ptr they are using is not owned by any other
    object.  (UntypedHandles are move-only by default, so the only way this
    could happen would be by e.g. calling one of these functions and using
    the resulting pointer in some unsafe way that would cause multiple
    ownership.)

    To that end, the caller must ensure `self` outlives the returned pointer.

    Finally the caller must never overwrite a ptr to a valid MojoHandle (it will
    be leaked).
    */

    /// Get a mutable pointer to the wrapped `MojoHandle` value. See
    /// "Safety note for all *_ptr functions" above.
    pub fn as_mut_ptr(&mut self) -> *mut types::MojoHandle {
        &mut self.mojo_handle as *mut _
    }

    // FOR RELEASE(https://crbug.com/458499013): We may want these two
    // slice-as-pointer functions to move off UntypedHandle and into their
    // own top level thing, e.g. slice_as_cxx_ptr and whatnot.
    /// Get an immutable pointer to a slice of wrapped `MojoHandle`s. See
    /// "Safety note for all *_ptr functions" above.
    pub fn slice_as_ptr(handles: &[Self]) -> *const types::MojoHandle {
        // Passing nothing must be done explicitly:
        // https://davidben.net/2024/01/15/empty-slices.html
        if handles.len() == 0 {
            return ptr::null();
        }
        // `Self` is a repr(transparent) wrapper for `MojoHandle`, so the
        // pointer cast is sound.
        handles.as_ptr() as *const _
    }

    /// Get a mutable pointer to a slice of wrapped `MojoHandle`s. See "Safety
    /// note for all *_ptr functions" above.
    pub fn slice_as_mut_ptr(handles: &mut [Self]) -> *mut types::MojoHandle {
        // Passing nothing must be done explicitly:
        // https://davidben.net/2024/01/15/empty-slices.html
        if handles.len() == 0 {
            return ptr::null_mut();
        }
        // `Self` is a repr(transparent) wrapper for `MojoHandle`, so the
        // pointer cast is sound.
        handles.as_mut_ptr() as *mut _
    }
}

// FOR_RELEASE: Implement CastHandle.

impl Drop for UntypedHandle {
    /// The destructor for an untyped handle which closes the native handle
    /// it wraps.
    fn drop(&mut self) {
        // SAFETY: Accessing the native handle is unsafe. However, in the C API,
        // it is always possible to close a valid handle, and we check that the
        // handle is valid before closing it.
        unsafe {
            if self.is_valid() {
                mojo_ffi::MojoClose(self.get_native_handle());
            }
        };
    }
}

pub use types::MojoTimeTicks;
pub fn get_time_ticks_now() -> MojoTimeTicks {
    unsafe {
        return mojo_ffi::MojoGetTimeTicksNow();
    }
}

pub struct MessageEndpoint {
    handle: UntypedHandle,
}

impl Handle for MessageEndpoint {
    /// Returns the native handle wrapped by this structure.
    fn get_native_handle(&self) -> types::MojoHandle {
        self.handle.get_native_handle()
    }
}

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
    /// # Safety
    ///
    ///
    /// // FOR_RELEASE: In the v1 version of the Mojo code, we should ensure
    /// this is enforced by typing stronger than UntypedHandle (perhaps by
    /// having the Mojo bindings generator handle this when deriving code
    /// from a Mojom interface?).
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
        let raw_handles_ptr: *const types::MojoHandle = UntypedHandle::slice_as_ptr(&handles);

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

pub fn create_message_pipe() -> Result<(MessageEndpoint, MessageEndpoint), MojoResult> {
    let mut handle0 = UntypedHandle::invalid();
    let mut handle1 = UntypedHandle::invalid();
    let opts = mojo_ffi::MojoCreateMessagePipeOptions::new(0);
    // FOR_RELEASE(https://crbug.com/457918863): Replace this with an into_rust_result helper.
    // SAFETY: This is safe; MojoCreateMessagePipe creates a new pipe
    // and does not keep ownership of either handle.
    match MojoResult::from_code(unsafe {
        mojo_ffi::MojoCreateMessagePipe(opts.as_ptr(), handle0.as_mut_ptr(), handle1.as_mut_ptr())
    }) {
        MojoResult::Okay => {
            Ok((MessageEndpoint { handle: handle0 }, MessageEndpoint { handle: handle1 }))
        }
        e => Err(e),
    }
}
