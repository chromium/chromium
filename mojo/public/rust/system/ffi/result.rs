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
// bindings for `MOJO_RESULT_CANCELLED` and use that here instead of
// manually hardcoding constants for each variant.

// FOR_RELEASE: See if we can get approval for the `strum` crate and use it here
// instead of manually writing string/int conversion functions.

/// MojoError represents all failures that can happen as a result of performing
/// some operation in Mojo.
///
/// Its implementation matches that found in the Mojo C API
/// (//mojo/public/c/system/types.h) so this enum can be used across the FFI
/// boundary simply by using "as u32" (but do not that the "OK" code is not a
/// valid value, since this represents errors only).
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
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
            1 => Err(MojoError::Cancelled),
            2 => Err(MojoError::Unknown),
            3 => Err(MojoError::InvalidArgument),
            4 => Err(MojoError::DeadlineExceeded),
            5 => Err(MojoError::NotFound),
            6 => Err(MojoError::AlreadyExists),
            7 => Err(MojoError::PermissionDenied),
            8 => Err(MojoError::ResourceExhausted),
            9 => Err(MojoError::FailedPrecondition),
            10 => Err(MojoError::Aborted),
            11 => Err(MojoError::OutOfRange),
            12 => Err(MojoError::Unimplemented),
            13 => Err(MojoError::Internal),
            14 => Err(MojoError::Unavailable),
            15 => Err(MojoError::DataLoss),
            16 => Err(MojoError::Busy),
            17 => Err(MojoError::ShouldWait),
            _ => unreachable!("Received unexpected return code from mojo function: {}", code),
        }
    }

    pub fn as_str(&self) -> &'static str {
        // TODO(https://crbug.com/456535277): Deduplicate MojoResult string
        // definitions across different language APIs.
        match *self {
            MojoError::Cancelled => "Cancelled",
            MojoError::Unknown => "Unknown",
            MojoError::InvalidArgument => "Invalid Argument",
            MojoError::DeadlineExceeded => "Deadline Exceeded",
            MojoError::NotFound => "Not Found",
            MojoError::AlreadyExists => "Already Exists",
            MojoError::PermissionDenied => "Permission Denied",
            MojoError::ResourceExhausted => "Resource Exhausted",
            MojoError::FailedPrecondition => "Failed Precondition",
            MojoError::Aborted => "Aborted",
            MojoError::OutOfRange => "Out Of Range",
            MojoError::Unimplemented => "Unimplemented",
            MojoError::Internal => "Internal",
            MojoError::Unavailable => "Unavailable",
            MojoError::DataLoss => "Data Loss",
            MojoError::Busy => "Busy",
            MojoError::ShouldWait => "Should Wait",
        }
    }
}

impl std::fmt::Display for MojoError {
    /// Allow a MojoResult to be displayed in a sane manner.
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}
