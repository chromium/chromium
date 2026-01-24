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

impl From<mojo_ffi::MojoResultCode> for MojoResult {
    fn from(code: mojo_ffi::MojoResultCode) -> Self {
        MojoResult::from_code(code)
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

impl HandleSignals {
    /// Check if the readable flag is set.
    pub fn is_readable(&self) -> bool {
        self.contains(HandleSignals::READABLE)
    }

    /// Check if the writable flag is set.
    pub fn is_writable(&self) -> bool {
        self.contains(HandleSignals::WRITABLE)
    }

    /// Check if the peer-closed flag is set.
    pub fn is_peer_closed(&self) -> bool {
        self.contains(HandleSignals::PEER_CLOSED)
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

/// Implementing the Handle trait means we can access the native integer-based
/// MojoHandle underneath.
///
/// FOR_RELEASE: Revisit if we want to limit the visibility of Handle.
/// In particular: while mostly we want the underlying get_native_handle to be
/// opaque, Traps really require access to the underlying value to work, which
/// means this trait needs to be public. One way to handle this may be by
/// "sealing" the trait: https://predr.ag/blog/definitive-guide-to-sealed-traits-in-rust/
///
/// # The `MojoHandle` Extended Universe
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
pub trait Handle {
    /// Returns the native handle that the structure implementing this trait is
    /// wrapped around.
    fn get_native_handle(&self) -> types::MojoHandle;

    // FOR_RELEASE: Implement query_signals_state.

    // FOR_RELEASE: Implement wait().
}

// This trait is only defined on Handles that are trappable.
// FOR_RELEASE: We may want to refactor this as a "sealed" trait to better
// control visibility. See https://predr.ag/blog/definitive-guide-to-sealed-traits-in-rust/
pub trait Trappable: Handle {}

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
#[repr(transparent)]
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
        if handles.is_empty() {
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
        if handles.is_empty() {
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
    unsafe { mojo_ffi::MojoGetTimeTicksNow() }
}
