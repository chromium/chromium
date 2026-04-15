// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This modules defines the `MessagePipeWatcher` type, which wraps one end of a
//! Mojo message pipe, and calls a user-provided function whenever it receives a
//! message.

chromium::import! {
    "//mojo/public/rust/system";
    "//base:sequenced_task_runner";
}

use sequenced_task_runner::SequencedTaskRunnerHandle;
use system::message::RawMojoMessage;
use system::message_pipe::MessageEndpoint;
use system::mojo_types::MojoResult;
use system::trap::{ArmResult, HandleSignals, Trap, TrapError, TrapEvent, TriggerCondition};

// TODO(crbug.com/470438844): Replace some/all of the std::sync imports with
// chromium sequenced equivalents once those are implemented (figure out which,
// if any, need to be replaced).
// TODO(crbug.com/477584253): Replace std::sync with std::nonpoison if there are
// any non-sequenced versions remaining.
use std::sync::{Arc, Mutex, Weak};

/// A message pipe endpoint that can send or receive messages. Whenever it
/// receives a message, it invokes a user-provided function with the raw bytes
/// of that message.
///
/// If the other end of pipe is closed, then the watcher will execute its
/// disconnect handler if one is set.
pub struct MessagePipeWatcher {
    // The watcher needs to share its state with the callback that processes
    // messages, so it can read from the endpoint, set the disconnect handler,
    // and so on. Therefore, we store the state in a sub-struct so we can
    // wrap it all in the same Arc.
    shared_state: Arc<MessagePipeWatcherState>,
}

// Stores the shared contents of the watcher type.
struct MessagePipeWatcherState {
    trap: Trap,
    endpoint: MessageEndpoint,
    // The user-provided callback to run for each message that arrives. This
    // is only ever run on-sequence, so it should be replaced with a sequenced
    // Mutex once we implement those.
    message_handler: Mutex<Box<dyn MessagePipeWatcherHandler>>,
    // The user-provided disconnect handler. This can be read/written
    // off-sequence, so it must be wrapped in a real Mutex, not a sequenced equivalent.
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
    /// Send a message through the underlying pipe, if possible.
    ///
    /// Returns true iff the message was successfully sent. This can only fail
    /// if one or both sides of the pipe have been closed.
    pub fn try_send_response(&self, msg: RawMojoMessage) -> bool {
        if let Some(state) = self.state_weak.upgrade() {
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

        let trap = Trap::new().ok()?;

        let disconnect_info = match disconnect_handler {
            None => DisconnectInfo::ConnectedNoHandler,
            Some(f) => DisconnectInfo::ConnectedWithHandler(f),
        };

        let watcher_state = Arc::new(MessagePipeWatcherState {
            trap,
            endpoint,
            message_handler: Mutex::new(Box::new(message_handler)),
            disconnect_info: Mutex::new(disconnect_info),
        });

        // Lifetime considerations:
        // The following variables are `move`d into the closure:
        // - `watcher_state` is weak because the ownership logically belongs only to the
        //   watcher (we don't want to read them after the watcher dies).
        // - `runner` is moved directly so that we can guarantee it survives until the
        //   trap is dropped. We can't rely on `get_current_default` in the trap handler
        //   because it can run on any thread.
        let watcher_state_weak = Arc::downgrade(&watcher_state);

        let trigger_handler = move |trap_event: &TrapEvent| {
            Self::process_trap_event(trap_event, &watcher_state_weak, &runner)
        };

        // Define when we trigger the trap (whenever we get a new message)
        let trigger_signals: HandleSignals = HandleSignals::READABLE;
        let trigger_condition: TriggerCondition = TriggerCondition::TriggerWhenSatisfied;

        // We only ever have one trigger, so there's no need to remember its ID.
        let _trigger_id = watcher_state.trap.add_trigger(
            &watcher_state.endpoint,
            trigger_signals,
            trigger_condition,
            trigger_handler,
        );

        Self::try_arm(&watcher_state);

        Some(Self { shared_state: watcher_state })
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
    /// once for each incoming message.
    /// 1. Parse the trap event, and
    /// 2. Post a task to handle it, which invokes either the user's callback or
    ///    the disconnect handler.
    ///
    /// The posted task is responsible for rearming the trap, so we won't post
    /// any more tasks until it's processed. The alternative would be looping
    /// through all blocking events here, which could keep this thread busy for
    /// an arbitrary amount of time.
    ///
    /// Note that since traps expect a function which takes only a TrapEvent, we
    /// actually wrap this function in a closure which owns the latter three
    /// arguments. This ensure that the handler function and task runner
    /// stay alive until all triggers are processed.
    fn process_trap_event(
        trap_event: &TrapEvent,
        watcher_state_weak: &Weak<MessagePipeWatcherState>,
        runner: &SequencedTaskRunnerHandle,
    ) {
        let Some(watcher_state) = watcher_state_weak.upgrade() else {
            // If we can't get the watcher state, then it must have just been
            // dropped, so just finish here.
            return;
        };

        match trap_event.result() {
            Ok(()) => (),
            Err(TrapError::Cancelled) => {
                // This indicates the trigger was cancelled, which can only
                // happen if we're tearing down the watcher. So no need to do anything.
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

        // Otherwise, there's a new message for us to read, so post a task to
        // handle it.
        let watcher_state_weak_clone = watcher_state_weak.clone();
        runner.post_task(move || Self::handle_message_on_sequence(watcher_state_weak_clone));
    }

    /// This is the task that we post to the sequenced task runner as a result
    /// of the trap handler firing. Its job is to:
    /// 1. Read the next message from the pipe
    /// 2. Invoke the user callback on that message
    /// 3. Re-arm the trap.
    ///
    /// If the trap fails to rearm due to blocking events, this will call
    /// `process_trap_event`, which will post a task that calls
    /// `handle_message_on_sequence` all over again.
    fn handle_message_on_sequence(watcher_state_weak: Weak<MessagePipeWatcherState>) {
        let Some(watcher_state) = watcher_state_weak.upgrade() else {
            // If we can't get the watcher state, then it must have just been
            // dropped, so just finish here.
            return;
        };

        // Read the message from the pipe and pass it to the user's callback.
        {
            let msg = watcher_state
                .endpoint
                .read()
                .expect("Tried to read a message but there wasn't one");
            let response_sender = ResponseSender { state_weak: watcher_state_weak };
            let mut message_handler = watcher_state
                .message_handler
                .try_lock()
                // This function is run on-sequence, so try_lock should always succeed
                .expect("Sequence-bound mutex was locked or poisoned");
            message_handler(msg, response_sender);
        }

        Self::try_arm(&watcher_state);
    }

    /// Try to rearm the trap. If there are blocking events, instead post a task
    /// to handle the first one of them (which will recursively post tasks to
    /// handle the remainder over time).
    fn try_arm(watcher_state: &Arc<MessagePipeWatcherState>) {
        if let ArmResult::BlockingEvents(vec) = watcher_state.trap.arm() {
            // This will post a task that will recursively call `handle_message_on_sequence`
            Self::process_trap_event(
                &vec[0],
                &Arc::downgrade(watcher_state),
                SequencedTaskRunnerHandle::get_current_default().as_ref().unwrap(),
            );
        }
    }
}
