// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! The way Mojo handles are handled in Rust is very similar
//! to Go, though more type-safe. Here we define an "untyped"
//! handle which means that what the handle actually represents
//! is unknown. This is the basic handle wrapper, and it handles
//! closing the handle once the wrapper goes out of scope, therefore
//! preventing any resources from being leaked. "Typed" handles
//! are MessageEndpoints or Consumers/Producers in this library
//! and they represent handles which represent parts of message pipes,
//! data pipes, and shared buffers. Typed handles wrap untyped handles
//! but act much the same as untyped handles.

use crate::ffi;
use crate::wait::*;

// This full import is intentional; nearly every type in mojo_types needs to be
// used.
use crate::mojo_types::*;

/// The CastHandle trait defines an interface to convert between
/// typed and untyped handles. These are only used internally for
/// typed handles.
pub trait CastHandle {
    /// Passes an ownership of an untyped handle and produces a
    /// typed handle which owns that untyped handle
    ///
    /// Casting to a typed handle is unsafe because the handle may
    /// not necessarily be a handle to the typed primitive being
    /// casted to internally in Mojo
    unsafe fn from_untyped(handle: UntypedHandle) -> Self;

    /// Consumes a typed handle and releases ownership of the
    /// untyped handle it owned
    fn as_untyped(self) -> UntypedHandle;
}

/// The Handle trait simply means that we can extract
/// the native integer-based Mojo handle from the typed handle.
pub trait Handle {
    /// Returns the native handle wrapped by whatever structure
    /// implements this trait.
    fn get_native_handle(&self) -> MojoHandle;

    /// Waits on the handle wrapped in the current struct until the signals
    /// declared in 'signals' are triggered.
    ///
    /// Returns the satisfied and satisfiable signals respectively for this
    /// handle when waiting is done.
    fn wait(&self, signals: HandleSignals) -> WaitResult {
        wait(self.get_native_handle(), signals)
    }

    /// Gets the last known signals state of the handle. The state may change at
    /// any time during or after this call.
    fn query_signals_state(&self) -> Result<SignalsState, MojoResult> {
        let mut state: SignalsState = Default::default();
        let r = MojoResult::from_code(unsafe {
            ffi::MojoQueryHandleSignalsState(self.get_native_handle(), state.as_raw_mut_ptr())
        });

        match r {
            MojoResult::Okay => Ok(state),
            r => Err(r),
        }
    }
}

/// The basic untyped handle that wraps a MojoHandle. It is "untyped" in the
/// sense that there are no guarantees about what type of Mojo object it holds.
/// Other Mojo wrappers can implement `Handle` and `CastHandle` while providing
/// type safety.
///
/// `UntypedHandle` must hold either a valid `MojoHandle` or be
/// `UntypedHandle::invalid()` (i.e. a 0 `MojoHandle`). The handle will be
/// closed on `drop` if it is not `invalid()`.
#[derive(Debug)]
#[repr(transparent)]
pub struct UntypedHandle {
    /// The native Mojo handle.
    value: MojoHandle,
}

impl UntypedHandle {
    /// Get an invalid handle.
    pub fn invalid() -> UntypedHandle {
        UntypedHandle { value: 0 }
    }

    /// Invalidates the Handle by setting its native handle to
    /// zero, the canonical invalid handle in Mojo.
    ///
    /// Using this improperly will leak Mojo resources.
    pub fn invalidate(&mut self) {
        self.value = 0;
    }

    /// Checks if the native handle is valid (0 = canonical invalid handle).
    pub fn is_valid(&self) -> bool {
        self.value != 0
    }

    /// Get a pointer to the wrapped `MojoHandle` value. Use with care: if a
    /// valid handle is overwritten, it will be leaked. This method is unsafe
    /// because writing a valid handle value owned by another `UntypedHandle`
    /// instance can cause undefined behavior. Furthermore, an arbitrary
    /// non-zero value may at any time become a valid handle owned somewhere
    /// else.
    ///
    /// # Safety
    ///
    /// The caller must ensure a `MojoHandle` stored here is either 0 or a valid
    /// handle returned from Mojo. Additionally, the handle must not be owned by
    /// any other instance.
    ///
    /// The caller must ensure `self` outlives the returned pointer.
    pub unsafe fn as_mut_ptr(&mut self) -> *mut MojoHandle {
        &mut self.value as *mut _
    }

    /// Get an immutable pointer to a slice of wrapped handle.
    ///
    /// The caller must ensure `handles` outlives the returned pointer. The
    /// handle must not be closed or wrapped by another instance.
    pub fn slice_as_ptr(handles: &[Self]) -> *const MojoHandle {
        // `Self` is a repr(transparent) wrapper for `MojoHandle`, so the
        // pointer cast is sound.
        handles.as_ptr() as *const _
    }

    /// Get a mutable pointer to a slice of wrapped handles. Comes with the same
    /// caveats as `as_mut_ptr()`.
    ///
    /// # Safety
    ///
    /// The caller must ensure that *all* stored `MojoHandle`s meet the
    /// requirements of `as_mut_ptr()`. It follows that all handles must be
    /// unique or 0.
    ///
    /// The caller must ensure `handles` outlives the returned pointer.
    pub unsafe fn slice_as_mut_ptr(handles: &mut [Self]) -> *mut MojoHandle {
        // `Self` is a repr(transparent) wrapper for `MojoHandle`, so the
        // pointer cast is sound.
        handles.as_mut_ptr() as *mut _
    }
}

impl Handle for UntypedHandle {
    /// Pulls out a copy of the native handle wrapped by this structure.
    fn get_native_handle(&self) -> MojoHandle {
        self.value
    }
}

impl CastHandle for UntypedHandle {
    /// Casting an untyped handle is a no-op, but we include
    /// this to eliminate code duplication.
    unsafe fn from_untyped(handle: UntypedHandle) -> Self {
        handle
    }

    /// Casting to an untyped handle is a no-op, but we include
    /// this to eliminate code duplication.
    fn as_untyped(self) -> UntypedHandle {
        self
    }
}

impl Drop for UntypedHandle {
    /// The destructor for an untyped handle which closes the native handle
    /// it wraps.
    fn drop(&mut self) {
        if self.is_valid() {
            let result = MojoResult::from_code(unsafe { ffi::MojoClose(self.get_native_handle()) });
            if result != MojoResult::Okay {
                panic!("Failed to close handle! Reason: {}", result);
            }
        }
    }
}

/// Acquires a native handle by wrapping it in an untyped handle, allowing
/// us to track the resource and free it appropriately
pub unsafe fn acquire(handle: MojoHandle) -> UntypedHandle {
    UntypedHandle { value: handle }
}
