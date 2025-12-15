//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::mojo_types::*;
use crate::mojo_types::{Handle, MojoResult, Trappable, UntypedHandle};

use std::convert::TryFrom;
use std::mem;
use std::ptr;

chromium::import! {
  pub "//mojo/public/rust:mojo_ffi";
}

use mojo_ffi::types;

/// # Safety requirements
/// * `Self` is a `struct` that has `struct_size: u32` as the first field (at
///   offset 0).
/// * `Self` is a `#[repr(C)]` `struct`
unsafe trait MojoStructWithStructSizeAsFirstField: Sized {
    fn init_size(mojo_struct: &mut mem::MaybeUninit<Self>) {
        // SAFETY:
        // * Alignment: guaranteed by
        //     - `impl` safety requirements which promise `#[repr(C]`
        //     - https://doc.rust-lang.org/reference/type-layout.html#r-layout.repr.c.struct.align
        // * Validity: `impl` safety requirements promises `u32` at offset 0
        let mojo_size = unsafe {
            std::mem::transmute::<&mut mem::MaybeUninit<Self>, &mut mem::MaybeUninit<u32>>(
                mojo_struct,
            )
        };

        // `unwrap` should be ok, because we expect sizes of all Mojo struct
        // to fit into `u32`.
        let self_size: u32 = std::mem::size_of::<Self>().try_into().unwrap();
        mojo_size.write(self_size);
    }
}

// # Safety requirements
//
// * Macro user has to verify that type `$t` is `#[repr(C)]` FOR_RELEASE: Maybe
//   this safety requirement can be avoided somehow? (One idea is to provide a
//   custom `#[derive(MojoStructWithStructSize)]` which checks `repr(C)` but of
//   course `bindgen` wouldn't be able to do this...
macro_rules! unsafe_impl_mojo_struct_with_size_as_first_field {
    ($t:ty) => {
        // Verify that `size` is a `u32` field at offset 0.
        const _: () = {
            assert!(0 == std::mem::offset_of!($t, struct_size));
            fn _check_type_of_size_field_at_compile_time(mojo_struct: $t) {
                let _: u32 = mojo_struct.struct_size;
            }
        };

        // SAFETY:
        // * `struct_size: u32` at offset 0: Verified in `const` above.
        // * `#[repr(C)]: Guaranteed by macro user (see safety requirements)
        unsafe impl MojoStructWithStructSizeAsFirstField for $t {}
    };
}

// SAFETY: MojoTrapEvent is guaranteed to be #[repr(C)] by bindgen.
unsafe_impl_mojo_struct_with_size_as_first_field! {mojo_ffi::MojoTrapEvent}

/// FOR_RELEASE(https://crbug.com/458796903): Instead of hardcoding the
/// constants (e.g. `0` and `1`), it'd be nicer to access
/// MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED here, but the bindings we
/// have right now don't export that. We should change that.
#[derive(Clone, Copy, Debug)]
pub enum TriggerCondition {
    /// The condition triggers when *any* observed signal transitions from
    /// satisfied to unsatisfied.
    SignalsUnsatisfied = 0,
    /// The condition triggers when *any* observed signal transitions from
    /// unsatisfied to satisfied.
    SignalsSatisfied = 1,
}

impl TriggerCondition {
    fn to_raw(self) -> types::MojoTriggerCondition {
        self as _
    }
}

/// An event reported by `Trap`.
#[repr(transparent)]
#[derive(Clone, Copy, Debug)]
pub struct RawTrapEvent(mojo_ffi::MojoTrapEvent);

impl RawTrapEvent {
    /// The context provided in `Trap::add_trigger`.
    pub fn trigger_context(&self) -> usize {
        self.0.trigger_context
    }

    /// Why the trigger fired:
    /// * Okay: a specified signal occurred.
    /// * FailedPrecondition: a signal can no longer happen on the handle.
    /// * Cancelled: the trigger was removed (explicitly or by closure).
    pub fn result(&self) -> MojoResult {
        MojoResult::from_code(self.0.result)
    }

    /// The handle's current and possible signals as of triggering.
    pub fn signals_state(&self) -> SignalsState {
        SignalsState(self.0.signals_state)
    }
}

// Due to ABI compatibility, *const T and &T are identical; RawTrapEvent is
// #[repr(transparent)] and thus has the same layout as MojoTrapEvent).
pub type EventHandler = extern "C" fn(&RawTrapEvent);

/// The result of arming a `RawTrap`.
pub enum ArmResult<'a> {
    /// The trap was successfully armed with no blocking events.
    Armed,
    /// An event would have triggered immediately, blocking the arm. Contains
    /// the event(s). The returned slice is a reborrow of the buffer passed to
    /// `RawTrap::arm`.
    Blocked(&'a [RawTrapEvent]),
    /// Arming failed due to a different Mojo error. If no buffer was passed in
    /// to `arm` but there were blocking events Failed(FailedPrecondition) will
    /// be returned.
    Failed(MojoResult),
}

/// `RawTrap` is a thin wrapper for Mojo traps. Each instance has an
/// associated `EventHandler` function which is called for each notification.
///
/// This is meant to be the "lower-level" API for traps, and not used
/// directly by most clients, hence the "Raw" designation.
///
/// In particular, `add_trigger` (as described in the documentation for that
/// function) presents a constraint on the caller: once the caller passes a
/// context to `add_trigger` it must ensure that memory is not freed before
/// the associated event fires.
///
/// A follow-on CL implements `Trap`, which will provide a wrapper to handle
/// management of that safely.
pub struct RawTrap {
    handle: UntypedHandle,
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
    pub fn new(handler: EventHandler) -> Result<RawTrap, MojoResult> {
        let handler_ptr = unsafe {
            // SAFETY: Here we take the passed-in handler and transmute it.
            // This is safe due to ABI compatibility (*const T and &T are
            // identical; RawTrapEvent is #[repr(transparent)] and thus
            // has the same layout as MojoTrapEvent).
            mem::transmute::<
                extern "C" fn(&RawTrapEvent),
                extern "C" fn(*const mojo_ffi::MojoTrapEvent),
            >(handler)
        };
        let mut handle = UntypedHandle::invalid();
        let result = unsafe {
            // SAFETY:
            // * MojoCreateTrap is given a valid function pointer.
            // * `handle`'s pointer cast is OK since `UntypedHandle` is repr(transparent)
            //   for MojoHandle
            MojoResult::from_code(mojo_ffi::MojoCreateTrap(
                Some(handler_ptr),
                // This will live until after the call to MojoCreateTrap returns.
                mojo_ffi::MojoCreateTrapOptions::new(0).as_ptr(),
                handle.as_mut_ptr(),
            ))
        };

        match result {
            MojoResult::Okay => Ok(RawTrap { handle }),
            e => Err(e),
        }
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
    pub fn add_trigger<H: Handle>(
        &self,
        handle_to_trap: &H,
        signals: HandleSignals,
        condition: TriggerCondition,
        context: usize,
    ) -> MojoResult
    where
        H: Trappable,
    {
        unsafe {
            // SAFETY: MojoAddTrigger requires two handles.
            // The first argument is a handle to the Trap object.
            // The second argument is the handle we're trapping on.
            // This is safe provided that both handles are valid (which
            // is enforced via typing here) AND that the caller ensures the
            // constraints on `context` noted earlier.
            MojoResult::from_code(mojo_ffi::MojoAddTrigger(
                self.handle.get_native_handle(),
                handle_to_trap.get_native_handle(),
                signals.bits(),
                condition.to_raw(),
                context,
                mojo_ffi::MojoAddTriggerOptions::new(0).as_ptr(),
            ))
        }
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
    pub fn remove_trigger(&self, context: usize) -> MojoResult {
        unsafe {
            // SAFETY: As noted above, the caller must ensure `context` is valid.
            MojoResult::from_code(mojo_ffi::MojoRemoveTrigger(
                self.handle.get_native_handle(),
                context,
                mojo_ffi::MojoRemoveTriggerOptions::new(0).as_ptr(),
            ))
        }
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
        mut blocking_events: Option<&'a mut [mem::MaybeUninit<RawTrapEvent>]>,
    ) -> ArmResult<'a> {
        let mut num_events = blocking_events
            .as_ref()
            // If the caller passes a bigger slice, populate only the first part.
            .map_or(0, |b| u32::try_from(b.len()).unwrap_or(u32::MAX));

        let (blocking_events_ptr, num_events_ptr) = match blocking_events {
            Some(ref mut slice) => {
                // For each blocking event, we must initialize `struct_size`.
                // Otherwise, `MojoArmTrap` below will complain about receiving an invalid
                // argument.
                // SAFETY: `transmute` is ok here, because `RawTrapEvent` is
                // `#[repr(transparent)]`.
                let raw_slice = unsafe {
                    mem::transmute::<
                        &mut [mem::MaybeUninit<RawTrapEvent>],
                        &mut [mem::MaybeUninit<mojo_ffi::MojoTrapEvent>],
                    >(slice)
                };

                for event in raw_slice.iter_mut() {
                    MojoStructWithStructSizeAsFirstField::init_size(event);
                }
                (slice.as_mut_ptr() as *mut mojo_ffi::MojoTrapEvent, &mut num_events as *mut u32)
            }
            None => (ptr::null_mut(), ptr::null_mut()),
        };

        // SAFETY: This is safe because (1) `num_events_ptr` points to `num_events`
        // which will outlive this call, and (2) `blocking_events_ptr` was
        // initialized with length `num_events` (we know the API will
        // not write more than `num_events`).  If blocking_events is None, both pointers
        // are null, which is valid for this API.
        let result = unsafe {
            MojoResult::from_code(mojo_ffi::MojoArmTrap(
                self.handle.get_native_handle(),
                mojo_ffi::MojoArmTrapOptions::new(0).as_ptr(),
                num_events_ptr,
                blocking_events_ptr,
            ))
        };

        match (result, blocking_events) {
            (MojoResult::Okay, _) => ArmResult::Armed,
            (MojoResult::FailedPrecondition, Some(blocking_events)) => {
                let count = num_events as usize;
                assert!(count <= blocking_events.len());
                assert!(num_events > 0);
                let ptr = blocking_events.as_mut_ptr() as *mut RawTrapEvent;
                // Tranmuting `ptr` into &mut [RawTrapEvent] is safe because:
                // * it is derived from blocking_events, which is already a valid Rust slice
                //   managing the lifetime
                // * MojoArmTrap guarantees it initialized the first `num_event`s
                let initialized_slice = unsafe { std::slice::from_raw_parts_mut(ptr, count) };
                ArmResult::Blocked(initialized_slice)
            }
            _ => ArmResult::Failed(result),
        }
    }
}
