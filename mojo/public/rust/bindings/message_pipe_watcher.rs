// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This modules defines the `MessagePipeWatcher` type, which wraps one end of a
//! Mojo message pipe, and calls a user-provided function whenever it receives a
//! message.

chromium::import! {
    "//mojo/public/rust:mojo_rust_system_api";
    "//mojo/public/rust/sequences:sequences";
}

use mojo_rust_system_api::message_pipe::MessageEndpoint;
use mojo_rust_system_api::mojo_types::{HandleSignals, MojoResult, UntypedHandle};
use mojo_rust_system_api::raw_trap::TriggerCondition;
use mojo_rust_system_api::trap::{Trap, TrapEvent};
use sequences::SequencedTaskRunnerHandle;

use std::sync::{Arc, Mutex, Weak};

// FOR_RELEASE: Add tests once this is finished and trap is functional.

/// A message pipe endpoint that can send or receive messages. Whenever it
/// receives a message, it invokes a user-provided function with the raw bytes
/// of that message.
pub struct MessagePipeWatcher {
    endpoint: Arc<MessageEndpoint>,
    // Watches the handle for incoming messages
    trap: Trap,
}

impl MessagePipeWatcher {
    /// Create a new MessagePipeWatcher which schedules `message_handler` on
    /// the default sequence whenever the endpoint receives a new message.
    pub fn new(
        endpoint: MessageEndpoint,
        message_handler: impl FnMut((Vec<u8>, Vec<UntypedHandle>)) + Send + 'static,
    ) -> Result<Self, MojoResult> {
        Self::new_with_runner(
            endpoint,
            message_handler,
            SequencedTaskRunnerHandle::get_current_default()
                .expect("Must be called in a context with a default SequencedTaskRunner"),
        )
    }

    /// Create a new MessagePipeWatcher which schedules `message_handler` on
    /// `runner` (as opposed to the default sequence) whenever `endpoint`
    /// receives a new message.
    ///
    /// When invoked, `message_handler` is passed the output of
    /// `MessageEndpoint::read`. See that method's documentation for
    /// information about `method_handler`'s input type.
    pub fn new_with_runner(
        endpoint: MessageEndpoint,
        message_handler: impl FnMut((Vec<u8>, Vec<UntypedHandle>)) + Send + 'static,
        runner: SequencedTaskRunnerHandle,
    ) -> Result<Self, MojoResult> {
        // The main goal of this function is to construct a closure which reads
        // from the `endpoint` and schedules `message_handler` on `runner`.

        // Wrap the endpoint in an Arc so we can share it with the closure
        let endpoint = Arc::new(endpoint);

        // Lifetime considerations:
        // The following variables are `move`d into the closure:
        // - `endpoint_weak` is weak because the ownership logically belongs only to the
        //   watcher (we don't want to read from it after the watcher dies).
        // - `message_handler` is strong because it's used by tasks we post to the
        //   runner, so it needs to be alive until those tasks finish or are dropped.
        // - `runner` is moved as well, so that we can guarantee it survives until the
        //   trap is dropped.
        let endpoint_weak = Arc::downgrade(&endpoint);
        let message_handler = Arc::new(Mutex::new(message_handler));

        let trigger_handler = move |trap_event: &TrapEvent| {
            Self::read_from_pipe_and_schedule_handler(
                trap_event,
                &endpoint_weak,
                &message_handler,
                &runner,
            )
        };

        // Define when we trigger the trap (whenever we get a new message)
        let trigger_signals: HandleSignals = HandleSignals::NEW_DATA_READABLE;
        let trigger_condition: TriggerCondition = TriggerCondition::SignalsSatisfied;

        let trap = Trap::new()?;
        // We only ever have one trigger active, so no need to keep track of its id
        let _trigger_id =
            trap.add_trigger(&*endpoint, trigger_signals, trigger_condition, trigger_handler)?;

        Ok(Self { endpoint, trap })
    }

    /// Consume the MessagePipeWatcher, returning the endpoint. The watching
    /// function will no longer be called for messages that arrive after this
    /// function returns.
    ///
    /// If this watcher held the last reference to its task runner, then any
    /// messages which previously arrived but haven't yet been processed by the
    /// runner might be dropped silently.
    pub fn into_endpoint(self) -> MessageEndpoint {
        // Drop the trap first so that any existing triggers are cleared, which
        // ends the lifetime of any references they contain.
        // FOR_RELEASE: Make sure this clears triggers properly
        drop(self.trap);
        // We only hand out weak references (into the triggers), and the
        // triggers were cleared when we dropped the trap, so none of them are
        // running. Thus there should be exactly one strong reference.
        return Arc::into_inner(self.endpoint).unwrap();
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
        endpoint_weak: &Weak<MessageEndpoint>,
        message_handler: &Arc<Mutex<impl FnMut((Vec<u8>, Vec<UntypedHandle>)) + Send + 'static>>,
        runner: &SequencedTaskRunnerHandle,
    ) {
        // An err result indicates that this function will never be called
        // again...which is fine. No need to do anything.
        if trap_event.result().is_err() {
            return;
        }
        // Otherwise, there's a new message for us to read

        // This upgrade should succeed because this function won't outlive the
        // trap, hence it won't outlive the watcher, which owns the endpoint.
        let endpoint = endpoint_weak.upgrade().unwrap();
        // The only way `read` can fail is if there isn't a message to read
        let msg = endpoint.read().expect("Tried to read a message but there wasn't one");
        // Clone the handler function Arc so we can move it into the closure
        let message_handler_clone = message_handler.clone();

        runner.post_task(move || {
            // We're posting to a sequence, so try_lock should always succeed
            let mut message_handler = message_handler_clone
                .try_lock()
                .expect("Sequence-bound mutex was locked or poisoned");
            message_handler(msg)
        });
        // FOR_RELEASE: We need to re-arm the trap here, unless trap
        // handles that for us once it's fully implemented.
    }
}

// FOR_RELEASE: Not sure about this, maybe we should just expose select
// functions like `write`?
impl std::ops::Deref for MessagePipeWatcher {
    type Target = MessageEndpoint;

    fn deref(&self) -> &MessageEndpoint {
        &self.endpoint
    }
}
