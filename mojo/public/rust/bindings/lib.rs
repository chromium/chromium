// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![feature(int_roundings)]
// Require unsafe blocks for unsafe operations even in an unsafe fn.
#![deny(unsafe_op_in_unsafe_fn)]

/// `pub` since a macro refers to `$crate::system`.
pub extern crate mojo_system as system;

pub mod macros;

pub mod decoding;
pub mod encoding;
pub mod message;
pub mod mojom;

pub use system::util::run_loop;
