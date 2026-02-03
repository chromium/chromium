// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This is an internal module that defines a macro for declaring Rust wrappers
//! for the various `Options` structs which are consumed by the C API. These
//! wrappers aren't exposed to the user; options are handled via bitflags
//! instead.

// TODO(https://crbug.com/458796903): Most of the bitflags we use have
// their values manually copied from the corresponding C code. We should tweak
// bindgen to make those values directly available so instead, then go through
// all the `bitflags!` invocations in this crate and update them.

// Most FFI functions take an options struct as input which we get from bindgen.
// Each one contains a `struct_size` member for versioning. We want to make a
// 'newtype' wrapper for each that manages the struct_size as well as adds a
// `new()` function for construction.
//
// To reduce boilerplate we use a macro.
//
// The generated structs contain methods to get raw pointers for passing to FFI
// functions. Note the FFI functions don't require the structs to live beyond
// each call.
macro_rules! declare_mojo_options {
    ($struct_name:ident, $( $field_name:ident : $field_type:ty ),*) => {
        #[repr(transparent)]
        struct $struct_name(raw_ffi::$struct_name);

        impl $struct_name {
            pub fn new($($field_name : $field_type),*) -> $struct_name {
                $struct_name(raw_ffi::$struct_name {
                    struct_size: ::std::mem::size_of::<$struct_name>() as u32,
                    $($field_name),*
                })
            }

            // Get an immutable pointer to the wrapped FFI struct to pass to
            // C functions.
            pub fn as_ptr(&self) -> *const raw_ffi::$struct_name {
              // $struct_name is a repr(transparent) wrapper around raw_ffi::$struct_name
              std::ptr::from_ref(self).cast()
            }
        }
    }
}

pub(crate) use declare_mojo_options;
