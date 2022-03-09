// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

    /// This macro assists in generating flags for
    /// functions and methods found in mojo::system::message_pipe.
    ///
    /// See mojo::system::message_pipe for the available flags
    /// that may be passed.
    ///
    /// # Examples
    ///
    /// # mpflags!(Create::None);
    /// # mpflags!(Read::MayDiscard);
    #[macro_export]
    macro_rules! mpflags {
        ( $( $flag:path ),* ) => {{
            use $crate::system::message_pipe::*;
            $(
                ($flag as u32)
            )|*
        }}
    }

    /// This macro assists in generating flags for
    /// functions and methods found in mojo::system::data_pipe.
    ///
    /// See mojo::system::data_pipe for the available flags
    /// that may be passed.
    ///
    /// # Examples
    ///
    /// # dpflags!(Create::None);
    /// # dpflags!(Read::AllOrNone, Read::Discard);
    #[macro_export]
    macro_rules! dpflags {
        ( $( $flag:path ),* ) => {{
            use $crate::system::data_pipe::*;
            $(
                ($flag as u32)
            )|*
        }}
    }

    /// This macro assists in generating flags for
    /// functions and methods found in mojo::system::shared_buffer.
    ///
    /// See mojo::system::shared_buffer for the available flags
    /// that may be passed.
    ///
    /// # Examples
    ///
    /// # sbflags!(Create::None);
    /// # sbflags!(Map::None);
    #[macro_export]
    macro_rules! sbflags {
        ( $( $flag:path ),* ) => {{
            use $crate::system::shared_buffer::*;
            $(
                ($flag as u32)
            )|*
        }}
    }

    /// This macro assists in generating flags for
    /// functions and methods found in mojo::system::wait_set.
    ///
    /// See mojo::system::wait_set for the available flags
    /// that may be passed.
    ///
    /// # Examples
    ///
    /// # wsflags!(Create::None);
    /// # wsflags!(Add::None);
    #[macro_export]
    macro_rules! wsflags {
        ( $( $flag:path ),* ) => {{
            use $crate::system::wait_set::*;
            $(
                ($flag as u32)
            )|*
        }}
    }

    /// This macro assists in generating MojoSignals to be
    /// used in wait() and wait_many(), part of mojo::system::core.
    ///
    /// See mojo::system::handle for the available signals
    /// that may be checked for by wait() and wait_many().
    ///
    /// # Examples
    ///
    /// # signals!(Signals::Readable, Signals::Writable);
    /// # signals!(Signals::PeerClosed);
    #[macro_export]
    macro_rules! signals {
        ( $( $flag:path ),* ) => {{
            use $crate::system::Signals;
            $crate::system::HandleSignals::new(
            $(
                ($flag as u32)
            )|*
            )
        }}
    }
}

#[macro_use]
pub mod bindings;
pub mod system;

pub use crate::system::MojoResult;
