// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::collections::{hash_map, HashMap};
use std::sync::{Arc, Condvar, Mutex};

use crate::handle::Handle;
use crate::mojo_types;
use crate::mojo_types::{MojoResult, SignalsState};
use crate::trap::{Trap, TrapEvent, TriggerCondition, TriggerId};

/// Identifies a handle added to `WaitSet`.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct WaitSetCookie(pub u64);

/// Represents a single ready handle.
#[derive(Clone, Copy, Debug)]
pub struct WaitSetResult {
    /// Cookie corresponding to a handle that is ready.
    pub cookie: WaitSetCookie,
    /// Wait result, like from `Trap`:
    /// * MojoResult::Okay: a signal was satisfied.
    /// * MojoResult::FailedPrecondition: a signal can never be satisfied again.
    /// * MojoResult::Cancelled: the handle was closed.
    pub wait_result: MojoResult,
    /// The latest known signals on the ready handle. if `wait_result` is
    /// `MojoResult::Okay` then it is guaranteed to contain at least one signal
    /// specified in `WaitSet::add`.
    pub signals_state: SignalsState,
}

/// Provides synchronous waiting on a dynamic set of handles.
///
/// Similar to `Trap`, a set of handles paired with signals to watch for can be
/// dynamically added and removed. Unlike `Trap`, which gives asynchronous
/// callbacks when an event happens, `WaitSet` provides a `wait_on_set` method
/// which synchronously returns events, waiting if necessary.
pub struct WaitSet {
    // The Mojo primitive that gives us notifications on handles. We register
    // handles paired with signals we're interested in and a context object.
    trap: Trap<Context, fn(event: &TrapEvent, context: &Context)>,
    // `trap` vends `TriggerId` to identify one (handle, trigger condition) pair
    // registered with it. We give a "cookie" to our client for each handle, so
    // we need to map this client-facing cookie to the trap's `TriggerId`.
    cookies: HashMap<WaitSetCookie, TriggerId>,
    // Contains state necessary to store received events and wait synchronously.
    // Ownership is shared with the `Context` objects we give to `trap`.
    state: Arc<State>,
}

struct State {
    // Maintain the most recent result for each signalled handle. Only contains
    // results of handles for which we've received a callback.
    results: Mutex<HashMap<WaitSetCookie, WaitSetResult>>,
    // Enables putting the thread to sleep while waiting for an event.
    cond: Condvar,
}

// Passed to the `Trap`. These exist 1:1 with handles added to the set.
struct Context {
    state: Arc<State>,
    cookie: WaitSetCookie,
}

impl WaitSet {
    /// Create a new WaitSet or returns an error code if the `Trap` could not be
    /// created.
    pub fn new() -> Result<WaitSet, MojoResult> {
        Ok(WaitSet {
            trap: Trap::new(Self::event_handler as _)?,
            cookies: HashMap::new(),
            state: Arc::new(State { results: Mutex::new(HashMap::new()), cond: Condvar::new() }),
        })
    }

    /// Add a handle to set, watching for `signals`.
    ///
    /// If the handle is closed it is implicitly removed from the set. A
    /// `WaitSetResult` of type `MojoResult::Cancelled` will be returned on the
    /// next call to `wait_on_set`.
    ///
    /// `cookie` must be a unique (but arbitrary) value to identify the handle.
    /// `WaitSetResult`s returned by `wait_on_set` contain the handle's cookie.
    pub fn add(
        &mut self,
        handle: &dyn Handle,
        signals: mojo_types::HandleSignals,
        cookie: WaitSetCookie,
    ) -> MojoResult {
        let cookie_entry = match self.cookies.entry(cookie) {
            hash_map::Entry::Occupied(_) => return MojoResult::AlreadyExists,
            hash_map::Entry::Vacant(e) => e,
        };

        let context = Context { state: self.state.clone(), cookie };

        match self.trap.add_trigger(
            handle.get_native_handle(),
            signals,
            TriggerCondition::SignalsSatisfied,
            context,
        ) {
            Ok(token) => {
                // Insert the cookie *after* adding the trigger. Only `remove`
                // and `wait_on_set` rely on the entry, not the handler function
                // which doesn't have access to `self` anyway. We take `&mut
                // self` so we are mutually exclusive with `wait_on_set` and
                // `remove`. Hence, it is OK to do this.
                cookie_entry.insert(token);
                MojoResult::Okay
            }
            Err(e) => e,
        }
    }

    /// Remove the handle identified by `cookie` from the set. No results for
    /// this handle will ever be returned by subsequent calls to `wait_on_set`.
    pub fn remove(&mut self, cookie: WaitSetCookie) -> MojoResult {
        let token = match self.cookies.remove(&cookie) {
            Some(token) => token,
            None => return MojoResult::NotFound,
        };

        match self.trap.remove_trigger(token) {
            // Gracefully handle if the trigger auto-deregistered (e.g. the
            // handle was closed).
            MojoResult::NotFound => MojoResult::Okay,
            // Removing a trigger will fire an event, even if the trap is not
            // armed. We filter in `wait_on_set` based on `self.cookies` to
            // avoid returning events for `cookie`.
            MojoResult::Okay => MojoResult::Okay,
            e => unreachable!("unexpected Mojo error {:?}", e),
        }
    }

    /// Wait on this wait set.
    ///
    /// The conditions for the wait to end include:
    /// * A handle has its requested signals satisfied.
    /// * A handle is determined to never be able to have its requested signals
    ///   satisfied.
    /// * A handle is closed.
    ///
    /// `output` is cleared, then populated with results for each ready handle
    /// on success. `MojoResult` is returned indicating success or failure
    /// (which can only happen if the `Trap` fails to arm for unknown reasons).
    pub fn wait_on_set(&self, output: &mut Vec<WaitSetResult>) -> MojoResult {
        output.clear();

        {
            // Take the lock for the results. Unwrap because we cannot recover
            // from a poisoned mutex.
            let mut results_guard = self.state.results.lock().unwrap();
            let results: &mut HashMap<WaitSetCookie, WaitSetResult> = &mut results_guard;

            // If there were already results (that weren't filtered out) we just
            // return them.
            if !results.is_empty() {
                self.process_results(results, output);
                if !output.is_empty() {
                    return MojoResult::Okay;
                }
            }
        }

        // Now we've unlocked the mutex and can arm the trap. We must loop: we
        // filter results for handles explicitly removed. If we are left with no
        // events, we still want to wait.
        //
        // NOTE: there's an edge case where we can receive a Cancelled event in
        // between our check above and arming. In this case we may leave the
        // trap armed after returning from this function. This is not ideal but
        // it is safe since our `event_handler` only pushes a new value to our
        // internal results list. The `Trap` type also safely handles if its
        // drop() is never called. There is no way to work around this edge case
        // with the Mojo trap API.

        assert!(output.is_empty());
        while output.is_empty() {
            match self.trap.arm() {
                MojoResult::Okay | MojoResult::FailedPrecondition => (),
                MojoResult::NotFound => return MojoResult::NotFound,
                e => unreachable!("unexpected Mojo error {:?}", e),
            };

            let mut results_guard = self
                .state
                .cond
                .wait_while(self.state.results.lock().unwrap(), |r| r.is_empty())
                .unwrap();
            let results: &mut HashMap<WaitSetCookie, WaitSetResult> = &mut results_guard;
            self.process_results(results, output);
        }

        MojoResult::Okay
    }

    // Helper function to report the latest `WaitSetResult` received by the trap
    // handler for each handle entry. Filters out events corresponding to
    // `remove` calls, which `Trap` sends for each `remove_trigger` call.
    fn process_results(
        &self,
        results: &mut HashMap<WaitSetCookie, WaitSetResult>,
        output: &mut Vec<WaitSetResult>,
    ) {
        // Report results, filtering those with no `self.cookies` entry which
        // means the handle was removed.
        output.extend(results.values().filter(|result| self.cookies.contains_key(&result.cookie)));
        results.clear();
    }

    fn event_handler(event: &TrapEvent, context: &Context) {
        let mut results_guard = context.state.results.lock().unwrap();
        let results: &mut HashMap<WaitSetCookie, WaitSetResult> = &mut results_guard;
        // Simply transcribe and insert the event. Removal events are processed
        // on the waiting task in a `wait_on_set` call. Note that the owning
        // `WaitSet` may not even contain the cookie: a handle may be closed
        // between `Trap::add_trigger` and inserting into `WaitSet::cookies`.
        // That is OK since only `wait_on_set` relies on the entry. `add` and
        // `wait_on_set` are mutually exclusive because `add` takes `&mut self`
        results.insert(
            context.cookie,
            WaitSetResult {
                cookie: context.cookie,
                wait_result: event.result(),
                signals_state: event.signals_state(),
            },
        );
        context.state.cond.notify_all();
    }
}
