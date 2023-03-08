// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ffi::{self, raw_ffi, types::MojoTriggerCondition};
use crate::handle::*;
use crate::mojo_types::*;

use std::collections::HashMap;
use std::convert::TryFrom;
use std::mem;
use std::ptr;
use std::sync::{Arc, Mutex, Weak};

#[derive(Clone, Copy, Debug)]
pub enum TriggerCondition {
    /// Trigger on a signal becoming unsatisfied (i.e. going low).
    SignalsUnsatisfied = 0,
    /// Trigger on a signal becoming satisfied (i.e. going high).
    SignalsSatisfied = 1,
}

impl TriggerCondition {
    fn to_raw(self) -> MojoTriggerCondition {
        self as _
    }
}

/// An event reported by `Trap`.
#[repr(transparent)]
#[derive(Clone, Copy, Debug)]
pub struct UnsafeTrapEvent(raw_ffi::MojoTrapEvent);

impl UnsafeTrapEvent {
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

pub type EventHandler = extern "C" fn(&UnsafeTrapEvent);

/// The result of arming an `UnsafeTrap`.
pub enum ArmResult<'a> {
    /// The trap was successfully armed with no blocking events.
    Armed,
    /// An event would have triggered immediately, blocking the arm. Contains
    /// the event(s). The returned slice is a reborrow of the buffer passed to
    /// `UnsafeTrap::arm`.
    Blocked(&'a [UnsafeTrapEvent]),
    /// Arming failed due to a different Mojo error. If no buffer was passed in
    /// to `arm` but there were blocking events Failed(FailedPrecondition) will
    /// be returned.
    Failed(MojoResult),
}

/// A Mojo trap object provides notifications for specified changes on Mojo
/// handles. `UnsafeTrap` is a thin wrapper for Mojo traps. Each instance has an
/// associated `EventHandler` function which is called for each notification.
///
/// This is called "unsafe" because clients generally must use unsafe code.
/// There isn't inherent unsafety; each Mojo handle registered has a context
/// `usize` associated with it that is passed to the event handle. But in most
/// cases this context integer will be interpreted as a raw pointer, which is
/// dangerous.
pub struct UnsafeTrap {
    handle: UntypedHandle,
}

impl UnsafeTrap {
    /// Create a `Trap` that calls `handler` for each event.
    ///
    /// Generally, `handler` will be called while the trap is armed. However,
    /// it will be called while disarmed upon a removing a trigger which happens
    /// in two cases:
    /// * The trigger is explicitly removed with `remove_trigger`
    /// * The trigger's handle is closed
    pub fn new(handler: EventHandler) -> Result<UnsafeTrap, MojoResult> {
        let handler_ptr = unsafe {
            // SAFETY: *const T and &T are ABI compatible and we are assured
            // that `handler` is passed a pointer that lives as long as the
            // function call. The lifetime of `handler`'s argument precludes it
            // from retaining the reference.
            mem::transmute::<
                extern "C" fn(&UnsafeTrapEvent),
                extern "C" fn(*const raw_ffi::MojoTrapEvent),
            >(handler)
        };
        let mut handle = UntypedHandle::invalid();
        let result = unsafe {
            // SAFETY:
            // * MojoCreateTrap is given a valid function pointer (type checked thanks to
            //   bindgen)
            // * `handle`'s pointer cast is OK since `UntypedHandle` is repr(transparent)
            //   for MojoHandle
            MojoResult::from_code(ffi::MojoCreateTrap(
                Some(handler_ptr),
                ffi::MojoCreateTrapOptions::new(0).inner_ptr(),
                handle.as_mut_ptr(),
            ))
        };

        match result {
            MojoResult::Okay => Ok(UnsafeTrap { handle }),
            e => Err(e),
        }
    }

    /// Listen for `signals` on `handle` becoming satisfied or unsatisfied
    /// based on `condition`. Once armed, the event handler may be called for
    /// events on this handle.
    ///
    /// The handler will be passed `context` when triggered for `handle`. This
    /// is a pointer-size integer that can be interpreted in any way. However,
    /// in almost all cases this will be used as an actual pointer.
    ///
    /// The caller should take care that:
    ///   * the event handler safely uses this pointer.
    ///   * the pointer remains valid until `remove_trigger`, or until `self` is
    ///     dropped.
    pub fn add_trigger(
        &self,
        handle: MojoHandle,
        signals: HandleSignals,
        condition: TriggerCondition,
        context: usize,
    ) -> MojoResult {
        unsafe {
            MojoResult::from_code(ffi::MojoAddTrigger(
                self.handle.get_native_handle(),
                handle,
                signals.bits(),
                condition.to_raw(),
                context,
                ffi::MojoAddTriggerOptions::new(0).inner_ptr(),
            ))
        }
    }

    /// Remove the handle associated with `context`. Note that, if successful,
    /// this immediately results in a callback to the user handler with
    /// `MojoResult::Cancelled`. No more callbacks will be issued for
    /// `context`'s handle.
    pub fn remove_trigger(&self, context: usize) -> MojoResult {
        unsafe {
            MojoResult::from_code(ffi::MojoRemoveTrigger(
                self.handle.get_native_handle(),
                context,
                ffi::MojoRemoveTriggerOptions::new(0).inner_ptr(),
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
    /// received. At this point it is immediately disarmed.
    pub fn arm<'a>(
        &self,
        blocking_events: Option<&'a mut [mem::MaybeUninit<UnsafeTrapEvent>]>,
    ) -> ArmResult<'a> {
        // Initialized to the available space in `blocking_events` (or 0), then
        // updated in-place by the Mojo FFI call.
        let mut num_events = blocking_events
            .as_ref()
            .map_or(0, |b| u32::try_from(b.len()).expect("`blocking_events` too large"));

        // Initialize `blocking_events` and set `struct_size` fields which are
        // used by Mojo for struct versioning.
        let mut blocking_events: Option<&'a mut [UnsafeTrapEvent]> =
            blocking_events.map(|blocking_events| {
                for uninit_event in blocking_events.iter_mut() {
                    // `UnsafeTrapEvent` wraps a C FFI struct that is POD and
                    // valid when zero-initialized.
                    let mut event: UnsafeTrapEvent = unsafe { mem::zeroed() };
                    event.0.struct_size = mem::size_of::<UnsafeTrapEvent>() as u32;
                    uninit_event.write(event);
                }

                // Now that all elements are initialized it is sound to
                // assume_init.
                unsafe { mem::MaybeUninit::slice_assume_init_mut(blocking_events) }
            });

        let (blocking_events_ptr, num_events_ptr) = match blocking_events.as_mut() {
            // Casting `*mut TrapEvent` to `*mut raw_ffi::MojoTrapEvent` is
            // sound because the former is a repr(transparent) wrapper for the
            // latter.
            Some(events) => {
                (events.as_mut_ptr() as *mut raw_ffi::MojoTrapEvent, &mut num_events as *mut u32)
            }
            None => (ptr::null_mut(), ptr::null_mut()),
        };

        let result = unsafe {
            MojoResult::from_code(ffi::MojoArmTrap(
                self.handle.get_native_handle(),
                ffi::MojoArmTrapOptions::new(0).inner_ptr(),
                num_events_ptr,
                blocking_events_ptr,
            ))
        };

        match (result, blocking_events) {
            (MojoResult::Okay, _) => ArmResult::Armed,
            (MojoResult::FailedPrecondition, None) => ArmResult::Failed(result),
            (MojoResult::FailedPrecondition, Some(blocking_events)) => {
                assert!(num_events > 0);
                let result = blocking_events.split_at_mut(num_events as usize).0;
                ArmResult::Blocked(result)
            }
            (e, _) => ArmResult::Failed(e),
        }
    }
}

/// Identifies a trigger added to a `Trap`.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct TriggerId(usize);

#[derive(Clone, Copy, Debug)]
pub struct TrapEvent {
    signals_state: SignalsState,
    handle: MojoHandle,
    result: MojoResult,
}

impl TrapEvent {
    /// The handle whose state changed.
    pub fn handle(&self) -> MojoHandle {
        self.handle
    }

    /// Why the trigger fired:
    /// * Okay: a specified signal occurred.
    /// * FailedPrecondition: a signal can no longer happen on the handle.
    /// * Cancelled: the trigger was removed (explicitly or by closure).
    pub fn result(&self) -> MojoResult {
        self.result
    }

    /// The handle's current and possible signals as of triggering.
    pub fn signals_state(&self) -> SignalsState {
        self.signals_state
    }
}

/// A wrapper around `UnsafeTrap` that provides safety for context objects in
/// exchange for some heap memory usage and indirection.'
///
/// `Context` is the client's data associated with each handle. `EventHandler`
/// is the `Fn` type that receives events.
///
/// `Context` must be `Send + Sync + Sized` but no other requirements are
/// imposed. `Handler` receives an immutable reference to `Context`.
pub struct Trap<Context, EventHandler> {
    // The thin wrapper around the Mojo C API for traps.
    trap: UnsafeTrap,
    // The inner data used by the Rust wrapper. It is locked because it can be
    // accessed on multiple threads: Mojo trap callbacks can come from any
    // thread. Shared ownership is used via `Arc` because `Weak` refs are held
    // by each `HandleData`. In turn each `HandleData` is referenced by both
    // `inner` (with `Arc`) and the C side (with `Weak`).
    inner: Arc<Mutex<TrapInner<Context, EventHandler>>>,
}

struct TrapInner<Context, EventHandler> {
    // Triggers are identified by an ID: `add_trigger` returns the ID, and
    // the client can later call `remove_trigger` on said ID to unsubscribe from
    // events on the associated handle. To support removal we maintain a mapping
    // from the client's IDs to our internal per-handle data.
    //
    // Each `HandleData` is owned through an `Arc` since we use `Weak` refs
    // that we pass to the C side as the context usize. The `Weak` refs are
    // converted to raw pointers with `Weak::into_raw()`, passed to the C API,
    // and reconstituted by `Weak::from_raw()` when passed to us by callback.
    context_map: HashMap<TriggerId, Arc<HandleData<Context, EventHandler>>>,
    event_handler: EventHandler,
    next_trigger_id: usize,
}

struct HandleData<Context, EventHandler> {
    context: Context,
    // This is a weak ref because `SafeTrap::inner` is the owner. We cannot rely
    // on the `Weak` referent's existence for soundness since if `Trap` is
    // partially forgotten the owned reference in `context_map` may be dropped
    // while Mojo trap handle isn't. This is important since Rust code cannot
    // rely on `drop` calls for soundness.
    owner: Weak<Mutex<TrapInner<Context, EventHandler>>>,
    handle: MojoHandle,
    trigger_id: TriggerId,
}

// Assert our types have the necessary thread safety traits.
mod asserts {
    use super::*;

    pub fn assert_send<T: Send>() {}
    pub fn assert_sync<T: Sync>() {}

    pub fn assert_traits<Context: Send + Sync, EventHandler: Send>() {
        assert_send::<Trap<Context, EventHandler>>();
        assert_sync::<Trap<Context, EventHandler>>();

        // `Arc<Mutex<TrapInner<Context, EventHandler>>>` is shared between
        // threads by ref, so it must be `Sync`. Normal type checking doesn't
        // catch this since the sharing happens through the C FFI.
        assert_sync::<Arc<Mutex<TrapInner<Context, EventHandler>>>>();

        // `TrapInner<...>` must be `Send` for the same reason as above.
        assert_send::<TrapInner<Context, EventHandler>>();

        // The raw event handler is passed `HandleData` pointers on any thread,
        // so it must be `Sync`.
        assert_sync::<HandleData<Context, EventHandler>>();
    }
}

impl<Context, EventHandler> Trap<Context, EventHandler>
where
    // Context: Sync because it is shared by
    // reference through `raw_handler`
    // calls (from any thread) and Send because
    // it is transitively owned by a
    // Mutex that must be Sync.
    Context: Send + Sync,
    // EventHandler: Send because it is shared by value through `raw_handler`
    // calls, synchronized by a mutex.
    EventHandler: Fn(&TrapEvent, &Context) + Send,
{
    /// Create a `Trap` that calls `handler` upon an event. `handler` takes a
    /// reference to the `Context` type associated with the handle in addition
    /// to `TrapEvent`.
    ///
    /// `handler` must not call `add_trigger` or `remove_trigger` or deadlock is
    /// assured. `handler` may also be called from within an `arm` call.
    ///
    /// `handler` may panic but if `panic_any` is used and the panic value is
    /// not `&str` or `String`, the message may be lost.
    pub fn new(handler: EventHandler) -> Result<Trap<Context, EventHandler>, MojoResult> {
        asserts::assert_traits::<Context, EventHandler>();
        Ok(Trap {
            trap: UnsafeTrap::new(Self::raw_handler)?,
            inner: Arc::new(Mutex::new(TrapInner {
                context_map: HashMap::new(),
                event_handler: handler,
                next_trigger_id: 0,
            })),
        })
    }

    /// Listen for `signals` on `handle` becoming satisfied or unsatisfied
    /// (based on `condition`). Once armed the event handler may be called for
    /// events on this handle.
    ///
    /// # Arguments
    ///
    /// * `handle` - the handle whose signals are to be watched.
    /// * `signals` - the signals to watch for on `handle`.
    /// * `condition` - watch for signals going high or low.
    /// * `context` - user's context for the handle, passed to the handler.
    pub fn add_trigger(
        &self,
        handle: MojoHandle,
        signals: HandleSignals,
        condition: TriggerCondition,
        context: Context,
    ) -> Result<TriggerId, MojoResult> {
        let (handle_data_ptr, id): (*const HandleData<_, _>, _) = {
            // Lock in scope so we unlock before calling into Mojo. If the trap
            // was armed and the new handle has a specified signal, our handler
            // will be called. Since the handler locks `self.inner` we must
            // avoid a deadlock.
            let mut inner = self.inner.lock().unwrap();
            let id = TriggerId(inner.next_trigger_id);
            inner.next_trigger_id += 1;

            let handle_data = Arc::new(HandleData {
                context,
                owner: Arc::downgrade(&self.inner),
                handle,
                trigger_id: id,
            });

            if let Some(_) = inner.context_map.insert(id, handle_data.clone()) {
                panic!("ID unexpectedly exists in context_map");
            }

            // Downgrade to a weak pointer which we logically pass through the C
            // FFI. When Mojo calls back our C handler function, we get the weak
            // pointer back.
            (Arc::downgrade(&handle_data).into_raw(), id)
        };

        match self.trap.add_trigger(handle, signals, condition, handle_data_ptr as usize) {
            MojoResult::Okay => Ok(id),
            e => {
                // Re-lock the mutex to clean up after an error. We know at this point
                // `handle` is not being watched.
                let mut inner = self.inner.lock().unwrap();

                // Drop the `Weak` ref we tried to give to the C side since it did not
                // take it.
                let _: Weak<HandleData<_, _>> = unsafe { Weak::from_raw(handle_data_ptr) };

                // Clean up the `HandleData` object in our map.
                inner.context_map.remove(&id);

                Err(e)
            }
        }
    }

    /// Remove the trigger identified by the `trigger_id` returned by
    /// `add_trigger`.
    pub fn remove_trigger(&self, trigger_id: TriggerId) -> MojoResult {
        let handle_data_ptr: *const HandleData<_, _> = {
            let inner = self.inner.lock().unwrap();
            match inner.context_map.get(&trigger_id) {
                // `Arc::as_ptr` will return the same pointer as
                // `Weak::into_raw` for a `Weak` derived from this `Arc`. This
                // pointer will identify the `HandleData` added to the C side.
                Some(handle_data) => Arc::as_ptr(handle_data),
                None => return MojoResult::NotFound,
            }
        };

        // If successful, this will cause the handler to be called immediately.
        // From the handler we remove the map entry.
        self.trap.remove_trigger(handle_data_ptr as usize)
    }

    /// Arm the trap to invoke event handler on any trigger condition.
    ///
    /// If arming was successful, the trap remains armed until an event is
    /// received. At this point it is immediately disarmed.
    ///
    /// One of three things can happen:
    ///   * The trap was armed successfully. Returns MojoResult::Okay.
    ///   * Failed because events would have triggered immediately. Calls the
    ///     handler with some or all of the events. Returns
    ///     MojoResult::FailedPrecondition.
    ///   * Failed for some other reason. Returns the error.
    pub fn arm(&self) -> MojoResult {
        const MAX_BLOCKING_EVENTS: usize = 16;
        let mut buf = [mem::MaybeUninit::uninit(); MAX_BLOCKING_EVENTS];

        // Try to arm the trap. If blocking events were returned handle them.
        let blocking_events: &[UnsafeTrapEvent] = match self.trap.arm(Some(&mut buf)) {
            ArmResult::Blocked(events) => events,
            ArmResult::Armed => return MojoResult::Okay,
            ArmResult::Failed(e) => return e,
        };

        // Panic on failure because a poisoned mutex is unrecoverable for us.
        let mut inner = self.inner.lock().unwrap();

        for blocking_event in blocking_events {
            // Any error in the calls below is unrecoverable: either a mutex was
            // poisoned, or a needed object no longer exists.
            let handle_data = unsafe { Self::get_handle_data_from_event(blocking_event) };
            Self::call_handler_and_maybe_delete_data(&mut *inner, handle_data, blocking_event);
        }

        MojoResult::FailedPrecondition
    }

    // Unsafe because this fn must only be called once for a given `event`.
    unsafe fn get_handle_data_from_event(
        event: &UnsafeTrapEvent,
    ) -> Arc<HandleData<Context, EventHandler>> {
        // A raw pointer version of `Weak<HandleData<Context, Handler>>`,
        // emulating a weak reference held by the C side.
        let handle_data_ptr = event.trigger_context() as *const HandleData<Context, EventHandler>;

        // We want to grab an actual `Weak<HandleData<Context, Handler>>`. But
        // we must take care to maintain the weak count correctly. The C side
        // still holds a reference unless the event type is Cancelled.
        let handle_data: Weak<HandleData<Context, EventHandler>> =
            if event.result() == MojoResult::Cancelled {
                // The C side effectively drops its reference and never calls
                // this again with `handle_data_ptr`. So we take its reference,
                // later dropping it.
                unsafe { Weak::from_raw(handle_data_ptr) }
            } else {
                // Otherwise, we must clone the weak pointer and then forget it:
                // we reconstitute the C side's `Weak` ref, grab our own, then
                // `forget` the original so the C side still holds its ref.
                let c_handle_data = unsafe { Weak::from_raw(handle_data_ptr) };
                let our_handle_data = c_handle_data.clone();
                mem::forget(c_handle_data);
                our_handle_data
            };

        // Return an `Arc` reference to the handle's data or panic if it no
        // longer exists.
        handle_data.upgrade().expect("could not upgrade handle_data pointer")
    }

    // Remove a handle for which we got a `Cancelled` event.
    fn remove_cancelled_trigger(
        inner: &mut TrapInner<Context, EventHandler>,
        trigger_id: TriggerId,
    ) {
        let handle_data: Arc<_> =
            inner.context_map.remove(&trigger_id).expect("tried to remove handle not present");

        // If the caller managed the ref counts correctly, `handle_data`'s inner
        // data should be dropped after this call.
        assert_eq!(1, Arc::strong_count(&handle_data), "unexpected strong ref");
        assert_eq!(0, Arc::weak_count(&handle_data), "unexpected weak ref");
    }

    fn call_handler_and_maybe_delete_data(
        inner: &mut TrapInner<Context, EventHandler>,
        handle_data: Arc<HandleData<Context, EventHandler>>,
        event: &UnsafeTrapEvent,
    ) {
        let safe_event = TrapEvent {
            signals_state: event.signals_state(),
            handle: handle_data.handle,
            result: event.result(),
        };

        // Call the handler.
        (inner.event_handler)(&safe_event, &handle_data.context);
        if safe_event.result() == MojoResult::Cancelled {
            let trigger_id = handle_data.trigger_id;
            // Drop our `handle_data` ref *before* calling so the assertions
            // in `remove_cancelled_trigger` are correct.
            drop(handle_data);
            Self::remove_cancelled_trigger(inner, trigger_id);
        }
    }

    // Unsafe because this fn must only be called once for a given `event`.
    unsafe fn handle_event_from_callback(event: &UnsafeTrapEvent) {
        let handle_data = unsafe { Self::get_handle_data_from_event(event) };

        // Get ready to call the handler. First get an upgraded reference to
        // `handle_data.owner`. Just like above, if we can't upgrade something
        // is fishy. We should just return.
        let owner = handle_data.owner.upgrade().expect("owning SafeTrapInner no longer exists");

        // This should never deadlock: the lock is taken in `add_trigger` and
        // `remove_trigger`, but `add_trigger` unlocks before calling
        // MojoAddTrigger and only re-locks it if failed. MojoRemoveTrigger also
        // fires an event but `remove_trigger` unlocks before calling it.
        let mut owner = owner.lock().expect("SafeTrapInner lock poisoned");
        Self::call_handler_and_maybe_delete_data(&mut *owner, handle_data, event);
    }

    extern "C" fn raw_handler(event: &UnsafeTrapEvent) {
        // If an error occurred the thread holding `SafeTrap` likely
        // panicked so we can't do much. Catch the panic, print the message,
        // and abort.
        if let Err(e) =
            std::panic::catch_unwind(|| unsafe { Self::handle_event_from_callback(event) })
        {
            // Standard panic objects are a &str or String. Try downcasting to
            // print the message.
            let message: &str = match e.downcast_ref::<&str>() {
                Some(m) => m,
                None => match e.downcast_ref::<String>() {
                    Some(m) => m.as_str(),
                    None => "unknown panic type",
                },
            };

            eprintln!("aborting after panic in C handler function:\n{}", message);
            std::process::abort()
        }
    }
}
