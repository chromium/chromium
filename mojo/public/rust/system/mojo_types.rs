// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains a variety of types which are used
//! for representing representing flag arguments a little bit
//! better than just as u32 and some other basic Mojo types that
//! we need to expose.
//!
//! This module also provides MojoResult which is the canonical
//! result coding system used by Mojo.
//!
//! Many places in the system code directly import this module as
//! a whole because it is intended to be used that way. It contains
//! all of the basic types needed by all system-level Mojo bindings.

use crate::ffi::types::{self, *};
use std::fmt;
use std::u64;

pub use types::MojoHandle;
pub use types::MojoMessageHandle;
pub use types::MojoTimeTicks;

/// Represents a deadline for wait() calls.
pub type MojoDeadline = u64;
pub static MOJO_INDEFINITE: MojoDeadline = u64::MAX;

pub use crate::wait_set::WaitSetResult;

/// MojoResult represents anything that can happen
/// as a result of performing some operation in Mojo.
///
/// It's implementation matches exactly that found in
/// the Mojo C API so this enum can be used across the
/// FFI boundary simply by using "as u32".
#[derive(Copy, Clone, Debug, PartialEq)]
#[repr(u32)]
pub enum MojoResult {
    Okay = 0,
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
    InvalidResult,
}

impl MojoResult {
    /// Convert a raw u32 code given by the C Mojo functions
    /// into a MojoResult.
    pub fn from_code(code: MojoResultCode) -> MojoResult {
        match code as u32 {
            0 => MojoResult::Okay,
            1 => MojoResult::Cancelled,
            2 => MojoResult::Unknown,
            3 => MojoResult::InvalidArgument,
            4 => MojoResult::DeadlineExceeded,
            5 => MojoResult::NotFound,
            6 => MojoResult::AlreadyExists,
            7 => MojoResult::PermissionDenied,
            8 => MojoResult::ResourceExhausted,
            9 => MojoResult::FailedPrecondition,
            10 => MojoResult::Aborted,
            11 => MojoResult::OutOfRange,
            12 => MojoResult::Unimplemented,
            13 => MojoResult::Internal,
            14 => MojoResult::Unavailable,
            15 => MojoResult::DataLoss,
            16 => MojoResult::Busy,
            17 => MojoResult::ShouldWait,
            _ => MojoResult::InvalidResult,
        }
    }

    pub fn to_str(&self) -> &'static str {
        match *self {
            MojoResult::Okay => "OK",
            MojoResult::Cancelled => "Cancelled",
            MojoResult::Unknown => "Unknown",
            MojoResult::InvalidArgument => "Invalid Argument",
            MojoResult::DeadlineExceeded => "Deadline Exceeded",
            MojoResult::NotFound => "Not Found",
            MojoResult::AlreadyExists => "Already Exists",
            MojoResult::PermissionDenied => "Permission Denied",
            MojoResult::ResourceExhausted => "Resource Exhausted",
            MojoResult::FailedPrecondition => "Failed Precondition",
            MojoResult::Aborted => "Aborted",
            MojoResult::OutOfRange => "Out Of Range",
            MojoResult::Unimplemented => "Unimplemented",
            MojoResult::Internal => "Internal",
            MojoResult::Unavailable => "Unavailable",
            MojoResult::DataLoss => "Data Loss",
            MojoResult::Busy => "Busy",
            MojoResult::ShouldWait => "Should Wait",
            MojoResult::InvalidResult => "Something went very wrong",
        }
    }

    /// For convenience in code using `Result<_, _>`, maps Okay to Ok(()) and
    /// other results to Err(self).
    pub fn into_result(self) -> Result<(), MojoResult> {
        match self {
            MojoResult::Okay => Ok(()),
            e => Err(e),
        }
    }
}

impl fmt::Display for MojoResult {
    /// Allow a MojoResult to be displayed in a sane manner.
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.to_str())
    }
}

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    #[repr(transparent)]
    pub struct HandleSignals: MojoHandleSignals {
        const READABLE = 1 << 0;
        const WRITABLE = 1 << 1;
        const PEER_CLOSED = 1 <<2;
        const NEW_DATA_READABLE = 1 <<3;
        const PEER_REMOTE = 1 << 4;
        const QUOTA_EXCEEDED = 1 << 5;
    }
}

impl HandleSignals {
    /// Check if the readable flag is set.
    pub fn is_readable(&self) -> bool {
        self.contains(HandleSignals::READABLE)
    }

    /// Check if the writable flag is set.
    pub fn is_writable(&self) -> bool {
        self.contains(HandleSignals::WRITABLE)
    }

    /// Check if the peer-closed flag is set.
    pub fn is_peer_closed(&self) -> bool {
        self.contains(HandleSignals::PEER_CLOSED)
    }
}

/// Represents the signals state of a handle: which signals are satisfied,
/// and which are satisfiable.
#[repr(transparent)]
#[derive(Clone, Copy, Debug)]
pub struct SignalsState(pub MojoHandleSignalsState);

impl SignalsState {
    /// Generates a new SignalsState
    pub fn new(satisfied: HandleSignals, satisfiable: HandleSignals) -> SignalsState {
        SignalsState(MojoHandleSignalsState {
            satisfied_signals: satisfied.bits(),
            satisfiable_signals: satisfiable.bits(),
        })
    }
    /// Gets a reference to the satisfied signals
    pub fn satisfied(&self) -> HandleSignals {
        HandleSignals::from_bits_truncate(self.0.satisfied_signals)
    }
    /// Gets a reference to the satisfiable signals
    pub fn satisfiable(&self) -> HandleSignals {
        HandleSignals::from_bits_truncate(self.0.satisfiable_signals)
    }

    /// Return the wrapped Mojo FFI struct.
    pub fn to_raw(self) -> MojoHandleSignalsState {
        self.0
    }

    /// Get a pointer to the inner struct for FFI calls.
    pub fn as_raw_mut_ptr(&mut self) -> *mut MojoHandleSignalsState {
        &mut self.0 as *mut _
    }
}

impl std::default::Default for SignalsState {
    fn default() -> Self {
        SignalsState(MojoHandleSignalsState { satisfied_signals: 0, satisfiable_signals: 0 })
    }
}
