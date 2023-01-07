// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod ffi;
mod handle;
mod mojo_types;

pub mod core;
pub mod data_pipe;
pub mod message_pipe;
pub mod shared_buffer;
pub mod trap;
pub mod wait;
pub mod wait_set;

// In order to keep the interface clean, we re-export basic Mojo and handle
// types and traits here in the system module.
pub use crate::system::handle::*;
pub use crate::system::mojo_types::*;
