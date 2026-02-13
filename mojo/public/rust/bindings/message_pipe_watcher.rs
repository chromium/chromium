// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This modules defines the `MessagePipeWatcher` type, which wraps one end of a
//! Mojo message pipe, and calls a user-provided function whenever it receives a
//! message.

chromium::import! {
    "//mojo/public/rust/system";
    "//mojo/public/rust/sequences:sequences";
}

use sequences::SequencedTaskRunnerHandle;
use system::message::RawMojoMessage;
use system::message_pipe::MessageEndpoint;
use system::mojo_types::MojoResult;
use system::raw_trap::{HandleSignals, TriggerCondition};
use system::trap::{InitialArmingPolicy, RearmingPolicy, Trap, TrapError, TrapEvent};

// TODO(crbug.com/477584253): Replace std::sync with std::nonpoison if there are
// any non-sequenced versions remaining.
use std::sync::{Arc, Mutex, Weak};

// FOR_RELEASE: Replace some/all of the std::sync imports with chromium
// sequenced equivalents once those are implemented (figure out which, if any,
// need to be replaced).

/// A message pipe endpoint that can send or receive messages. Whenever it
/// receives a message, it invokes a user-provided function with the raw bytes
/// of that message.
pub struct MessagePipeWatcher {
    // Watches the handle for incoming messages.
    trap: Trap,
    // The watcher needs to share its state with the callback that processes
    // messages, so it can read from the endpoint, set the disconnect handler,
    // and so on. Therefore, we store the state in a sub-struct so we can
    // wrap it all in the same Arc.
    shared_state: Arc<MessagePipeWatcherState>,
}

// Stores the shared contents of the watcher type.
struct MessagePipeWatcherState {
    endpoint: MessageEndpoint,
    // The endpoint is already thread-safe, but for this we need our own
    // synchronization. This can be read/written off-sequence, so it must
    // be wrapped in a real Mutex, not a sequenced equivalent.
    disconnect_info: Mutex<DisconnectInfo>,
}

/// Implementation detail for the MessagePipeWatcher. Stores information related
/// to the pipe becoming disconnected, specifically:
/// 1. Whether it's currently connected, and (if so)
/// 2. What handler (if any) to run should it become disconnected.
enum DisconnectInfo {
    ConnectedNoHandler,
    ConnectedWithHandler(Box<dyn FnOnce() + Send + 'static>),
    Disconnected,
}

/// This struct is passed as an argument to the MessagePipeWatcher's handler.
/// It contains a weak reference to the underlying pipe which can only be used
/// to send a response.
pub struct ResponseSender {
    state_weak: Weak<MessagePipeWatcherState>,
}

/// Send a message through the underlying pipe. This function just forwards
/// MessageEndpoint::write from the underlying endpoint.
impl ResponseSender {
    /// Send a message through the underlying pipe, if possible. Return true iff
    /// the message was successfully sent.
    pub fn try_send_response(&self, msg: RawMojoMessage) -> bool {
        if let Some(state) = self.state_weak.upgrade() {
            // FOR_RELEASE: Handle the returned MojoResult
            return state.endpoint.write(msg).is_ok();
        }
        return false;
    }
}

/// This is a convenience trait describing the handler executed by a
/// `MessagePipeWatcher` whenever it receives a message. Because it might be
/// executed on any thread after any amount of time, it must be `Send` and
/// `'static`. It is automatically implemented for all appropriate objects.
///
/// The handler must be a function with no return type whose arguments are:
/// 1. The output of `MessageEndpoint::read`. See that method's documentation
///    for information about `method_handler`'s input type.
/// 2. A (weak) reference to the endpoint in question, so the handler can send a
///    response if it wishes.
pub trait MessagePipeWatcherHandler:
    FnMut(RawMojoMessage, ResponseSender) + Send + 'static
{
}

impl<T> MessagePipeWatcherHandler for T where
    T: FnMut(RawMojoMessage, ResponseSender) + Send + 'static
{
}

impl MessagePipeWatcher {
    /// Create a new MessagePipeWatcher which schedules `message_handler` on
    /// the default sequence whenever the endpoint receives a new message.
    ///
    /// May fail if the system has run out of resources to allocate new Mojo
    /// handles.
    pub fn new(
        endpoint: MessageEndpoint,
        message_handler: impl MessagePipeWatcherHandler,
        disconnect_handler: Option<Box<dyn FnOnce() + Send + 'static>>,
    ) -> Option<Self> {
        Self::new_with_runner(
            endpoint,
            SequencedTaskRunnerHandle::get_current_default().expect(concat!(
                "Must be called in a context with a default SequencedTaskRunner.\n",
                "Use MessagePipeWatcher::new_with_runner instead to provide one explicitly."
            )),
            message_handler,
            disconnect_handler,
        )
    }

    /// Create a new MessagePipeWatcher which schedules `message_handler` on
    /// `runner` (as opposed to the default sequence) whenever `endpoint`
    /// receives a new message.
    ///
    /// When invoked, `message_handler` is passed the output of
    /// `MessageEndpoint::read`. See that method's documentation for
    /// information about `method_handler`'s input type.
    ///
    /// May fail if the system has run out of resources to allocate new Mojo
    /// handles.
    pub fn new_with_runner(
        endpoint: MessageEndpoint,
        runner: SequencedTaskRunnerHandle,
        message_handler: impl MessagePipeWatcherHandler,
        disconnect_handler: Option<Box<dyn FnOnce() + Send + 'static>>,
    ) -> Option<Self> {
        // The main goal of this function is to construct a closure which reads
        // from the `endpoint` and schedules `message_handler` on `runner`.

        let disconnect_info = match disconnect_handler {
            None => DisconnectInfo::ConnectedNoHandler,
            Some(f) => DisconnectInfo::ConnectedWithHandler(f),
        };

        let watcher_state = Arc::new(MessagePipeWatcherState {
            endpoint,
            disconnect_info: Mutex::new(disconnect_info),
        });

        // Lifetime considerations:
        // The following variables are `move`d into the closure:
        // - `watcher_state` is weak because the ownership logically belongs only to the
        //   watcher (we don't want to read them after the watcher dies).
        // - `message_handler` is strong because it's shared by all tasks we post to the
        //   runner, so it needs to be alive until those tasks finish or are dropped.
        // - `runner` is moved directly so that we can guarantee it survives until the
        //   trap is dropped.
        let watcher_state_weak = Arc::downgrade(&watcher_state);
        let message_handler = Arc::new(Mutex::new(message_handler));

        let trigger_handler = move |trap_event: &TrapEvent| {
            Self::read_from_pipe_and_schedule_handler(
                trap_event,
                &watcher_state_weak,
                &message_handler,
                &runner,
            )
        };

        // Define when we trigger the trap (whenever we get a new message)
        let trigger_signals: HandleSignals = HandleSignals::READABLE;
        let trigger_condition: TriggerCondition = TriggerCondition::TriggerWhenSatisfied;

        let trap = Trap::new(RearmingPolicy::Automatic).ok()?;

        // This trap only ever has one trigger active, so no need to track its id
        let _trigger_id = trap.add_trigger(
            &watcher_state.endpoint,
            trigger_signals,
            trigger_condition,
            trigger_handler,
        );

        // The only way this can fail is if there aren't any triggers.
        let _ = trap.arm(InitialArmingPolicy::RunTriggersOnBlockingEvents);

        Some(Self { trap, shared_state: watcher_state })
    }

    /// Consume the MessagePipeWatcher, returning the endpoint. The watching
    /// function will no longer be called for messages that arrive after this
    /// function returns.
    ///
    /// If this watcher held the last reference to its task runner, then any
    /// messages which previously arrived but haven't yet been processed by the
    /// runner might be dropped silently.
    pub fn into_endpoint(mut self) -> MessageEndpoint {
        // Clear any existing triggers, which ends the lifetime of any references
        // they contain.
        self.trap.clear_triggers();
        // We only hand out weak references (into the triggers), and the
        // triggers were cleared when we dropped the trap, so none of them are
        // running. Thus there should be exactly one strong reference.
        return Arc::into_inner(self.shared_state).unwrap().endpoint;
    }

    /// Send a message through the underlying pipe. This function just forwards
    /// MessageEndpoint::write from the underlying endpoint.
    /// # Possible Error Codes:
    /// - `FailedPrecondition`: If the other end of the message pipe is closed.
    pub fn send_message(&self, msg: RawMojoMessage) -> MojoResult<()> {
        self.shared_state.endpoint.write(msg)
    }

    /// Check if the watcher is currently connected
    pub fn is_connected(&self) -> bool {
        !(matches!(
            *self
                .shared_state
                .disconnect_info
                .lock()
                .expect("disconnect_info should never be poisoned"),
            DisconnectInfo::Disconnected
        ))
    }

    /// This is the function that we pass to the underlying trap. It is executed
    /// once for each incoming message. Its job is to:
    /// 1. Make sure it can acquire all necessary locks
    /// 2. Read the most recent message from the pipe
    /// 3. Schedule the user-provided handler on the given task runner, to parse
    ///    and process the raw data from the message.
    ///
    /// Note that since traps expect a function which takes only a TrapEvent, we
    /// actually wrap this function in a closure which owns the latter three
    /// arguments. This ensure that the handler function and task runner
    /// stay alive until all triggers are processed.
    fn read_from_pipe_and_schedule_handler(
        trap_event: &TrapEvent,
        watcher_state: &Weak<MessagePipeWatcherState>,
        message_handler: &Arc<Mutex<impl MessagePipeWatcherHandler>>,
        runner: &SequencedTaskRunnerHandle,
    ) {
        let Some(watcher_state) = watcher_state.upgrade() else {
            // If we can't get the watcher state, then it must have just been
            // dropped, so just finish here.
            return;
        };

        match trap_event.result() {
            Ok(()) => (),
            Err(TrapError::Cancelled) => {
                // This indicates the trigger was cancelled, which can only
                // happen if we're tearing down the trap and therefore the
                // watcher. So no need to do anything.
                return;
            }
            Err(TrapError::FailedPrecondition) => {
                // This indicates the underlying pipe was shut down for some
                // reason, so call the disconnect handler and return.

                // Retrieve the current info and set it to "Disconnected"
                let disconnect_info = std::mem::replace(
                    &mut *watcher_state
                        .disconnect_info
                        .lock()
                        .expect("disconnect_info should never be poisoned"),
                    DisconnectInfo::Disconnected,
                );

                if let DisconnectInfo::ConnectedWithHandler(handler) = disconnect_info {
                    runner.post_task(handler);
                }

                return;
            }
        }
        // Otherwise, there's a new message for us to read

        // The only way `read` can fail is if there isn't a message to read
        let msg =
            watcher_state.endpoint.read().expect("Tried to read a message but there wasn't one");
        // Clone the handler function Arc so we can move it into the closure
        let message_handler_clone = message_handler.clone();
        let response_sender = ResponseSender { state_weak: Arc::downgrade(&watcher_state) };

        runner.post_task(move || {
            // We're posting to a sequence, so try_lock should always succeed
            let mut message_handler = message_handler_clone
                .try_lock()
                .expect("Sequence-bound mutex was locked or poisoned");
            message_handler(msg, response_sender)
        });
    }
}
