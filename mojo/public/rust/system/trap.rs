// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module provides a safe wrapper around the C mojo API which provides the
//! same guarantees as the lower-level trap objects but is safe to use.
//!
//! Rather than passing a raw `context` value (typically a pointer), you pass
//! a callback for each trigger. The Trap object takes care of the pointer
//! management under-the-hood. Otherwise, this object provides the same
//! operations as a C Trap object: adding triggers, removing triggers (by ID),
//! and arming the trap.
//!
//! You may assume the following properties of the trap:
//! 1. No user-provided callback will be invoked on multiple threads
//!    simultaneously.
//! 2. The last time each callback is run, it will have `Cancelled` as its
//!    `TrapResult`.
//!
//! There are no guarantees about _which_ thread each callback runs on, and
//! different invocations of the same callback may run on different threads
//! (just not at the same time).
//!
//! In order to invoke callbacks, the trap must be `arm`ed. If the trap would
//! immediately fire, it will refuse to arm, and will return a vector of events
//! corresponding to the invocations that would have happened if it fired.
//! These events contain the ID of their trigger in order to distinguish them.
//! It is your responsibility to clear all of these events before calling `arm`
//! again. For example, if one of the triggers watches for incoming messages on
//! a pipe, you might clear it by reading all the messages from that pipe.
//!
//! Furthermore, after the trap fires a handler (except for `Cancelled`
//! handlers), it disarms itself and must be manually re-armed.

use std::mem;

chromium::import! {
  "//mojo/public/rust/c_mojo_api" as mojo_ffi;
}

use crate::mojo_types::declare_typed_handle;

use mojo_ffi::trap;
pub use mojo_ffi::trap::types::ArmResult as RawArmResult;
pub use mojo_ffi::trap::types::TrapEvent as RawTrapEvent;
pub use mojo_ffi::trap::types::{HandleSignals, SignalsState};
pub use mojo_ffi::trap::TriggerCondition;
use mojo_ffi::{MojoError, MojoResult};

declare_typed_handle!(TrapHandle);

pub trait Trappable {
    fn get_untyped_handle(&self) -> &crate::mojo_types::UntypedHandle;
}

/// Unique ID for a trigger added to a `Trap`.
#[derive(Clone, Copy, Debug)]
pub struct TriggerId(usize);

type Trigger = Box<dyn FnMut(&TrapEvent) + Send + 'static>;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TrapError {
    /// The trigger was removed (explicitly or by closure).
    Cancelled,
    /// The signal can no longer happen on the handle.
    FailedPrecondition,
}

pub enum ArmResult {
    Success,
    BlockingEvents(Vec<TrapEvent>),
    NoTriggers,
}

impl ArmResult {
    pub fn expect(self, msg: &str) {
        match self {
            ArmResult::Success => (),
            _ => panic!("{}", msg),
        }
    }

    pub fn expect_blocking(self, msg: &str) -> Vec<TrapEvent> {
        match self {
            ArmResult::BlockingEvents(vec) => vec,
            _ => panic!("{}", msg),
        }
    }
}

// Represents a trap event, e.g. a trigger firing.
#[derive(bytemuck::TransparentWrapper)]
#[repr(transparent)]
#[derive(Clone, Debug)]
pub struct TrapEvent(RawTrapEvent);

impl<'a> From<&'a RawTrapEvent> for &'a TrapEvent {
    fn from(raw: &'a RawTrapEvent) -> Self {
        bytemuck::TransparentWrapper::wrap_ref(raw)
    }
}

impl TrapEvent {
    pub fn signals_state(&self) -> SignalsState {
        self.0.signals_state()
    }

    /// Why the trigger fired:
    /// * Okay: a specified signal occurred.
    /// * FailedPrecondition: a signal can no longer happen on the handle.
    /// * Cancelled: the trigger was removed (explicitly or by closure).
    pub fn result(&self) -> Result<(), TrapError> {
        match self.0.result() {
            Ok(()) => Ok(()),

            Err(MojoError::Cancelled) => Err(TrapError::Cancelled),
            Err(MojoError::FailedPrecondition) => Err(TrapError::FailedPrecondition),
            bad_result => {
                panic!("TrapEvent received an unexpected MojoResult: {:?}", bad_result)
            }
        }
    }

    pub fn trigger_id(&self) -> TriggerId {
        TriggerId(self.0.trigger_context())
    }
}

/// A safe Trap object that invokes user-defined handler functions (on an
/// arbitrary thread) whenever its trigger conditions are met.
///
/// To use the trap, add a "Trigger": a combination of a `FnMut` handler and
/// a specification of when it should fire, then arm the trap. Once it's armed,
/// it will invoke the handler whenever conditions are met. Each time the
/// handler runs, the trap will disarm itself immediately beforehand, and must
/// be re-armed before it will fire again.
pub struct Trap {
    // The actual underlying trap object.
    trap_handle: TrapHandle,
}

impl Trap {
    /// Create a new trap with no triggers.
    ///
    /// # Possible Error Codes:
    /// - `ResourceExhausted`: If the trap handler was unable to be created
    ///   (e.g. because the process ran out of possible handle values)
    pub fn new() -> MojoResult<Self> {
        Ok(Trap {
            trap_handle: trap::MojoCreateTrap(Self::handle_event_from_callback).map(Into::into)?,
        })
    }

    /// The underlying C trap object expects to get a single handler function
    /// which is invoked by all triggers. This is that function.
    ///
    /// This function's job is to take the provided `context` information from
    /// the `RawTrapEvent` and use it to invoke the actual
    /// user-provided handler. It does so by interpreting the `context` as a
    /// pointer to the a specific `Trigger` which contains the rest of the data.
    ///
    /// # Safety consideration
    ///
    /// This function must not be called concurrently with the same `context`
    /// value. After it is called with a `Cancelled` result, it must never be
    /// called again with the same `context` value. Note that the C Mojo API
    /// makes both these promises.
    //
    // Relevant lines from mojo/public/c/system/trap.h:
    // > the handler will never be entered for a trigger while another thread is
    // > executing it for the same trigger.
    // > ...
    // > |MOJO_RESULT_CANCELLED|: The trigger has been removed and will never
    // > cause another event to fire.
    extern "C" fn handle_event_from_callback(raw_event: &RawTrapEvent) {
        let trigger_ptr = raw_event.trigger_context() as *mut Trigger;

        {
            // Safety: The pointer was obtained from `Box::leak` so it's valid and
            // non-null. Since we're not called concurrently with the same context,
            // nobody else has access to this pointer. The pointer isn't freed until
            // we're called with `Cancelled`, and we're guaranteed not to be called
            // again after that point.
            let trigger_ref: &mut Trigger = unsafe { trigger_ptr.as_mut_unchecked() };

            // Call the user's callback, acquiring the associated lock.
            (trigger_ref)(raw_event.into());
        }

        // We're guaranteed that we'll never again be called with this context
        // value, so we should deallocate it.
        if raw_event.result() == Err(MojoError::Cancelled) {
            // Safety: This pointer was generated with `Box::into_raw`. No code
            // outside this module has access to this pointer (`TriggerId` wraps
            // it, but is opaque), so it hasn't been freed before. The reference
            // we made earlier is out of scope by now, so no references to this
            // memory exist.
            let _ = unsafe { Box::from_raw(trigger_ptr) };
        }
    }

    /// Add a trigger to the trap, which will fire when its signals become
    /// satisfied (or unsatisfied, depending on `condition`).
    ///
    /// The callback passed in to add_trigger should be able to account for
    /// both an Ok() MojoResult (the "happy" path) and the following TrapErrors:
    /// * `Cancelled``, indicates the trigger has been removed and will never
    ///   fire again. and
    /// * `FailedPrecondition`, which is returned if the conditions for this
    ///   trigger ever become impossible (e.g., if one end of a pipe is closed,
    ///   such that the other end will never again become readable). Note that
    ///   the trigger has not yet removed, and the handler may fire once more
    ///   with the `Cancelled` status.
    ///
    /// You can find examples in `core_api_unittests.rs`, e.g. in
    /// `test_trap_multiple_blocking_events`
    pub fn add_trigger(
        &self,
        handle_to_trap: &impl Trappable,
        signals: HandleSignals,
        condition: TriggerCondition,
        callback: impl FnMut(&TrapEvent) + Send + 'static,
    ) -> TriggerId {
        // Double boxing because the inner box is actually a fat pointer, so it
        // doesn't fit in a `usize`.
        let trigger_ptr: *mut Trigger = Box::into_raw(Box::new(Box::new(callback)));
        let trigger_usize = trigger_ptr as usize;

        trap::MojoAddTrigger(
            &self.trap_handle.handle,
            handle_to_trap.get_untyped_handle(),
            signals,
            condition,
            trigger_usize,
        )
        // Note: The mojo API says that it's invalid to add multiple traps on the
        // same handle, but it seems that requirement was silently dropped when
        // we transferred to ipcz.
        .expect("The Trap class handles all possible failures when adding a trigger");

        return TriggerId(trigger_usize);
    }

    /// Remove the trigger with the given ID from the trap.
    ///
    /// The handler will be invoked one last time with a `Cancelled` result.
    /// Note that the handler is invoked asynchronously, and may not finish
    /// before this function returns.
    ///
    /// # Possible Error Codes:
    /// - `NotFound`: If the trigger with that ID has already been removed
    pub fn remove_trigger(&self, trigger_id: TriggerId) -> MojoResult<()> {
        trap::MojoRemoveTrigger(&self.trap_handle.handle, trigger_id.0)
    }

    /// Attempt to arm the trap.
    ///
    /// If the trap would fire immediately, then instead of arming it returns a
    /// subset of the events that would it to fire. The caller is responsible
    /// for handling them and then calling this again.
    pub fn arm(&self) -> ArmResult {
        // We can increase this if we need to, but right now callers only use
        // one at a time.
        const MAX_BLOCKING_EVENTS: usize = 1;
        let mut buf =
            (0..MAX_BLOCKING_EVENTS).map(|_| mem::MaybeUninit::uninit()).collect::<Vec<_>>();

        match trap::MojoArmTrap(&self.trap_handle.handle, Some(&mut buf)) {
            RawArmResult::Armed => ArmResult::Success,
            RawArmResult::Blocked(events) => {
                let num_initialized_elements = events.len();
                ArmResult::BlockingEvents(
                    buf.into_iter()
                        .take(num_initialized_elements)
                        // Safety: `MojoArmTrap` guarantees that the first
                        // `events.len()` elements of `buf` are initialized
                        .map(|raw_uninit| TrapEvent(unsafe { raw_uninit.assume_init() }))
                        .collect(),
                )
            }
            RawArmResult::Failed(_) => ArmResult::NoTriggers,
        }
    }
}
