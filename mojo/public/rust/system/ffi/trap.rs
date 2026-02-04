// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines (mostly) safe Rust wrappers around the Mojo trap API.
//!
//! Not all C API functions are included yet. More can be added as-needed by
//! following the example of existing wrappers.

chromium::import! {
  "//mojo/public/rust/system:mojo_c_system_bindings" as raw_ffi;
}

use crate::handles::*;
use crate::result::*;

/// This module defines various auxiliary types which are used as arguments or
/// return values by the wrapper functions.
pub mod types {
    use super::*;

    /// An event reported by the trap; used as the argument to the trap's
    /// handler.
    #[repr(transparent)]
    #[derive(Clone, Debug)]
    pub struct TrapEvent(raw_ffi::MojoTrapEvent);

    impl TrapEvent {
        /// Retrieve the context value provided in `Trap::add_trigger`.
        pub fn trigger_context(&self) -> usize {
            self.0.trigger_context
        }

        /// Why the trigger fired. Possible return values:
        /// - Ok(()): a specified signal occurred.
        /// - Err(FailedPrecondition): a signal can no longer happen on the
        ///   handle.
        /// - Err(Cancelled): the trigger was removed (explicitly or by closing
        ///   the trap or the watched handle).
        pub fn result(&self) -> MojoResult<()> {
            MojoError::result_from_code(self.0.result)
        }

        /// The handle's current and possible signals as of triggering.
        pub fn signals_state(&self) -> SignalsState {
            SignalsState(self.0.signals_state)
        }
    }

    /// The EventHandler that will be called when a trigger fires.
    ///
    /// This type is ABI-compatible with the (non-null) MojoTrapEventHandler C
    /// type. Due to ABI compatibility, *const T and &T are identical, and
    /// TrapEvent is #[repr(transparent)] and thus has the same layout as
    /// MojoTrapEvent).
    ///
    /// # Safety invariant
    ///
    /// The Mojo C API guarantees that once `EventHandler` is called with
    /// `MOJO_RESULT_CANCELLED`, then it won't be called again with the same
    /// `context`.
    pub type EventHandler = extern "C" fn(&TrapEvent);

    // FOR_RELEASE(https://crbug.com/458796903): Don't hardcode these
    /// An enum describing the two possible conditions for a trigger to fire:
    /// either when a signal becomes satisfied, or when it become unsatisfied.
    #[derive(Clone, Copy, Debug)]
    pub enum TriggerCondition {
        /// The condition triggers when *any* observed signal transitions from
        /// satisfied to unsatisfied.
        TriggerWhenUnsatisfied = 0,
        /// The condition triggers when *any* observed signal transitions from
        /// unsatisfied to satisfied.
        TriggerWhenSatisfied = 1,
    }

    impl TriggerCondition {
        pub(super) fn into_raw(self) -> raw_ffi::MojoTriggerCondition {
            self as _
        }
    }

    bitflags::bitflags! {
      #[derive(Clone, Copy, Default)]
      #[repr(transparent)]
      /// A bitfield describing the possible signals that a trap might observe.
      pub struct HandleSignals: raw_ffi::MojoHandleSignals {
          const READABLE = 1 << 0;
          const WRITABLE = 1 << 1;
          const PEER_CLOSED = 1 << 2;
          const NEW_DATA_READABLE = 1 << 3;
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
    pub struct SignalsState(pub raw_ffi::MojoHandleSignalsState);

    impl SignalsState {
        /// Generates a new SignalsState
        pub fn new(satisfied: HandleSignals, satisfiable: HandleSignals) -> SignalsState {
            SignalsState(raw_ffi::MojoHandleSignalsState {
                satisfied_signals: satisfied.bits(),
                satisfiable_signals: satisfiable.bits(),
            })
        }

        /// Returns the bitfield of the satisfied signals
        pub fn satisfied(&self) -> HandleSignals {
            HandleSignals::from_bits_truncate(self.0.satisfied_signals)
        }

        /// Returns the bitfield of the satisfiable signals
        pub fn satisfiable(&self) -> HandleSignals {
            HandleSignals::from_bits_truncate(self.0.satisfiable_signals)
        }

        /// Return the wrapped Mojo FFI struct.
        pub fn into_raw(self) -> raw_ffi::MojoHandleSignalsState {
            self.0
        }

        /// Get a pointer to the inner struct for FFI calls.
        pub fn as_mut_ptr(&mut self) -> *mut raw_ffi::MojoHandleSignalsState {
            &mut self.0 as *mut _
        }
    }

    impl std::default::Default for SignalsState {
        fn default() -> Self {
            SignalsState(raw_ffi::MojoHandleSignalsState {
                satisfied_signals: 0,
                satisfiable_signals: 0,
            })
        }
    }

    /// The result of arming a trap.
    pub enum ArmResult<'a> {
        /// The trap was successfully armed with no blocking events.
        Armed,
        /// An event would have triggered immediately, blocking the arming.
        /// Contains an arbitrary subset of the blocking event(s). The
        /// returned slice is a reborrow of the buffer passed to
        /// `MojoArmTrap`.
        Blocked(&'a [TrapEvent]),
        /// Arming failed due to a different Mojo error, or no buffer of
        /// blocking events was provided.
        Failed(MojoError),
    }
}

pub use types::*;

/// Create a trap that calls `handler` for each event. The handler may run
/// on any thread. See the documentation for `MojoAddTrigger` for more
/// information about triggers.
///
/// Generally, `handler` will be only called while the trap is armed. However,
/// it will be called while disarmed upon a removing a trigger which happens
/// in two cases:
/// - The trigger is explicitly removed with `MojoRemoveTrigger`
/// - The trap's handle is closed
///
/// # Safety guarantees
/// - `handler` will *not* be called after the trap is dropped.
///
/// This guarantee may be helpful to meet the safety requirements involved
/// in reinterpreting `TrapEvent::trigger_context` as a reference:
/// the dereference will be safe if the lifetime of the referent is
/// guaranteed to be longer than the lifetime of the trap.
///
/// # Possible Error Codes:
/// - `ResourceExhausted`: If the trap handler was unable to be created (e.g.
///   because the process ran out of possible handle values)
pub fn MojoCreateTrap(handler: EventHandler) -> MojoResult<UntypedHandle> {
    // First we need to transmute the passed-in handler to match the C type.
    // SAFETY: The EventHandler type is ABI-compatible with MojoTrapEventHandler.
    let handler_ptr = unsafe {
        std::mem::transmute::<extern "C" fn(&TrapEvent), extern "C" fn(*const raw_ffi::MojoTrapEvent)>(
            handler,
        )
    };
    let mut handle: raw_ffi::MojoHandle = 0;

    // SAFETY: Function pointers are always valid. The options pointer is
    // permitted to be null.
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoCreateTrap(
            Some(handler_ptr),
            std::ptr::null(), // This function doesn't have any options
            std::ptr::from_mut(&mut handle),
        )
    });

    // SAFETY: We just got this value from Mojo
    ret.map(|_| unsafe { UntypedHandle::wrap_raw_value(handle) })
}

/// Add a trigger to the trap, which will cause the trap's handler (set when the
/// trap was created) to be invoked with the given `context` value in its
/// `TrapEvent`. Triggers are only invoked if the trap is armed (see
/// `MojoArmTrap`), except for cancellations.
///
/// `monitored_handle` must be either a data pipe or message pipe handle.
/// `signals` is a bitfield indicating which signals should be observed on that
/// handle. The handler will be invoked whenever *any* observed signal
/// becomes satisfied (if `condition` is `TriggerWhenSatisfied), or when any
/// signal becomes unsatisfied (if `condition` is `TriggerWhenUnsatisfied`).
///
/// # Safety Notes
/// `context` is a pointer-size integer that is used to tell the handler which
/// trigger invoked it. In almost all cases it will be interpreted as an actual
/// pointer.
///
/// Therefore, this function is not itself `unsafe`, the provided handler must
/// ensure that the value `context` points to is still alive when dereferenced.
///
/// ## Guarantees
/// The given `context` will not be passed to `EventHandler` after either of
/// the following happen:
/// - The trap handle is closed.
/// - `MojoRemoveTrigger` is called on this trap with `context`.
///
/// # Requirements
/// - `trap_handle` is actually a trap handle.
/// - `monitored_handle` is either a message or data pipe handle.
/// - The trap must not already have a trigger with `context`.
///
/// # Possible Error Codes:
/// - `InvalidArgument` if one of the handles had an incorrect type
/// - `AlreadyExists` if the trap already has a trigger with `context`.
pub fn MojoAddTrigger(
    trap_handle: &UntypedHandle,
    monitored_handle: &UntypedHandle,
    signals: HandleSignals,
    condition: TriggerCondition,
    context: usize,
) -> MojoResult<()> {
    // SAFETY: The `UntypedHandle` type guarantees its contents are alive.
    // The options pointer is allowed to be null.
    MojoError::result_from_code(unsafe {
        raw_ffi::MojoAddTrigger(
            trap_handle.handle_value.into(),
            monitored_handle.handle_value.into(),
            signals.bits(),
            condition.into_raw(),
            context,
            std::ptr::null(), // This function doesn't take options
        )
    })
}

/// Remove the trigger associated with `context` from the trap.
///
/// After removal, this function will invoke the trigger with `context` one
/// last time, with the result `Cancelled` inside the trap event. Removal
/// may block an arbitrarily long time if the handler is already executing
/// elsewhere.
///
/// # Safety Guarantees
/// - The given `context` will not be passed to `EventHandler` after the
///   `remove_trigger` call returns.
///
/// # Requirements
/// - `trap_handle` must actually be a trap handle.
/// - `trap_handle` must have a trigger associated with `context`.
///
/// # Possible Error Codes:
/// - `InvalidArgument`: if `trap_handle` is not actually a trap handle
/// - `NotFound`: if the trap has no trigger associated with `context`.
pub fn MojoRemoveTrigger(trap_handle: &UntypedHandle, context: usize) -> MojoResult<()> {
    // SAFETY: The `UntypedHandle` type guarantees the handle is valid. The options
    // pointer is allowed to be null.
    MojoError::result_from_code(unsafe {
        raw_ffi::MojoRemoveTrigger(
            trap_handle.handle_value.into(),
            context,
            std::ptr::null(), // This function doesn't take options
        )
    })
}

/// Attempt to arm the provided trap, causing its handler to be unvoked the next
/// time any trigger's condition is satisfied.
///
/// Before invoking the handler, the trap will disarm itself, and must be
/// re-armed before triggers can resume firing. Often this is done as part of
/// the handler.
///
/// The trap will refuse to arm if any trigger would fire immediately. If this
/// is the case, this function can provide the "blocking events" which are
/// would cause it to fire. These are the `TrapEvent`s with which the handler
/// would have been invoked if the trap were already armed. In order to arm the
/// trap, the user must first handle all of these events in an appropriate way.
///
/// For example, if the trap is watching for incoming messages on a pipe, the
/// blocking events will correspond to messages that are waiting to be read, and
/// the user should read all of them before re-arming the trap.
///
/// # Safety Guarantees
/// - If this function returns `Blocked(s)`, then the slice `s` is a reborrow of
///   `blocking_events`, and the first `s.len()` elements of `blocking_events`
///   are initialized.
///
/// # Requirements
/// - `trap_handle` must actually be a trap handle.
///
/// # Possible Error Codes
/// - `InvalidArgument` if `trap_handle` is not actually a trap handle.
/// - `NotFound` if the trap has no triggers.
/// - `FailedPrecondition` if `blocking_events` was None but the trap failed to
///   arm due to blocking events.
pub fn MojoArmTrap<'a>(
    trap_handle: &UntypedHandle,
    mut blocking_events: Option<&'a mut [std::mem::MaybeUninit<TrapEvent>]>,
) -> ArmResult<'a> {
    let mut num_events = blocking_events
        .as_ref()
        // If the caller passes a bigger slice, populate only the first part.
        .map_or(0, |b| u32::try_from(b.len()).unwrap_or(u32::MAX));

    let (blocking_events_ptr, num_events_ptr) = match blocking_events {
        Some(ref mut slice) => {
            // For each blocking event, we must initialize `struct_size`. Otherwise,
            // `MojoArmTrap` below will complain about receiving an invalid argument.

            for event in slice.iter_mut() {
                // A reference to the `struct_size` field of the TrapEvent.
                // SAFETY: TrapEvent is a #[repr(transparent)] wrapper around `MojoTrapEvent`,
                // which itself is a #[repr(C)] struct with `struct_size` as the first element.
                // Therefore, the `struct_size` field begins at the same memory address as the
                // `TrapEvent` as a whole.
                let size_ref = unsafe {
                    std::mem::transmute::<
                        &mut std::mem::MaybeUninit<TrapEvent>,
                        &mut std::mem::MaybeUninit<u32>,
                    >(event)
                };
                // The size of the struct will certainly fit into 32 bits.
                let size = std::mem::size_of::<raw_ffi::MojoTrapEvent>().try_into().unwrap();
                size_ref.write(size);
            }
            (slice.as_mut_ptr().cast(), std::ptr::from_mut(&mut num_events))
        }
        None => (std::ptr::null_mut(), std::ptr::null_mut()),
    };

    // SAFETY: The `UntypedHandle` type ensures the handle is live. The options
    // pointer is allowed to be null. The other pointers are either both validly
    // null, or derived from references. `num_events` holds at most the length
    // of `blocking_events`, so the function will not try to write past the end.
    let result = MojoError::result_from_code(unsafe {
        raw_ffi::MojoArmTrap(
            trap_handle.handle_value.into(),
            std::ptr::null(), // This function doesn't have any options
            num_events_ptr,
            blocking_events_ptr,
        )
    });

    match (result, blocking_events) {
        (Ok(()), _) => ArmResult::Armed,
        (Err(MojoError::FailedPrecondition), Some(blocking_events)) => {
            let count = num_events as usize;
            debug_assert!(count > 0);
            debug_assert!(count <= blocking_events.len());
            // Tranmuting into &mut [TrapEvent] is safe because:
            // * it is derived from blocking_events, which is already a valid Rust slice
            //   managing the lifetime
            // * MojoArmTrap guarantees it initialized the first `num_events` elements
            let initialized_slice = unsafe {
                std::slice::from_raw_parts_mut(blocking_events.as_mut_ptr().cast(), count)
            };
            ArmResult::Blocked(initialized_slice)
        }
        (Err(err), _) => ArmResult::Failed(err),
    }
}
