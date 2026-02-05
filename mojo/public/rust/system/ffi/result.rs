// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module provides Rust representations of the `MojoResult` C type, which
//! represents the possible error codes from the underlying functions. To make
//! things more Rust-y, it splits the return codes into two types:
//! - The `MojoError` enum contains all _failure_ codes.
//! - The `MojoResult<T> = Result<T, MojoError>` type represents all possible
//!   results.

chromium::import! {
  "//mojo/public/rust/system:mojo_c_system_bindings" as raw_ffi;
}

use raw_ffi::MojoResult as MojoResultCode;

// TODO(https://crbug.com/457917334): See if `bindgen` can generate
// bindings for the code definitions and use those here instead of
// manually hardcoding constants for each variant.

/// MojoError represents all failures that can happen as a result of performing
/// some operation in Mojo.
///
/// Its implementation matches that found in the Mojo C API
/// (//mojo/public/c/system/types.h) so this enum can be used across the FFI
/// boundary simply by using "as u32" (but do not that the "OK" code is not a
/// valid value, since this represents errors only).
#[derive(Copy, Clone, Debug, Eq, PartialEq, strum::FromRepr, strum::IntoStaticStr)]
#[repr(u32)]
pub enum MojoError {
    Cancelled = 1,
    Unknown = 2,
    InvalidArgument = 3,
    DeadlineExceeded = 4,
    NotFound = 5,
    AlreadyExists = 6,
    PermissionDenied = 7,
    ResourceExhausted = 8,
    FailedPrecondition = 9,
    Aborted = 10,
    OutOfRange = 11,
    Unimplemented = 12,
    Internal = 13,
    Unavailable = 14,
    DataLoss = 15,
    Busy = 16,
    ShouldWait = 17,
}

pub type MojoResult<T> = Result<T, MojoError>;

impl MojoError {
    /// Convert a raw u32 code given by the C Mojo functions into a
    /// MojoResult.
    pub fn result_from_code(code: MojoResultCode) -> MojoResult<()> {
        match code {
            0 => Ok(()),
            _ => Err(MojoError::from_repr(code)
                .expect("Received unexpected return code from mojo function")),
        }
    }

    pub fn as_str(&self) -> &'static str {
        self.into()
    }
}

impl std::fmt::Display for MojoError {
    /// Allow a MojoResult to be displayed in a sane manner.
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}
