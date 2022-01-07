// Copyright 2016 The Chromium Authors. All rights reserved.
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

use std::fmt;
use std::u64;
use system::ffi::types::*;

/// A MojoHandle is represented as a plain 32-bit unsigned int.
pub type MojoHandle = u32;

/// Represents time ticks as specified by Mojo. A time tick value
/// is meaningless when not used relative to another time tick.
pub type MojoTimeTicks = i64;

/// Represents a deadline for wait() calls.
pub type MojoDeadline = u64;
pub static MOJO_INDEFINITE: MojoDeadline = u64::MAX;

pub type CreateFlags = u32;
pub type DuplicateFlags = u32;
pub type InfoFlags = u32;
pub type MapFlags = u32;
pub type WriteFlags = u32;
pub type ReadFlags = u32;
pub type AddFlags = u32;

/// MojoResult represents anything that can happen
/// as a result of performing some operation in Mojo.
///
/// It's implementation matches exactly that found in
/// the Mojo C API so this enum can be used across the
/// FFI boundary simply by using "as u32".
#[derive(Copy, Clone, Debug, PartialEq)]
#[repr(u32)]
pub enum MojoResult {
    Okay = 0x0,
    Cancelled = 0x1,
    Unknown = 0x2,
    InvalidArgument = 0x3,
    DeadlineExceeded = 0x4,
    NotFound = 0x5,
    AlreadyExists = 0x6,
    PermissionDenied = 0x7,
    ResourceExhausted = 0x8,
    FailedPrecondition = 0x9,
    Aborted = 0xa,
    OutOfRange = 0xb,
    Unimplemented = 0xc,
    Internal = 0xd,
    Unavailable = 0xe,
    DataLoss = 0xf,
    Busy = 0x0019,
    ShouldWait = 0x001e,
    InvalidResult,
}

impl MojoResult {
    /// Convert a raw u32 code given by the C Mojo functions
    /// into a MojoResult.
    pub fn from_code(code: MojoResultCode) -> MojoResult {
        match code as u32 {
            0x0 => MojoResult::Okay,
            0x1 => MojoResult::Cancelled,
            0x2 => MojoResult::Unknown,
            0x3 => MojoResult::InvalidArgument,
            0x4 => MojoResult::DeadlineExceeded,
            0x5 => MojoResult::NotFound,
            0x6 => MojoResult::AlreadyExists,
            0x7 => MojoResult::PermissionDenied,
            0x8 => MojoResult::ResourceExhausted,
            0x9 => MojoResult::FailedPrecondition,
            0xa => MojoResult::Aborted,
            0xb => MojoResult::OutOfRange,
            0xc => MojoResult::Unimplemented,
            0xd => MojoResult::Internal,
            0xe => MojoResult::Unavailable,
            0xf => MojoResult::DataLoss,
            0x0019 => MojoResult::Busy,
            0x001e => MojoResult::ShouldWait,
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
}

impl fmt::Display for MojoResult {
    /// Allow a MojoResult to be displayed in a sane manner.
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.to_str())
    }
}

/// This tuple struct represents a bit vector configuration of possible
/// Mojo signals. Used in wait() and wait_many() primarily as a convenience.
///
/// One invariant must be true for this data structure and it is that:
///     sizeof(HandleSignals) == sizeof(MojoHandleSignals)
/// If this is ever not the case or there is a way in Rust to ensure that,
/// this data structure must be updated to reflect that.
#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq)]
pub struct HandleSignals(MojoHandleSignals);

impl HandleSignals {
    /// Create a new HandleSignals given the raw MojoHandleSignals
    pub fn new(s: MojoHandleSignals) -> HandleSignals {
        HandleSignals(s)
    }

    /// Check if the readable flag is set
    pub fn is_readable(&self) -> bool {
        (self.0 & (Signals::Readable as u32)) != 0
    }

    /// Check if the writable flag is set
    pub fn is_writable(&self) -> bool {
        (self.0 & (Signals::Writable as u32)) != 0
    }

    /// Check if the peer-closed flag is set
    pub fn is_peer_closed(&self) -> bool {
        (self.0 & (Signals::PeerClosed as u32)) != 0
    }

    /// Check if the read threshold flag is set
    pub fn is_read_threshold(&self) -> bool {
        (self.0 & (Signals::ReadThreshold as u32)) != 0
    }

    /// Check if the write threshold flag is set
    pub fn is_write_threshold(&self) -> bool {
        (self.0 & (Signals::WriteThreshold as u32)) != 0
    }

    /// Pull the raw MojoHandleSignals out of the data structure
    pub fn get_bits(&self) -> MojoHandleSignals {
        self.0
    }
}

/// Represents the signals state of a handle: which signals are satisfied,
/// and which are satisfiable.
///
/// One invariant must be true for this data structure and it is that:
///     sizeof(SignalsState) == sizeof(MojoSignalsState) (defined in handle.h)
/// If this is ever not the case or there is a way in Rust to ensure that,
/// this data structure must be updated to reflect that.
#[repr(C)]
#[derive(Default)]
pub struct SignalsState {
    satisfied: HandleSignals,
    satisfiable: HandleSignals,
    _align: [u32; 0], // Hack to align to a 4-byte boundary
}

impl SignalsState {
    /// Generates a new SignalsState
    pub fn new(satisfied: HandleSignals, satisfiable: HandleSignals) -> SignalsState {
        SignalsState {
            satisfied: satisfied,
            satisfiable: satisfiable,
            _align: [],
        }
    }
    /// Gets a reference to the satisfied signals
    pub fn satisfied(&self) -> &HandleSignals {
        &self.satisfied
    }
    /// Gets a reference to the satisfiable signals
    pub fn satisfiable(&self) -> &HandleSignals {
        &self.satisfiable
    }
    /// Consume the SignalsState and release its tender interior
    ///
    /// Returns (satisfied, satisfiable)
    pub fn unwrap(self) -> (HandleSignals, HandleSignals) {
        (self.satisfied, self.satisfiable)
    }
}

/// The different signals options that can be
/// used by wait() and wait_many(). You may use
/// these directly to build a bit-vector, but
/// the signals! macro will already do it for you.
/// See the root of the library for more information.
#[repr(u32)]
pub enum Signals {
    None = 0,
    /// Wait for the handle to be readable
    Readable = 1 << 0,

    /// Wait for the handle to be writable
    Writable = 1 << 1,

    /// Wait for the handle to be closed by the peer
    /// (for message pipes and data pipes, this is
    /// the counterpart handle to the pipe)
    PeerClosed = 1 << 2,

    /// Wait for the handle to have at least some
    /// readable data
    ReadThreshold = 1 << 3,

    /// Wait for the handle to allow for at least
    /// some data to be writable
    WriteThreshold = 1 << 4,
}

/// The result struct used by the wait_set module
/// to return wait result information. Should remain
/// semantically identical to the implementation of
/// this struct in wait_set.h in the C bindings.
///
/// This struct should never be constructed by anything
/// but the Mojo system in MojoWaitSetWait.
#[repr(C)]
pub struct WaitSetResult {
    cookie: u64,
    result: MojoResultCode,
    reserved: u32,
    signals_state: SignalsState,
    _align: [u64; 0], // Hack to align struct to 8 byte boundary
}

impl WaitSetResult {
    /// Getter for the cookie corresponding to the handle
    /// which just finished waiting.
    pub fn cookie(&self) -> u64 {
        self.cookie
    }

    /// Getter for the wait result.
    pub fn result(&self) -> MojoResult {
        MojoResult::from_code(self.result)
    }

    /// Getter for the signals state that comes with any
    /// wait result.
    pub fn state(&self) -> &SignalsState {
        &self.signals_state
    }
}
