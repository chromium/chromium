// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This crate defines (mostly) safe Rust wrappers around the Mojo C API. These
//! wrappers aim to provide a Rust-y interface around the C code with minimal
//! unsafety. The wrappers do _not_ aim to provide higher-level abstractions
//! than the C code does; that is the job of the system/bindings API.
//!
//! Each wrapper function has the name name as the underlying C function, and
//! documents its behavior, including the set of possible errors it might return
//! if the function fails.
//!
//! Many functions additionally support flags to modify their behavior; for each
//! such function, this module provides an associated `FooFlags` type which can
//! be used to pass them into the function.
//!
//! ## Fundamental Types
//!
//! This crate uses the `UntypedHandle` type to represent a raw mojo handle to
//! some object. These objects manage ownership of the raw handle, ensuring that
//! it's not closed until the object is dropped. Therefore, handles cannot be
//! cloned or copied.
//!
//! This crate deliberately does not provide strongly-typed wrappers; that is
//! left to higher-level APIs. The primary reason is that an `UntypedHandle`
//! cannot always be interpreted without context that the C API lacks.
//!
//! All Mojo wrappers return a `MojoResult<T>`, which is an alias for
//! `Result<T, MojoError>`. The `MojoError` enum contains all possible error
//! codes from the underlying API; consult the documentation of each function
//! for information about what codes are possible.

pub mod data_pipe;
mod internal_options;
mod mojo_handles;
mod mojo_result;

pub use mojo_handles::*;
pub use mojo_result::*;
