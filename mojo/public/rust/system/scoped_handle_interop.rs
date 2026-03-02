// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module (and its corresponding C++ files) defines a way to convert
//! safely between C++ and Rust handle types.
//!
//! TRY TO AVOID USING THESE TYPES! It is vastly preferable to get your mojo
//! handles via an existing message pipe. These types are meant to be used
//! over FFI in situations where that isn't feasible.
//!
//! For each type (untyped, message pipe, data pipe, etc), the C++ side defines
//! a wrapper for that ScopedHandle type. We need separate wrapper types for
//! each because cxx doesn't handle templated types.
//!
//! To give ownership of a handle from C++ to Rust, wrap the scoped handle in
//! the corresponding wrapper type, create a unique pointer, and pass it into
//! Rust (via an FFI function that you define yourself). Then you can simply
//! call that wrapper's `into_whatever_handle` method to convert it to the Rust
//! type.
//!
//! To give ownership of a handle from Rust to C++, call that wrapper's
//! `from_whatever_handle` method, then hand it to C++ (again, via your own FFI
//! function).
//!
//! For an example of doing this, see //mojo/public/rust/bindings/test/cxx.rs.

use crate::message_pipe::MessageEndpoint;
use crate::mojo_types::UntypedHandle;

#[cxx::bridge(namespace = "mojo::rust")]
mod ffi {
    unsafe extern "C++" {
        include!("mojo/public/rust/system/scoped_handle_interop.h");

        pub type ScopedHandleWrapper;
        #[Self = ScopedHandleWrapper]
        fn Release(wrapper: UniquePtr<ScopedHandleWrapper>) -> usize;
        #[Self = ScopedHandleWrapper]
        fn Create(handle: usize) -> UniquePtr<ScopedHandleWrapper>;

        pub type ScopedMessagePipeHandleWrapper;
        #[Self = ScopedMessagePipeHandleWrapper]
        fn Release(wrapper: UniquePtr<ScopedMessagePipeHandleWrapper>) -> usize;
        #[Self = ScopedMessagePipeHandleWrapper]
        fn Create(handle: usize) -> UniquePtr<ScopedMessagePipeHandleWrapper>;
    }
}

// Re-export all the types in the bridge, since they're the only thing in this
// file so there's no point having them in a sub-module.
pub use ffi::*;

// We have to use inherent implementations rather than `From` due to the
// orphaning rule: this crate doesn't define `UniquePtr` or `UntypedHandle`.

impl ScopedHandleWrapper {
    /// Returns None if the given handle is 0.
    pub fn into_untyped_handle(handle: cxx::UniquePtr<Self>) -> Option<UntypedHandle> {
        let raw = Self::Release(handle);
        if raw == 0 {
            return None;
        }
        // SAFETY: The ScopedHandle type owns its underlying handle value, which
        // is either live or 0, and we know it's not 0. `Release` gives up that
        // ownership.
        return Some(unsafe { UntypedHandle::wrap_raw_value(raw) });
    }

    pub fn from_untyped_handle(handle: UntypedHandle) -> cxx::UniquePtr<Self> {
        Self::Create(handle.into_raw_value())
    }
}

impl ScopedMessagePipeHandleWrapper {
    /// Returns None if the given handle is 0.
    pub fn into_message_endpoint(handle: cxx::UniquePtr<Self>) -> Option<MessageEndpoint> {
        let raw = Self::Release(handle);
        if raw == 0 {
            return None;
        }
        // SAFETY: The ScopedHandle type owns its underlying handle value, which
        // is either live or 0, and we know it's not 0. `Release` gives up that
        // ownership.
        let untyped = unsafe { UntypedHandle::wrap_raw_value(raw) };
        Some(MessageEndpoint::from(untyped))
    }

    pub fn from_message_endpoint(endpoint: MessageEndpoint) -> cxx::UniquePtr<Self> {
        Self::Create(UntypedHandle::from(endpoint).into_raw_value())
    }
}
