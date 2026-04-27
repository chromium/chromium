//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains generally useful types and macros that are referenced
//! from multiple other modules.

chromium::import! {
  "//mojo/public/rust/c_mojo_api" as mojo_ffi;
}

pub use mojo_ffi::{MojoError, MojoResult, UntypedHandle};

/// Helper macro to declare strongly-typed wrappers around an UntypedHandle
/// which are inter-convertible with it.
// TODO(crbug.com/479878778): If the C API ever exposes the ability to check
// a handle's type, we could do the check here and change these to `TryFrom`.
macro_rules! declare_typed_handle {
    ($name:ident) => {
        #[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
        pub struct $name {
            handle: $crate::mojo_types::UntypedHandle,
        }

        impl From<$crate::mojo_types::UntypedHandle> for $name {
            fn from(handle: $crate::mojo_types::UntypedHandle) -> Self {
                Self { handle }
            }
        }

        impl From<$name> for $crate::mojo_types::UntypedHandle {
            fn from(typed_handle: $name) -> Self {
                typed_handle.handle
            }
        }
    };
}

macro_rules! declare_trappable_typed_handle {
    ($name:ident) => {
        crate::mojo_types::declare_typed_handle!($name);

        impl crate::trap::Trappable for $name {
            fn get_untyped_handle(&self) -> &$crate::mojo_types::UntypedHandle {
                &self.handle
            }
        }
    };
}

pub(crate) use declare_trappable_typed_handle;
pub(crate) use declare_typed_handle;
