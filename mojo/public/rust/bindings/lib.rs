// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This crate defines various utilities for working with Mojo from Rust code.
//! The types in this crate are designed as ergonomic, high-level
//! representations of the underlying Mojo types. Developers should prefer to
//! use these types when possible. However, there is an abstraction cost, and
//! performance-critical code may need to use the lower-level types in the
//! `mojo_rust_system_bindings` crate instead.

pub mod interface;
pub mod message;
pub mod message_header;
pub mod message_pipe_watcher;
pub mod receiver;
pub mod remote;
