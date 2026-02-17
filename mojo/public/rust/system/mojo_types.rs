//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains generally useful types and macros that are referenced
//! from multiple other modules.

chromium::import! {
  "//mojo/public/rust/system:ffi_bindings" as mojo_ffi;
}

pub use mojo_ffi::{MojoError, MojoResult, UntypedHandle};

// TODO(crbug.com/479878778): If the C API ever exposes the ability to check
// a handle's type, we could do the check here and change this to `TryFrom`.
/// Helper macro to declare strongly-typed wrappers around an UntypedHandle
/// which are inter-convertible with it.
macro_rules! declare_typed_handle {
    ($name:ident) => {
        #[derive(Debug, PartialEq, Eq)]
        pub struct $name {
            handle: UntypedHandle,
        }

        impl From<UntypedHandle> for $name {
            fn from(handle: UntypedHandle) -> Self {
                Self { handle }
            }
        }

        impl From<$name> for UntypedHandle {
            fn from(typed_handle: $name) -> UntypedHandle {
                typed_handle.handle
            }
        }
    };
}

macro_rules! declare_trappable_typed_handle {
    ($name:ident) => {
        crate::mojo_types::declare_typed_handle!($name);

        impl crate::raw_trap::Trappable for $name {
            fn get_untyped_handle(&self) -> &UntypedHandle {
                &self.handle
            }
        }
    };
}

pub(crate) use declare_trappable_typed_handle;
pub(crate) use declare_typed_handle;
