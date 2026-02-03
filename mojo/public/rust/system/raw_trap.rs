//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! FOR_RELEASE: We can probably deprecate this entire file and build `trap.rs`
//! directly upon the FFI bindings (which are basically the same code with
//! small tweaks)

chromium::import! {
  "//mojo/public/rust/system:ffi_bindings" as mojo_ffi;
}

use crate::mojo_types::declare_typed_handle;
use mojo_ffi::trap;
use mojo_ffi::{MojoResult, UntypedHandle};

pub use trap::types::TrapEvent as RawTrapEvent;
pub use trap::types::*;

// `RawTrap` is a thin wrapper for Mojo traps. Each instance has an
// associated `EventHandler` function which is called for each notification.
//
// This is meant to be the "lower-level" API for traps, and not used
// directly by most clients, hence the "Raw" designation.
//
// In particular, `add_trigger` (as described in the documentation for that
// function) presents a constraint on the caller: once the caller passes a
// context to `add_trigger` it must ensure that memory is not freed before
// the associated event fires.

declare_typed_handle!(RawTrap);

pub trait Trappable {
    fn get_untyped_handle(&self) -> &UntypedHandle;
}

impl RawTrap {
    /// Create a `RawTrap` that calls `handler` for each event.
    ///
    /// Generally, `handler` will be called while the trap is armed. However,
    /// it will be called while disarmed upon a removing a trigger which happens
    /// in two cases:
    /// * The trigger is explicitly removed with `remove_trigger`
    /// * The trigger's handle is closed
    ///
    /// # Safety guarantees
    ///
    /// * `handler` will *not* be called after RawTrap is dropped.
    ///
    /// This guarantee may be helpful to meet the safety requirements involved
    /// in reinterpreting `RawTrapEvent::trigger_context` as a reference:
    /// the dereference will be safe if the lifetime of the referent is
    /// guaranteed to be longer than the lifetime of RawTrap.
    pub fn new(handler: EventHandler) -> MojoResult<RawTrap> {
        trap::MojoCreateTrap(handler).map(Into::into)
    }

    /// Listen for `signals` on `handle` becoming satisfied or unsatisfied
    /// based on `condition`. Once armed, the EventHandler `handler` (passed
    /// in when `new` was called) may be called for events on this handle.
    ///
    /// At the time the handler is called, the `context` passed in here will
    /// be accessible via `RawTrapEvent::trigger_context` to that handler.
    ///
    /// That context is a pointer-size integer that can be interpreted in any
    /// way. However, in almost all cases this will be used as an actual
    /// pointer.
    ///
    /// Ergo while there are not safety concerns for `add_trigger` itself,
    /// this does mean care is required at the time `context` is dereferenced.
    ///
    /// # Safety guarantees
    ///
    /// * The given `context` will not be passed to `EventHandler` after RawTrap
    ///   is dropped.
    ///
    /// * Additionally, the given `context` will not be passed to EventHandler
    ///   after `remove_trigger` is called with that `context`.
    ///
    /// These guarantees may be helpful to meet the safety requirements involved
    /// in reinterpreting `RawTrapEvent::trigger_context` as a reference:
    /// the dereference will be safe if the lifetime of the referent is
    /// guaranteed to be longer than the lifetime of RawTrap.
    pub fn add_trigger(
        &self,
        monitored_handle: &impl Trappable,
        signals: HandleSignals,
        condition: TriggerCondition,
        context: usize,
    ) -> MojoResult<()> {
        trap::MojoAddTrigger(
            &self.handle,
            monitored_handle.get_untyped_handle(),
            signals,
            condition,
            context,
        )
    }

    /// Remove the handle associated with `context`.
    ///
    /// If successful, this immediately results in a callback to the user
    /// handler with `MojoResult::Cancelled`. No more callbacks will be
    /// issued for `context`'s handle.
    ///
    /// # Safety Guarantees
    ///
    /// * The given `context` will not be passed to `EventHandler` after the
    ///   `remove_trigger` call.
    ///
    /// This guarantee may help meet the safety requirements of dereferencing
    /// `RawTrapEvent::trigger context`: such a dereference will be safe if
    /// the lifetime of the referent is guaranteed to be longer than the
    /// lifetime of RawTrap.
    pub fn remove_trigger(&self, context: usize) -> MojoResult<()> {
        trap::MojoRemoveTrigger(&self.handle, context)
    }

    /// Arm the trap to invoke event handler on any trigger condition.
    ///
    /// `blocking_events` is an optional buffer to hold events that would block
    /// arming the trap, if any exist. If supplied and there were events
    /// blocking the arm, a subslice with the actual events is returned. Its
    /// length must be > 0 and < `u32::MAX`, otherwise this function will panic.
    ///
    /// If arming was successful, the trap remains armed until an event is
    /// received. At that point, it is immediately disarmed.
    pub fn arm<'a>(
        &self,
        blocking_events: Option<&'a mut [std::mem::MaybeUninit<RawTrapEvent>]>,
    ) -> ArmResult<'a> {
        trap::MojoArmTrap(&self.handle, blocking_events)
    }
}
