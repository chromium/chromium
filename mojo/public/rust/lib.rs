// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![feature(maybe_uninit_slice)]
#![feature(new_uninit)]
// Require unsafe blocks for unsafe operations even in an unsafe fn.
#![deny(unsafe_op_in_unsafe_fn)]

#[macro_use]
mod macros {
    /// This macro must be used at the top-level in any
    /// Rust Mojo application. It defines and abstracts away the
    /// hook needed by Mojo in order to set up the basic
    /// functionality (see mojo::system::ffi). It must take the
    /// name of a function who returns a MojoResult and takes
    /// exactly one argument: a mojo::handle::Handle, or on in
    /// other words, an untyped handle.
    #[macro_export]
    macro_rules! set_mojo_main {
        ( $fn_main:ident ) => {
            #[allow(bad_style)]
            #[no_mangle]
            pub fn MojoMain(app_request_handle: mojo::system::MojoHandle) -> mojo::MojoResult {
                use mojo::system::CastHandle;
                use std::panic;
                let handle = unsafe {
                    mojo::system::message_pipe::MessageEndpoint::from_untyped(
                        mojo::system::acquire(app_request_handle),
                    )
                };
                let result = panic::catch_unwind(|| $fn_main(handle));
                match result {
                    Ok(value) => value,
                    Err(_) => mojo::MojoResult::Aborted,
                }
            }
        };
    }
}

#[macro_use]
pub mod bindings;
pub mod system;

pub use crate::system::MojoResult;
