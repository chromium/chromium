// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![feature(maybe_uninit_slice)]
// Require unsafe blocks for unsafe operations even in an unsafe fn.
#![deny(unsafe_op_in_unsafe_fn)]

mod handle;
mod mojo_types;

pub mod core;
pub mod data_pipe;
pub mod message_pipe;
pub mod shared_buffer;
pub mod trap;
pub mod wait;
pub mod wait_set;

/// Export publicly for tests, but use a different name. This is awkward since
/// we can't have private `mod ffi` then re-export it publicly under a different
/// name.
#[path = "ffi.rs"]
pub mod ffi_for_testing;
use ffi_for_testing as ffi;

// In order to keep the interface clean, we re-export basic Mojo and handle
// types and traits here in the system module.
pub use crate::handle::*;
pub use crate::mojo_types::*;
