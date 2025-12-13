// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module provides a safe, high-level `Trap` API that *does not* require
//! any management of raw `context` objects.
//!
//! This is the preferred trap API unless callers have constraints that make the
//! overhead of the heap allocations/tracking untenable - in that case the
//! lower-level `RawTrap` API may be considered.
//!
//! # Implementation details
//!
//! Manually setting RawTraps is potentially error-prone/unsafe due to
//! the requirement that, when adding a trigger to a trap, users provide
//! a raw `context` value (typically a pointer).
//!
//! Entailed in this requirement is the fact that the user must manually
//! ensure the object pointed to by `context` outlives the trigger (and
//! do the associated pointer-casting/management).
//!
//! When we make a Trap with this interface:
//! 1. The user provides a closure (event handler) to `add_trigger`.
//! 2. That closure is placed in a `TriggerData` struct and moved onto the heap.
//! 3. We take the memory address of `TriggerData` and pass *that* to the C API
//!    as the raw `context`.
//! 4. We return an opaque `TriggerId` to the user and use *that* to safely
//!    track triggers.

// FOR_RELEASE: There are a lot of implementation details even in this file.
// Find a way to present *just* the API to end users.

#![allow(unused)] /* FOR_RELEASE: Remove this once we've filled out the API. */

use crate::mojo_types::*;
use crate::raw_trap::*;

use std::sync::{Arc, Mutex, Weak};

use std::collections::HashMap;

chromium::import! {
  pub "//mojo/public/rust:mojo_ffi";
}

use mojo_ffi::types;

/// Unique ID for a trigger added to a `Trap`.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct TriggerId(usize);

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TrapError {
    /// The trigger was removed (explicitly or by closure).
    Cancelled,
    /// The signal can no longer happen on the handl.
    FailedPrecondition,
}

// Represents a trap event, e.g. a trigger firing.
#[derive(Clone, Copy, Debug)]
pub struct TrapEvent {
    signals_state: SignalsState,
    result: MojoResult,
}

impl TrapEvent {
    /// Why the trigger fired:
    /// * Okay: a specified signal occurred.
    /// * FailedPrecondition: a signal can no longer happen on the handle.
    /// * Cancelled: the trigger was removed (explicitly or by closure).
    pub fn result(&self) -> Result<(), TrapError> {
        // FOR_RELEASE: Upon constructing a TrapEvent, we should detect if the
        // C layer has done something unexpected and panic/handle accordingly.
        match self.result {
            MojoResult::Okay => Ok(()),
            MojoResult::Cancelled => Err(TrapError::Cancelled),
            MojoResult::FailedPrecondition => Err(TrapError::FailedPrecondition),
            _ => todo!(),
        }
    }

    /// The handle's current and possible signals as of triggering.
    pub fn signals_state(&self) -> SignalsState {
        self.signals_state
    }
}

/// A safe, ergonomic wrapper around `RawTrap`.
///
/// The C Mojo API associates a `context` with every trigger,
/// and the RawTrap API understands how to interface with those `context`
/// objects.
///
/// The Trap API abstracts this away.  Instead of manually handling `context`
/// objects, triggers are added by passing in a Rust closure (FnMut), and Trap
/// manages the lifecycle of these closures by storing them in `TriggerData`
/// objects.
pub struct Trap {
    // The thin wrapper around the Mojo C API for traps.
    trap: RawTrap,
    // The inner data used by the Rust wrapper. It is locked because it can be
    // accessed on multiple threads: Mojo trap callbacks can come from any
    // thread. Shared ownership is used via `Arc` because `Weak` refs are held
    // by each `TriggerData`. In turn each `TriggerData` is referenced by both
    // `inner` (with `Arc`) and the C side (with `Weak`).
    inner: Arc<Mutex<TrapInnerData>>,
}

impl Trap {
    pub fn new() -> Result<Self, MojoResult> {
        Ok(Trap {
            trap: RawTrap::new(Self::raw_handler)?,
            inner: Arc::new(Mutex::new(TrapInnerData {
                trigger_map: HashMap::new(),
                next_trigger_id: 0,
            })),
        })
    }

    // Get raw handler to initialize RawTrap.
    extern "C" fn raw_handler(event: &RawTrapEvent) {}

    pub fn add_trigger(
        &self,
        handle_to_trap: &impl Trappable,
        signals: HandleSignals,
        condition: TriggerCondition,
        handler: impl FnMut(&TrapEvent),
    ) -> Result<TriggerId, MojoResult> {
        todo!()
    }

    pub fn remove_trigger(&mut self, trigger_id: TriggerId) -> Result<(), MojoResult> {
        todo!()
    }

    pub fn arm(&mut self) -> Result<(), MojoResult> {
        todo!()
    }
}

struct TrapInnerData {
    // Represents the data "inside" the trap which may be accessed on multiple
    // threads.
    //
    // Triggers are identified by an ID: `add_trigger` returns the ID, and
    // the client can later call `remove_trigger` on said ID to unsubscribe from
    // events on the associated handle. To support removal we maintain a mapping
    // from the client's IDs to our internal per-handle data.
    //
    // Each `TriggerData` is owned through an `Arc` since we use `Weak` refs
    // that we pass to the C side. The `Weak` refs are
    // converted to raw pointers with `Weak::into_raw()`, passed to the C API,
    // and reconstituted by `Weak::from_raw()` when passed to us by callback.
    trigger_map: HashMap<TriggerId, Arc<TriggerData>>,
    // Field for a unique ID, generated per trigger.
    next_trigger_id: usize,
}

// Internal state for a single active trigger.
//
// While `RawTrap` wraps the C Mojo API functions, this struct
// serves as the object *referenced by* the opaque `context` integer
// passed to that API.
//
// Thus this struct is referenced when making the necessary
// RawTrap calls "under the hood".
struct TriggerData {
    // FOR_RELEASE: These Mutexes *may* be imposing an unnecessary overhead
    // on our end users. However, sorting out a superior model for cross-thread
    // synchronization is likely to be a larger project (one that we're in
    // active discussions about right now.)  We *must* revisit this choice to
    // make sure we are still satisfied with it prior to handing v1 over to users
    // but let's use the Mutexes for now.
    callback: Mutex<Box<dyn FnMut(&TrapEvent) + Send>>,
    // TriggerData points back to the Trap that "owns" it.
    owner: Weak<Mutex<TrapInnerData>>,
    handle: types::MojoHandle,
    trigger_id: TriggerId,
}
