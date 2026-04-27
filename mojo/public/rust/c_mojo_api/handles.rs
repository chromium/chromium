// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module provides wrappers around the underlying C handle types, which
//! are simply integers (though the low-level C code uses them as pointers).
//!
//! There are two distinct types of handle: `MessageHandle`s represent a message
//! object that can be written or or read from a message pipe. All other handles
//! are represented as a single `UntypedHandle` type.
//!
//! We deliberately do not provide strongly-typed wrappers around
//! `UntypedHandle` to distinguish, e.g. a message pipe endpoint from a data
//! pipe endpoint from a trap. That task is left to higher-level API/binding
//! code. This is for two reasons:
//! 1. We cannot enforce that the types are correct: several API functions
//!    accept and return any type of handle, so context is needed to determine
//!    the meaning of any given handle type.
//! 2. We want to minimize the code which has access to the underlying integer
//!    value of the handle, since accidentally copying it can result in holding
//!    a closed handle.
//!
//! ## Liveness
//!
//! The handle types guarantee that the contained handle is *live*. Liveness
//! has two requirements:
//! 1. The handle must not be null (0)
//! 2. The handle must be "alive", i.e. it was obtained from a mojo call that
//!    creates new handles, and has never been closed.
//!
//! (1) is enforced by the type system; (2) is enforced by closing the handle
//! only when it is dropped. Therefore, it is important that the handle types
//! remain unique (do not implement Copy or Clone); otherwise, it would be
//! possible to close a handle which still exists elsewhere in the program.

chromium::import! {
  "//mojo/public/rust/c_mojo_api:mojo_c_system_bindings" as raw_ffi;
}

use crate::result::*;

// It's unlikely, but if the underlying type for these handles ever changes
// we'll need to change our representation to match.
static_assertions::assert_type_eq_all!(raw_ffi::MojoHandle, usize);
static_assertions::assert_type_eq_all!(raw_ffi::MojoMessageHandle, usize);

// TODO(crbug.com/498966599): Expose and use base::win::IsPseudoHandle
// instead of reimplementing it here. See the documentation for that
// function for more information.
fn is_pseudohandle(raw_value: raw_ffi::MojoHandle) -> bool {
    // Truncate to 32 bits, and treat as signed
    let val = raw_value as i32;
    return (-12..0).contains(&val);
}

/// A wrapper for the MojoHandle C type which is guaranteed to be live.
/// This type can represent any handle except for a message object.
#[repr(transparent)]
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Hash)] // Do NOT derive Copy or Clone!
pub struct UntypedHandle {
    pub(crate) handle_value: std::num::NonZeroUsize,
    // Private member to force construction using the `wrap_raw_value`
    // function, which requires explicit use of `unsafe`.
    _private: (),
}

impl UntypedHandle {
    /// Create a new UntypedHandle from a raw value.
    ///
    /// # Safety
    /// The value must represent a live, unowned handle.
    /// Passing a value of 0 will panic, but will not cause undefined behavior.
    pub unsafe fn wrap_raw_value(raw_value: raw_ffi::MojoHandle) -> Self {
        if is_pseudohandle(raw_value) {
            panic!("Cannot wrap a pseudohandle (value between 0 and -12)!")
        }
        Self { handle_value: raw_value.try_into().unwrap(), _private: () }
    }

    // FOR RELEASE(https://crbug.com/458499013): We may want these two
    // slice-as-pointer functions to move off UntypedHandle and into their
    // own top level thing, e.g. slice_as_cxx_ptr and whatnot.
    /// Convert a slice of UntypedHandles into a pointer to their underlying
    /// handle values.
    pub fn slice_as_ptr(handles: &[Self]) -> *const raw_ffi::MojoHandle {
        // Passing nothing must be done explicitly:
        // https://davidben.net/2024/01/15/empty-slices.html
        if handles.is_empty() {
            return std::ptr::null();
        }
        // `Self` is a repr(transparent) wrapper for `MojoHandle`, so the
        // pointer cast is sound.
        handles.as_ptr().cast()
    }

    /// Convert a mutable slice of UntypedHandles into a pointer to their
    /// underlying handle values.
    pub fn slice_as_mut_ptr(handles: &mut [Self]) -> *mut raw_ffi::MojoHandle {
        // Passing nothing must be done explicitly:
        // https://davidben.net/2024/01/15/empty-slices.html
        if handles.is_empty() {
            return std::ptr::null_mut();
        }
        // `Self` is a repr(transparent) wrapper for `MojoHandle`, so the
        // pointer cast is sound.
        handles.as_mut_ptr().cast()
    }

    /// Consume this UntypedHandle and return the underlying raw MojoHandle.
    ///
    /// This function gives up ownership of the underlying handle, so the
    /// caller is responsible for ensuring it does not get copied, and gets
    /// properly closed.
    pub fn into_raw_value(self) -> raw_ffi::MojoHandle {
        let val = self.handle_value.into();
        std::mem::forget(self);
        val
    }
}

impl Drop for UntypedHandle {
    fn drop(&mut self) {
        // SAFETY: Our invariant is that this handle is live
        let result =
            MojoError::result_from_code(unsafe { raw_ffi::MojoClose(self.handle_value.into()) });
        // The only way closing can fail is if the handle is invalid
        debug_assert!(result.is_ok());
    }
}

/// A wrapper for the MojoMessageHandle C type which is guaranteed to be live.
/// This type always represents a Mojo message object.
///
/// NOTE: The C equivalent of this type is not thread-safe, because it exposes
/// direct mutable access to its internal buffer without any locks. In Rust,
/// the borrow checker enforces that accesses are safe, so this type can be
/// safely shared between threads..
#[repr(transparent)]
#[derive(Debug)] // Do NOT derive Copy or Clone!
pub struct MessageHandle {
    pub(crate) handle_value: std::num::NonZeroUsize,
}

impl MessageHandle {
    /// Create a new MessageHandle from a raw value.
    ///
    /// # Safety
    /// The value must represent a live, unowned handle.
    /// Passing a value of 0 will panic, but will not cause undefined behavior.
    pub unsafe fn wrap_raw_value(raw_value: raw_ffi::MojoHandle) -> Self {
        if is_pseudohandle(raw_value) {
            panic!("Cannot wrap a handle value between 0 and -12!")
        }
        Self { handle_value: raw_value.try_into().unwrap() }
    }
}

impl Drop for MessageHandle {
    fn drop(&mut self) {
        // SAFETY: Our invariant is that this handle is live
        let result = MojoError::result_from_code(unsafe {
            raw_ffi::MojoDestroyMessage(self.handle_value.into())
        });
        // The only way closing can fail is if the handle is invalid
        debug_assert!(result.is_ok());
    }
}
