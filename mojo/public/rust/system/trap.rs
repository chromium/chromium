// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module provides a safe, high-level `Trap` API that *does not* require
//! any management of raw `context` objects.
//!
//! This is the preferred trap API unless callers have constraints that make the
//! overhead of the heap allocations/tracking untenable - in that case the
//! lower-level `TrapHandle` API may be considered.
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
//!   |               |
//!   |               |
//!   |               |
//!   |               |
//!   |               |
//!   v               v
//! +----------+    +-------------------------------+
//! | TrapHandle  |    | TriggerRegistry               |
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
//! * Upon creating a TrapHandle (which is done implicitly/"under the hood" when
//!   creating a Trap), the underlying C API call MojoCreateTrap takes the
//!   passed-in MojoTrapEventHandler function pointer and stores it in a place
//!   to be managed by the Mojo system itself. It will be freed when the
//!   MojoHandle for Trap is released.

pub use mojo_ffi::trap::TriggerCondition;

use std::collections::HashMap;
use std::mem;
use std::sync::{Arc, Mutex, Weak};

chromium::import! {
  "//mojo/public/rust/c_mojo_api" as mojo_ffi;
}

use crate::mojo_types::declare_typed_handle;

use mojo_ffi::trap;
pub use mojo_ffi::trap::types::TrapEvent as RawTrapEvent;
pub use mojo_ffi::trap::types::{ArmResult, HandleSignals, SignalsState};
use mojo_ffi::{MojoError, MojoResult};

declare_typed_handle!(TrapHandle);

pub trait Trappable {
    fn get_untyped_handle(&self) -> &crate::mojo_types::UntypedHandle;
}

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

pub enum TryArmResult {
    Success,
    BlockingEvents(Vec<TrapEvent>),
    NoTriggers,
}

/// This policy specifies whether the trap should automatically re-arm itself
/// after a trigger fires. If so, it will repeatedly fire all its triggers
/// until all blocking events are cleared. If not, it must be re-armed
/// manually after firing (often as part of the user-provided handler).
///
/// The `Automatic` policy is analogous to the `RunTriggersOnBlockingEvents`
/// initial arming policy, and has the same caveat: triggers must try to make
/// progress on clearing blocking events, or risk looping indefinitely.
pub enum RearmingPolicy {
    Manual,
    Automatic,
}

// Represents a trap event, e.g. a trigger firing.
// TODO(458499013): Replace this with a trait that we can implement for
// RawTrapEvent, so we don't have to actually convert between the types.
#[derive(Clone, Copy, Debug)]
pub struct TrapEvent {
    pub signals_state: SignalsState,
    /// Why the trigger fired:
    /// * Okay: a specified signal occurred.
    /// * FailedPrecondition: a signal can no longer happen on the handle.
    /// * Cancelled: the trigger was removed (explicitly or by closure).
    pub result: Result<(), TrapError>,
}

impl From<&RawTrapEvent> for TrapEvent {
    fn from(raw_event: &RawTrapEvent) -> Self {
        let result = match raw_event.result() {
            Ok(()) => Ok(()),
            Err(MojoError::Cancelled) => Err(TrapError::Cancelled),
            Err(MojoError::FailedPrecondition) => Err(TrapError::FailedPrecondition),
            bad_result => {
                panic!("TrapEvent received an unexpected MojoResult: {:?}", bad_result)
            }
        };
        TrapEvent { signals_state: raw_event.signals_state(), result }
    }
}

/// A safe Trap object that invokes user-defined handler functions (on an
/// arbitrary thread) whenever its trigger conditions are met.
///
/// To use the trap, add a "Trigger": a combination of a `FnMut` handler and
/// a specification of when it should fire, then arm the trap. Once it's armed,
/// it will invoke the handler whenever conditions are met. The trap can either
/// re-arm itself automatically after each trigger, or require manual rearming.
pub struct Trap {
    // The trap needs to share its internal state with its handler, so we make
    // separate internal structs wrapped in synchronization primitives.
    // This one is immutable so no need for a mutex.
    shared_data: Arc<TrapSharedData>,
    // This data holds a strong ref for each trigger, to prevent them from dying early.
    // Each trigger holds a circular reference to this data so it can delete itself.
    trigger_registry: Arc<Mutex<TriggerRegistry>>,
}

impl Trap {
    /// Create a new trap with no triggers.
    ///
    /// # Possible Error Codes:
    /// - `ResourceExhausted`: If the trap handler was unable to be created
    ///   (e.g. because the process ran out of possible handle values)
    pub fn new(rearming_policy: RearmingPolicy) -> MojoResult<Self> {
        Ok(Trap {
            shared_data: Arc::new(TrapSharedData {
                // SAFETY: The requirements of `handle_event_from_callback` are
                // guaranteed by the C trap API
                trap_handle: trap::MojoCreateTrap(Self::handle_event_from_callback)
                    .map(Into::into)?,
                rearming_policy,
            }),
            trigger_registry: Arc::new(Mutex::new(TriggerRegistry {
                trigger_map: HashMap::new(),
                next_trigger_id: 0,
            })),
        })
    }

    /// The underlying C trap object expects to get a single handler function
    /// which is invoked by all triggers. This is that function.
    ///
    /// This function's job is to take the provided `context` information from
    /// the `RawTrapEvent` and use it to invoke the actual
    /// user-provided handler. It does so by interpreting the `context` as a
    /// weak pointer to the a specific `Trigger` which contains the rest of
    /// the data.
    ///
    /// # Safety consideration
    ///
    /// This function must not be called concurrently with the same `context`
    /// value. After it is called with a `Cancelled` result, it must never be
    /// called again with the same `context` value. Note that the C Mojo API
    /// makes both these promises, but you must still be cautious if calling
    /// this function directly.
    //
    // mojo/public/c/system/trap.h:
    // > the handler will never be entered for a trigger while another thread is
    // > executing it for the same trigger.
    // > ...
    // > |MOJO_RESULT_CANCELLED|: The trigger has been removed and will never
    // > cause another event to fire.
    extern "C" fn handle_event_from_callback(raw_event: &RawTrapEvent) {
        // Get the Arc<Trigger> from the raw context.
        // SAFETY: Same conditions as this function
        let trigger_data = unsafe { Self::get_trigger_data_from_event(raw_event) };

        // These upgrades should only fail if the trap has been dropped,
        // in which case there's nothing left for us to do.
        let Some(trigger_data) = trigger_data else {
            return;
        };
        let Some(owner) = trigger_data.owner.upgrade() else {
            return;
        };
        let Some(shared_data) = trigger_data.shared_data.upgrade() else {
            return;
        };

        // Call the user's callback, acquiring the associated lock.
        (trigger_data.callback.lock().unwrap())(&raw_event.into());

        let trigger_id = trigger_data.trigger_id;
        drop(trigger_data);

        if raw_event.result() == Err(MojoError::Cancelled) {
            Self::remove_cancelled_trigger(&mut owner.lock().unwrap(), trigger_id);
        }

        // If the precondition is failed, we can't ever make progress again, so
        // remove the trigger. This will trigger an immediate handler with
        // `Cancelled` that will actually do the cleanup.
        if raw_event.result() == Err(MojoError::FailedPrecondition) {
            trap::MojoRemoveTrigger(&shared_data.trap_handle.handle, raw_event.trigger_context())
                .unwrap();
        }

        // Re-arm the trap if the rearming policy is set to automatic
        if matches!(shared_data.rearming_policy, RearmingPolicy::Automatic) {
            // The only way this can fail is if the trap has no triggers. This is
            // possible (and fine) if we were the last trigger and just got cancelled.
            // SAFETY: We dropped `trigger_data` already.
            let _ = unsafe { Self::handle_blocking_events_until_armed(&shared_data.trap_handle) };
        }
    }

    /// Add a trigger to the trap, which will fire when its signals become
    /// satisfied (or unsatisfied, depending on `condition`).
    ///
    /// # Safety consideration
    /// The callback passed in to add_trigger should be able to account for
    /// both an Ok() MojoResult (the "happy" path) and the following TrapErrors:
    /// * `Cancelled``, which is returned every time a Trap goes out of scope,
    ///   and
    /// * `FailedPrecondition`, which is returned if the conditions for this
    ///   trigger ever become impossible (e.g., if one end of a pipe is closed,
    ///   such that the other end will never again become readable)
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
        let (trigger_data_ptr, id): (*const Trigger, _) = {
            let mut trigger_registry = self.trigger_registry.lock().unwrap();
            let id = TriggerId(trigger_registry.next_trigger_id);
            trigger_registry.next_trigger_id += 1;

            let trigger_data = Arc::new(Trigger {
                callback: Mutex::new(Box::new(callback)),
                owner: Arc::downgrade(&self.trigger_registry),
                shared_data: Arc::downgrade(&self.shared_data),
                trigger_id: id,
            });
            if trigger_registry.trigger_map.insert(id, trigger_data.clone()).is_some() {
                panic!("trigger_id unexpectedly already exists in trigger_map")
            }
            (Arc::downgrade(&trigger_data).into_raw(), id)
        };

        // Note: The mojo API says that it's invalid to add multiple traps on the
        // same handle, but it seems that requirement was silently dropped when
        // we transferred to ipcz.
        trap::MojoAddTrigger(
            &self.shared_data.trap_handle.handle,
            handle_to_trap.get_untyped_handle(),
            signals,
            condition,
            trigger_data_ptr as usize,
        )
        .expect("The Trap class handles all possible failures when adding a trigger");

        return id;
    }

    /// Remove the trigger with the given ID from the trap.
    ///
    /// The handler will be invoked one last time with a `Cancelled` result.
    /// Note that the handler is invoked asynchronously, and may not finish
    /// before this function returns.
    ///
    /// # Possible Error Codes:
    /// - `NotFound`: If the trigger with that ID has already been removed
    pub fn remove_trigger(&mut self, trigger_id: TriggerId) -> MojoResult<()> {
        // The handler will do cleanup of the trigger's weak pointer and
        // remove the key from our internal map.
        let context = Arc::as_ptr(
            self.trigger_registry
                .lock()
                .unwrap()
                .trigger_map
                .get(&trigger_id)
                .ok_or(MojoError::NotFound)?,
        ) as usize;
        trap::MojoRemoveTrigger(&self.shared_data.trap_handle.handle, context)
    }

    /// Removes *all* triggers from this Trap.
    ///
    /// See the documentation of `remove_trigger` for caveats.
    pub fn clear_triggers(&mut self) {
        let triggers_to_remove: Vec<TriggerId> = {
            let registry = self.trigger_registry.lock().unwrap();
            registry.trigger_map.keys().copied().collect()
        };
        for trigger_id in triggers_to_remove {
            // A failure here means the trigger was already removed
            let _ = self.remove_trigger(trigger_id);
        }
    }

    /// Attempt to arm the trap, handling any existing triggers immediately and
    /// synchronously until the trap is armed.
    ///
    /// If the trigger handler does not remove events (e.g. by reading the
    /// corresponding message from a pipe), this will cause an infinite loop.
    ///
    /// # Possible Error Codes
    /// - `NotFound` if the trap has no triggers.
    pub fn arm(&mut self) -> MojoResult<()> {
        // SAFETY: `handle_blocking_events_until_armed` explicitly allows `arm`
        // to call it.
        unsafe { Self::handle_blocking_events_until_armed(&self.shared_data.trap_handle) }
    }

    /// Attempt to arm the trap. If it would fire immediately, it returns a
    /// subset of the events that would it to fire; the caller must somehow
    /// handle them before the trap can be armed.
    pub fn try_arm(&mut self) -> TryArmResult {
        const MAX_BLOCKING_EVENTS: usize = 16;
        let mut buf = [const { mem::MaybeUninit::uninit() }; MAX_BLOCKING_EVENTS];

        match trap::MojoArmTrap(&self.shared_data.trap_handle.handle, Some(&mut buf)) {
            ArmResult::Armed => TryArmResult::Success,
            ArmResult::Blocked(events) => {
                TryArmResult::BlockingEvents(events.iter().map(Into::into).collect())
            }
            ArmResult::Failed(_) => TryArmResult::NoTriggers,
        }
    }

    /// Attempt to arm the trap; if there are blocking events, invoke the
    /// handler on each of them until none remain and the trap is armed.
    ///
    /// The provided handler must at least attempt to make progress on clearing
    /// the events, or this will loop forever.
    ///
    /// # Safety:
    /// This function should only be called from `arm` and
    /// `handle_event_from_callback`. In the latter case, the caller must
    /// not hold a reference to any trigger's `TriggerData`.
    unsafe fn handle_blocking_events_until_armed(raw_trap: &TrapHandle) -> MojoResult<()> {
        const MAX_BLOCKING_EVENTS: usize = 16;
        let mut buf = [const { mem::MaybeUninit::uninit() }; MAX_BLOCKING_EVENTS];

        loop {
            let blocking_events: &[RawTrapEvent] =
                match trap::MojoArmTrap(&raw_trap.handle, Some(&mut buf)) {
                    ArmResult::Blocked(events) => events,
                    ArmResult::Armed => return Ok(()),
                    ArmResult::Failed(e) => {
                        return Err(e);
                    }
                };
            for blocking_event in blocking_events {
                // SAFETY:
                //
                // - If called from `arm`: `arm` takes `&mut self` so it can't be
                // called concurrently.
                // - If called from `handle_event_from_callback`: the caller
                // promises it doesn't have a reference to `trigger_data`.
                // - In both cases: Removing a trigger requires `&mut self`, so the
                // trap won't fire a `Cancelled` event while we're doing this.
                // It won't fire any other event because it's disarmed.
                //
                // Therefore, nobody else has a reference to `trigger_data`.
                let trigger_data = unsafe { Self::get_trigger_data_from_event(blocking_event) };
                // The unwraps should succeed because the trap is still alive
                (trigger_data.unwrap().callback.lock().unwrap())(&blocking_event.into());
                if blocking_event.result() == Err(MojoError::FailedPrecondition) {
                    // We know the trigger hasn't been removed yet because it's blocking!
                    trap::MojoRemoveTrigger(&raw_trap.handle, blocking_event.trigger_context())
                        .unwrap();
                }
            }
        }
    }

    /// Takes a given RawTrapEvent and determines the
    /// associated Trigger. Also decrements the trigger's `Weak` count if
    /// the trigger will never again be called.
    ///
    /// # Safety
    ///
    /// This function must not be called concurrently with the same `context`
    /// value. After it is called with a `Cancelled` result, it must never be
    /// called again with the same context.
    unsafe fn get_trigger_data_from_event(raw_event: &RawTrapEvent) -> Option<Arc<Trigger>> {
        // The context value is really a weak pointer to this event's trigger.
        let trigger_data_ptr = raw_event.trigger_context() as *const Trigger;
        // SAFETY: This pointer was created by `Weak::into_raw` in `add_trigger`
        // The function's safety requirements guarantee that the pointer is still
        // valid and no other `Weak` owns its ref-count. If the result isn't
        // `Cancelled`, we'll release our hold on the ref-count when we forget this
        // below.
        let trigger_data_weak = unsafe { Weak::from_raw(trigger_data_ptr) };
        let trigger_data = trigger_data_weak.upgrade();

        // Any result besides `Cancelled` means that this `context` value might
        // be re-used, so make sure we don't decrement the weak count when our
        // reconstituted `Weak` pointer is dropped.
        if raw_event.result() != Err(MojoError::Cancelled) {
            mem::forget(trigger_data_weak);
        }

        return trigger_data;
    }

    fn remove_cancelled_trigger(trigger_registry: &mut TriggerRegistry, trigger_id: TriggerId) {
        // If the `unwrap` panics, we've tried to remove a nonexistent handle, which
        // should be impossible unelss there is an error in how we are handling
        // handles.
        let trigger_data: Arc<_> = trigger_registry.trigger_map.remove(&trigger_id).unwrap();

        // If the caller managed the ref counts correctly, `trigger_data`'s inner
        // data should be dropped after this call.
        debug_assert_eq!(1, Arc::strong_count(&trigger_data), "unexpected strong ref");
        debug_assert_eq!(0, Arc::weak_count(&trigger_data), "unexpected weak ref");
    }
}

impl Drop for Trap {
    fn drop(&mut self) {
        // The C API will do this for us, but doing it here means we don't
        // depend on drop order to make sure Arcs are alive.
        self.clear_triggers();
    }
}

struct TrapSharedData {
    // The actual underlying trap object.
    trap_handle: TrapHandle,
    // Whether the trap should automatically rearm each time a trigger fires or not
    rearming_policy: RearmingPolicy,
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
// While `TrapHandle` wraps the C Mojo API functions, this struct
// serves as the object *referenced by* the opaque `context` integer
// passed to that API.
//
// Thus this struct is referenced when making the necessary
// TrapHandle calls "under the hood".
struct Trigger {
    // TODO(crbug.com/470438844): These Mutexes *may* be imposing an unnecessary
    // overhead on our end users (though "traps can be called back on any thread"
    // is a pretty tricky constraint). See if we can replace some or all of them
    // with sequenced equivalents.
    #[allow(clippy::type_complexity)]
    callback: Mutex<Box<dyn FnMut(&TrapEvent) + Send + 'static>>,
    // Trigger points back to the TriggerRegistry that "owns" it.
    owner: Weak<Mutex<TriggerRegistry>>,
    shared_data: Weak<TrapSharedData>,
    trigger_id: TriggerId,
}
