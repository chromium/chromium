// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::sync::{Arc, Condvar, Mutex};

use crate::mojo_types::{HandleSignals, MojoHandle, MojoResult, SignalsState};
use crate::trap::{Trap, TrapEvent, TriggerCondition};

/// The result of `wait`ing on a handle. There are three possible outcomes:
///     * A requested signal was satisfied
///     * A requested signal became unsatisfiable due to a change on the handle
///     * The handle was closed
#[derive(Clone, Copy, Debug)]
pub enum WaitResult {
    /// The handle had a signal satisfied.
    Satisfied(SignalsState),
    /// A requested signal became unsatisfiable for the handle.
    Unsatisfiable(SignalsState),
    /// The handle was closed.
    Closed,
}

impl WaitResult {
    /// Returns the signals state if satisfied, and an Err with the original
    /// result otherwise.
    pub fn satisfied(self) -> Result<SignalsState, WaitResult> {
        match self {
            Satisfied(s) => Ok(s),
            _ => Err(self),
        }
    }

    /// Returns the signals state if unsatisfiable, and an Err with the original
    /// result otherwise.
    pub fn unsatisfiable(self) -> Result<SignalsState, WaitResult> {
        match self {
            Unsatisfiable(s) => Ok(s),
            _ => Err(self),
        }
    }

    /// Returns the signals state if the handle wasn't closed, and an Err with
    /// the original result otherwise.
    pub fn signals_state(self) -> Result<SignalsState, WaitResult> {
        match self {
            Satisfied(s) => Ok(s),
            Unsatisfiable(s) => Ok(s),
            Closed => Err(self),
        }
    }
}

use WaitResult::*;

/// Wait on `handle` until a signal in `signals` is satisfied or becomes
/// unsatisfiable.
#[must_use]
pub fn wait(handle: MojoHandle, signals: HandleSignals) -> WaitResult {
    // Mojo's mechanism for asynchronous event notification is traps: a trap
    // object contains a handler function and a mapping of handles to
    // interesting signals. Traps provide asynchronous notification.
    //
    // To implement synchronous waiting on a single handle, we use a trap along
    // with a mutex/condvar. Our handler function simply stores the event and
    // signals the condvar.
    //
    // In `wait`'s stack frame, we arm the trap then if successful wait on the
    // mutex/condvar pair. We then process the received event.

    let ctx = Arc::new(Context { signaled_event: Mutex::new(None), cond: Condvar::new() });

    let handler = |event: &TrapEvent, context: &Arc<Context>| {
        // We have no way to recover from a poisoned mutex so just unwrap.
        let mut e = context.signaled_event.lock().unwrap();
        *e = Some(HandleEventInfo { result: event.result(), signals_state: event.signals_state() });
        context.cond.notify_all();
    };

    let trap = Trap::new(handler).unwrap();
    trap.add_trigger(handle, signals, TriggerCondition::SignalsSatisfied, ctx.clone()).unwrap();

    let event: HandleEventInfo = match trap.arm() {
        MojoResult::Okay | MojoResult::FailedPrecondition => {
            // Wait until we get a trap event callback (this should return
            // immediately if there was a blocking event). Unwrap because we
            // cannot recover from a poisoned mutex.
            let event_slot: Option<HandleEventInfo> =
                *ctx.cond.wait_while(ctx.signaled_event.lock().unwrap(), |e| e.is_none()).unwrap();

            // We can unwrap because `Cond::wait_while` will only return when
            // the condition is true (or the mutex was poisoned, but the first
            // unwrap covers that).
            event_slot.unwrap()
        }
        e => {
            panic!("unexpected mojo error {:?}", e);
        }
    };

    match event.result {
        MojoResult::Okay => Satisfied(event.signals_state),
        MojoResult::FailedPrecondition => Unsatisfiable(event.signals_state),
        MojoResult::Cancelled => Closed,
        e => panic!("unexpected mojo error {:?}", e),
    }
}

#[derive(Clone, Copy, Debug)]
struct HandleEventInfo {
    result: MojoResult,
    signals_state: SignalsState,
}

struct Context {
    signaled_event: Mutex<Option<HandleEventInfo>>,
    cond: Condvar,
}
