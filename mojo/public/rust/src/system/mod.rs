// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod ffi;
mod mojo_types;
mod handle;

pub mod core;
pub mod data_pipe;
pub mod message_pipe;
pub mod shared_buffer;
pub mod wait_set;

// In order to keep the interface clean, we re-export basic Mojo and handle
// types and traits here in the system module.
pub use system::handle::*;
pub use system::mojo_types::*;
