// Copyright 2016 The Chromium Authors. All rights reserved.
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

use crate::system::ffi;
// This full import is intentional; nearly every type in mojo_types needs to be used.
use crate::system::mojo_types::*;

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
    fn wait(&self, signals: HandleSignals) -> (SignalsState, MojoResult) {
        let mut state: SignalsState = Default::default();
        let r = unsafe {
            ffi::MojoWait(self.get_native_handle(), signals.get_bits(), &mut state.0 as *mut _)
        };
        (state, MojoResult::from_code(r))
    }
}

/// The basic untyped handle that wraps a MojoHandle (a u32)
pub struct UntypedHandle {
    /// The native Mojo handle
    value: MojoHandle,
}

impl UntypedHandle {
    /// Invalidates the Handle by setting its native handle to
    /// zero, the canonical invalid handle in Mojo.
    ///
    /// This function is unsafe because clearing a native handle
    /// without closing it is a resource leak.
    pub unsafe fn invalidate(&mut self) {
        self.value = 0
    }

    /// Checks if the native handle is valid (0 = canonical invalid handle).
    pub fn is_valid(&self) -> bool {
        self.value != 0
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
