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
  "//mojo/public/rust/system:mojo_c_system_bindings" as raw_ffi;
}

use crate::result::*;

// It's unlikely, but if the underlying type for these handles ever changes
// we'll need to change our representation to match.
static_assertions::assert_type_eq_all!(raw_ffi::MojoHandle, usize);
static_assertions::assert_type_eq_all!(raw_ffi::MojoMessageHandle, usize);

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
    /// The value must represent a live, unonwned handle.
    pub unsafe fn wrap_raw_value(raw_value: raw_ffi::MojoHandle) -> Self {
        // FOR_RELEASE: There are apparently other types of handle ("Pseudohandles")
        // that should not be representable by this type. Look into these and check for
        // them here.
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
/// NOTE: Unlike other types of Mojo API objects, messages are NOT thread-safe
/// and thus callers of message-related APIs must be careful to restrict usage
/// of any given `MessageHandle` to a single thread at a time. In Rust, this is
/// enforced by not implementing the `Sync` trait.
#[repr(transparent)]
#[derive(Debug)] // Do NOT derive Copy or Clone!
pub struct MessageHandle {
    pub(crate) handle_value: std::num::NonZeroUsize,
    // This member is equivalent to `impl !Sync`, which is currently unstable
    _phantom_unsync: std::marker::PhantomData<std::cell::Cell<()>>,
}

impl MessageHandle {
    /// Create a new MessageHandle from a raw value.
    ///
    /// # Safety
    /// The value must represent a live, unonwned handle.
    pub unsafe fn wrap_raw_value(raw_value: raw_ffi::MojoHandle) -> Self {
        Self {
            handle_value: raw_value.try_into().unwrap(),
            _phantom_unsync: std::marker::PhantomData,
        }
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
