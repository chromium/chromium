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
//! This means users of RawTraps must manually
//! ensure the object pointed to by `context` outlives the trigger (and
//! do the associated pointer-casting/management).
//!
//! When we make a Trap with this interface:
//! 1. The user provides a closure (of type FnMut(&TrapEvent)) to `add_trigger`.
//! 2. That closure is encapsulated in a `Trigger` struct and moved onto the
//!    heap.
//! 3. We take the memory address of `Trigger` and pass *that* to the C API as
//!    the raw `context`.
//! 4. We return an opaque `TriggerId` to the user and use *that* to safely
//!    track triggers.
//!
//! A diagram of what these relationships look like is as follows:
//!
//!       +-------+
//!       | Trap  |
//!       +-------+
//!      /        |
//!     /         |
//!    /      trigger_registry: Arc<TriggerRegistry>
//!   /               |
//! raw_trap          |
//!   |               |
//!   |               |
//!   |               |
//!   |               |
//!   v               v
//! +----------+    +-------------------------------+
//! | RawTrap  |    | TriggerRegistry               |
//! +----------+    +-------------------------------+
//! | handle   |    | trigger_map:                  |
//! +----------+    |   HashMap<usize, Arc<Trigger> |
//!       |         | next_trigger_id: usize        |
//!       |         +-------------------------------+
//!       v               |                      |
//! +------------+        v                      |
//! | MojoHandle |   next_trigger_id             |
//! +------------+        |                      |
//!                       |                      |
//!                       |   +-----------------------------+
//!                       |   | HashMap<usize, Arc<Trigger>>|------+
//!                       |   +-----------------------------+      |
//!                       |           |                 |          v
//!                       |           |                 |    (other Triggers...)
//!                                   v                 v
//!       +------------------------------+       +--------------------+
//!       |         Trigger              |       | other Triggers...  |
//!       +------------------------------+       +--------------------+
//!       | callback: FnMut(&TrapEvent)  |
//!       | owner: weak pointer to owning|
//!       |        TriggerRegistry       |
//!       | handle: MojoHandle           |
//!       | trigger_id                   |
//!       +------------------------------+
//!
//! Two notes on the above:
//!
//! * Trigger objects hold weak pointers to their owning TriggerRegistry. This
//!   allows Trigger objects to reach "up" and manage/cleanup tracking state
//!   after being triggered.
//!
//! * Upon creating a RawTrap (which is done implicitly/"under the hood" when
//!   creating a Trap), the underlying C API call MojoCreateTrap takes the
//!   passed-in MojoTrapEventHandler function pointer and stores it in a place
//!   to be managed by the Mojo system itself. It will be freed when the
//!   MojoHandle for Trap is released.

// FOR_RELEASE: There are a lot of implementation details even in this file.
// Find a way to present *just* the API to end users.

#![allow(unused)] /* FOR_RELEASE: Remove this once we've filled out the API. */

use crate::mojo_types::*;
pub use crate::raw_trap::TriggerCondition;
use crate::raw_trap::*;

use std::collections::HashMap;
use std::mem;
use std::sync::{Arc, Mutex, Weak};

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
    /// The signal can no longer happen on the handle.
    FailedPrecondition,
}

pub enum ArmingPolicyForBlockingEvents {
    RearmUntilNoBlockingEvents,
    ArmOnce,
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
        match self.result {
            MojoResult::Okay => Ok(()),
            MojoResult::Cancelled => Err(TrapError::Cancelled),
            MojoResult::FailedPrecondition => Err(TrapError::FailedPrecondition),
            unhandled_result => {
                panic!("TrapEvent received an unhandled MojoResult: {:?}", unhandled_result)
            }
        }
    }

    /// The handle's current and possible signals as of triggering.
    pub fn signals_state(&self) -> SignalsState {
        self.signals_state
    }
}

/// A safe, ergonomic wrapper around `RawTrap`.
///
/// When creating a Trap, triggers are added by passing in a Rust closure
/// (FnMut), and Trap manages the lifecycle of these closures by storing them
/// in `Trigger` objects.
pub struct Trap {
    // The thin wrapper around the Mojo C API for traps.
    raw_trap: RawTrap,
    // The "database" of triggers associated with this trap.
    // It is locked because it can be accessed on multiple threads: Mojo trap
    // callbacks can come from any thread. Shared ownership is used via `Arc`
    // because `Weak` refs are held by each `Trigger`. In turn each
    // `Trigger` is referenced by both `trigger_registry` (with `Arc`) and
    // the C side (with `Weak`).
    trigger_registry: Arc<Mutex<TriggerRegistry>>,
}

impl Trap {
    pub fn new() -> Result<Self, MojoResult> {
        Ok(Trap {
            raw_trap: RawTrap::new(Self::handle_event_from_callback)?,
            trigger_registry: Arc::new(Mutex::new(TriggerRegistry {
                trigger_map: HashMap::new(),
                next_trigger_id: 0,
            })),
        })
    }

    // To initialize a Trap, we must pass it some "default" handler/function
    // pointer.
    //
    // This will be the function called by the Mojo C API.
    //
    // For our Trap implementation, we initialize our trap with this
    // `handle_event_from_callback` function, which serves to wrap the callback
    // that the user passes in via `add_trigger`.
    //
    // # Safety consideration
    //
    // This must only be called once for a given `raw_event`.
    // It takes ownership of the Weak ref from the C side if the event is Cancelled.
    extern "C" fn handle_event_from_callback(raw_event: &RawTrapEvent) {
        // Get the Arc<Trigger> from the raw context.
        let trigger_data = unsafe { Self::get_trigger_data_from_event(raw_event) };

        // Upgrade the Weak reference to TriggerRegistry to get the mutex.
        // We panic if this upgrade fails; that is a state that should never happen.
        // Specifically:
        // * `TriggerRegistry`` is owned by `Trap`. Each `Trigger` has a Weak pointer to
        //   `TriggerRegistry``.
        // * The underlying C Mojo API guarantees that the registered event handler (in
        //   our case, `handle_event_from_callback`) will only be called while the
        //   handle to `Trap` is valid.
        // * Therefore, if we're in this function (e.g. if a callback is currently
        //   running), the `TriggerRegistry` associated with our `Trap` must be valid.
        //
        // An `unwrap` failure would indicate that the owning TriggerRegistry
        // disappeared while this trigger was active which should be impossible.
        let owner = trigger_data.owner.upgrade().unwrap();

        // Lock the TriggerRegistry mutex.
        // Note to self: may not need to lock this?
        let mut trigger_registry = owner.lock().unwrap();

        // Call the user's callback.
        Self::call_callback_and_maybe_delete_data(&mut trigger_registry, trigger_data, raw_event);
    }

    // # Safety consideration
    // The callback passed in to add_trigger should be able to account for
    // both an Ok() MojoResult (the "happy" path) and the following TrapErrors:
    // * `Cancelled``, which is returned every time a Trap goes out of scope, and
    // * `FailedPrecondition`, which is returned if the conditions for this trigger
    //   ever become impossible (e.g., if one end of a pipe is closed, such that the
    //   other end will never again become readable)
    // You can find examples in `core_api_unittests.rs`, e.g. in
    // `test_safe_trap_multiple_blocking_events`
    pub fn add_trigger(
        &self,
        handle_to_trap: &impl Trappable,
        signals: HandleSignals,
        condition: TriggerCondition,
        callback: impl FnMut(&TrapEvent) + Send + 'static,
    ) -> Result<TriggerId, MojoResult> {
        let (trigger_data_ptr, id): (*const Trigger, _) = {
            let mut trigger_registry = self.trigger_registry.lock().unwrap();
            let id = TriggerId(trigger_registry.next_trigger_id);
            trigger_registry.next_trigger_id += 1;

            let trigger_data = Arc::new(Trigger {
                callback: Mutex::new(Box::new(callback)),
                owner: Arc::downgrade(&self.trigger_registry),
                handle: handle_to_trap.get_native_handle(),
                trigger_id: id,
            });
            if trigger_registry.trigger_map.insert(id, trigger_data.clone()).is_some() {
                panic!("trigger_id unexpectedly already exists in trigger_map")
            }

            (Arc::downgrade(&trigger_data).into_raw(), id)
        };

        // Now call the add_trigger in the inside implementation.
        match self.raw_trap.add_trigger(
            handle_to_trap,
            signals,
            condition,
            trigger_data_ptr as usize,
        ) {
            MojoResult::Okay => Ok(id),
            e => {
                // If add_trigger fails, we must clean up the associated data.
                let mut trigger_registry = self.trigger_registry.lock().unwrap();
                // Take the Weak pointer from the raw pointer and drop it.
                let _: Weak<Trigger> = unsafe { Weak::from_raw(trigger_data_ptr) };
                trigger_registry.trigger_map.remove(&id);
                Err(e)
            }
        }
    }

    pub fn remove_trigger(&mut self, trigger_id: TriggerId) -> Result<(), MojoResult> {
        todo!()
    }

    // Removes *all* triggers from this Trap.
    pub fn clear_triggers(&mut self) -> Result<(), MojoResult> {
        todo!()
    }

    // Arms the trap according to the ArmingPolicyForBlockingEvents passed in.
    //
    // There are two possible arming policies:
    //
    // RearmUntilNoBlockingEvents:
    //
    // At the time of arming the trap, if there are blocking events,
    // the associated callback is called on those events *immediately*
    // until the blocking events queue clears, then `arm` is attempted
    // again.
    //
    // ArmOnce: Not yet implemented (FOR_RELEASE: implement it!).
    //
    // A single attempt is made to arm the trap. Blocking events will
    // not be automatically drained; if there are such events, an error
    // will simply be returned to be handled by the user.
    //
    // # Safety consideration
    //
    // The handler must at least try to make progress on clearing blocking
    // events when this function is called. (Otherwise there is a risk of looping
    // forever on attempts to clear the blocking events.)
    pub fn arm(&self, arming_policy: ArmingPolicyForBlockingEvents) -> Result<(), MojoResult> {
        match arming_policy {
            ArmingPolicyForBlockingEvents::RearmUntilNoBlockingEvents => {
                const MAX_BLOCKING_EVENTS: usize = 16;
                let mut buf = [mem::MaybeUninit::uninit(); MAX_BLOCKING_EVENTS];

                loop {
                    let blocking_events: &[RawTrapEvent] = match self.raw_trap.arm(Some(&mut buf)) {
                        ArmResult::Blocked(events) => events,
                        ArmResult::Armed => return Ok(()),
                        ArmResult::Failed(e) => {
                            return Err(e);
                        }
                    };
                    let mut trigger_registry = self.trigger_registry.lock().unwrap();
                    for blocking_event in blocking_events {
                        let trigger_data =
                            unsafe { Self::get_trigger_data_from_event(blocking_event) };
                        Self::call_callback_and_maybe_delete_data(
                            &mut trigger_registry,
                            trigger_data,
                            blocking_event,
                        )
                    }
                }
            }
            ArmingPolicyForBlockingEvents::ArmOnce => {
                todo!();
            }
        }
    }

    // Takes a given RawTrapEvent and determines the associated Trigger.
    //
    // # Safety
    //
    // This fn must only be called once for a given event.
    unsafe fn get_trigger_data_from_event(raw_event: &RawTrapEvent) -> Arc<Trigger> {
        // A raw pointer version of `Weak<Trigger>`.
        // Emulates a weak reference held by the C side.
        let trigger_data_ptr = raw_event.trigger_context() as *const Trigger;

        // We want to grab an actual `Weak<Trigger>`. But
        // we must take care to maintain the weak count correctly. The C side
        // still holds a reference unless the event type is Cancelled.
        let trigger_data: Weak<Trigger> = if raw_event.result() == MojoResult::Cancelled {
            // The C side effectively drops its reference and never calls
            // this again with `trigger_data_ptr`. So we take its reference,
            // later dropping it.
            unsafe { Weak::from_raw(trigger_data_ptr) }
        } else {
            // Otherwise, we must clone the weak pointer and then forget it:
            // we reconstitute the C side's `Weak` ref, grab our own, then
            // `forget` the original so the C side still holds its ref.
            let c_trigger_data = unsafe { Weak::from_raw(trigger_data_ptr) };
            let our_trigger_data = c_trigger_data.clone();
            mem::forget(c_trigger_data);
            our_trigger_data
        };

        // Return an `Arc` reference to the handle's data.
        // Will panic if it no longer exists (indicates something has gone
        // unrecoverably wrong with our handle management).
        trigger_data.upgrade().unwrap()
    }

    // Calls the callback associated with a Trigger.
    // If the Trigger is no longer "live", deletes the associated data.
    fn call_callback_and_maybe_delete_data(
        trigger_registry: &mut TriggerRegistry,
        trigger_data: Arc<Trigger>,
        raw_event: &RawTrapEvent,
    ) {
        let event =
            TrapEvent { signals_state: raw_event.signals_state(), result: raw_event.result() };

        // Call the callback, acquiring the associated lock.
        {
            let mut callback_guard = trigger_data.callback.lock().unwrap();
            (*callback_guard)(&event);
        }

        // If the trigger was cancelled, remove it from our internal map.
        if raw_event.result() == MojoResult::Cancelled {
            let trigger_id = trigger_data.trigger_id;
            // Drop our `trigger_data` Arc *before* calling, so the assertions
            // in `remove_cancelled_trigger` are correct.
            drop(trigger_data);
            Self::remove_cancelled_trigger(trigger_registry, trigger_id);
        }
    }

    fn remove_cancelled_trigger(trigger_registry: &mut TriggerRegistry, trigger_id: TriggerId) {
        // If the `unwrap` panics, we've tried to remove a nonexistent handle, which
        // should be impossible unelss there is an error in how we are handling
        // handles.
        let trigger_data: Arc<_> = trigger_registry.trigger_map.remove(&trigger_id).unwrap();

        // If the caller managed the ref counts correctly, `trigger_data`'s inner
        // data should be dropped after this call.
        debug_assert_eq!(1, Arc::strong_count(&trigger_data), "unexpected strong ref");
        // Question for discussion:
        //
        // FOR_RELEASE: Right now we leak a Weak pointer per trigger upon
        // trigger removal, which causes the following assertion that
        // *used* to be here....
        //
        // `debug_assert_eq!(0, Arc::weak_count(&trigger_data), "unexpected weak
        // ref");`
        //
        // ...to fail.
        //
        // This is because a Weak pointer is passed to the C side in
        // `add_trigger` via `Weak::into_raw` and then never consumed.
        //
        // In particular, when the Trap is Dropped, since we do not have access
        // to the C-side Weak pointer, we create a *new* Weak pointer
        // via another `into_raw` call so that we can (correctly) pass
        // *that* value to the underlying `raw_trap.remove_trigger()`
        // call.
        //
        // This works fine but does mean the original Weak pointer is leaked.
        //
        // At the time we fully implement `remove_trigger` however we must
        // prevent this leakage.  (We should take care that any time we
        // remove a trigger we remove it both from our map and the underlying
        // Mojo.)
        //
        // We'll do this by updating the code so it can access the raw
        // underlying `context` (safely) from the safe API layer.
    }
}

impl Drop for Trap {
    fn drop(&mut self) {
        let triggers_to_remove: Vec<usize>;
        {
            let registry = self.trigger_registry.lock().unwrap();
            triggers_to_remove = registry
                .trigger_map
                .values()
                .map(|trigger_arc| {
                    // Convert &Arc<Trigger> to the raw usize context
                    Arc::downgrade(trigger_arc).into_raw() as usize
                })
                .collect();
        }
        for context in triggers_to_remove {
            self.raw_trap.remove_trigger(context);
            unsafe {
                let _: Weak<Trigger> = Weak::from_raw(context as *const Trigger);
            }
        }
    }
}

struct TriggerRegistry {
    // Represents a registry of triggers associated with a Trap.
    //
    // Data in the TriggerRegistry may be accessed on multiple threads.
    //
    // Triggers are identified by an ID: `add_trigger` returns the ID, and
    // the client can later call `remove_trigger` on said ID to unsubscribe from
    // events on the associated handle. To support removal we maintain a mapping
    // from the client's IDs to our internal per-handle data.
    //
    // Each `Trigger` is owned through an `Arc` since we use `Weak` refs
    // that we pass to the C side. The `Weak` refs are
    // converted to raw pointers with `Weak::into_raw()`, passed to the C API,
    // and reconstituted by `Weak::from_raw()` when passed to us by callback.
    trigger_map: HashMap<TriggerId, Arc<Trigger>>,
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
struct Trigger {
    // FOR_RELEASE: These Mutexes *may* be imposing an unnecessary overhead
    // on our end users (though "traps can be called back on any thread" is
    // a pretty tricky constraint).
    //
    // Revisit here once we've got end-to-end Traps working, with an eye for
    // seeing how the C++ API works in this case to guide a potential improved
    // design.  It may be possible to send these to a fixed sequence or
    // the sequence the trap was armed on (once we have sequences available
    // in Rust).
    #[allow(clippy::type_complexity)]
    callback: Mutex<Box<dyn FnMut(&TrapEvent) + Send + 'static>>,
    // Trigger points back to the TriggerRegistry that "owns" it.
    owner: Weak<Mutex<TriggerRegistry>>,
    handle: types::MojoHandle,
    trigger_id: TriggerId,
}
