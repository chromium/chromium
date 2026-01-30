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

use crate::mojo_result::*;

// It's unlikely, but if the underlying type for these handles ever changes
// we'll need to change our representation to match.
static_assertions::assert_type_eq_all!(raw_ffi::MojoHandle, usize);
static_assertions::assert_type_eq_all!(raw_ffi::MojoMessageHandle, usize);

/// A wrapper for the MojoHandle C type which is guaranteed to be live.
/// This type can represent any handle except for a message object.
#[repr(transparent)]
#[derive(Debug)] // Do NOT derive Copy or Clone!
pub struct UntypedHandle {
    pub(crate) handle_value: std::num::NonZeroUsize,
}

impl UntypedHandle {
    /// Create a new UntypedHandle from a raw value.
    /// SAFETY: The value must represent a live, unonwned handle.
    pub unsafe fn wrap_raw_value(raw_value: raw_ffi::MojoHandle) -> Self {
        Self { handle_value: raw_value.try_into().unwrap() }
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
#[repr(transparent)]
#[derive(Debug)] // Do NOT derive Copy or Clone!
pub struct MessageHandle {
    pub(crate) handle_value: std::num::NonZeroUsize,
}

impl MessageHandle {
    /// Create a new MessageHandle from a raw value.
    /// SAFETY: The value must represent a live, unonwned handle.
    pub unsafe fn wrap_raw_value(raw_value: raw_ffi::MojoHandle) -> Self {
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
