// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! TODO: Module docs (link to interface.rs?)

chromium::import! {
  "//mojo/public/rust/system";
  "//mojo/public/rust/sequences";
}

use std::marker::PhantomData;
// FOR_RELEASE: Replace some/all Arc/Mutexes with the sequenced equivalents,
// where appropriate (maybe all of them?).
// TODO(crbug.com/477584253): Replace std::sync with std::nonpoison once
// it's stabilized, if any uses remain.
use std::sync::{Arc, Mutex, Weak};

use sequences::SequencedTaskRunnerHandle;
use system::message::RawMojoMessage;
use system::message_pipe::MessageEndpoint;

use crate::interface::{DynMojomInterface, MojomInterface};
use crate::message::MojomMessage;
use crate::message_pipe_watcher::{MessagePipeWatcher, ResponseSender};

/// This type represents one end of a Mojo pipe corresponding to a
/// particular Mojom `interface`. The parameter `T` names the interface:
/// it will always be instantiated with `dyn SomeInterface`. Each
/// `Receiver` is entangled with exactly one `Remote` or `PendingRemote`
/// elsewhere in the program, corresponding to the same `interface`,
/// which holds the other end of the pipe.
///
/// To obtain a `Receiver`, `bind` a `PendingReceiver` to a sequence and
/// provide a state object which implements the trait named by `T`.
/// While bound, the `Receiver` will process any messages it receives from
/// its `Remote`, by invoking the trait methods of its state object.
/// Incoming messages are handled asynchronously on the `Receiver`'s
/// sequence.
///
/// Note that a `PendingReceiver` can receive messages, but they will not be
/// processed until it is bound. Once bound, the newly-created `Receiver`
/// will immediately schedule processing of all pending messages.
// FOR_RELEASE: Naming question (should we put in a T for
// searchability/consistency?)
pub struct Receiver<StateTy: MojomInterface> {
    endpoint_watcher: MessagePipeWatcher,
    // FOR_RELEASE: Replace these with their sequenced equivalents
    state: Arc<Mutex<StateTy>>,
}

/// This type represents one end of a Mojo pipe corresponding to with a
/// particular Mojom `interface`. The parameter `T` names the interface:
/// it will always be instantiated with `dyn SomeInterface`. Each
/// `PendingReceiver` is entangled with exactly one `Remote` or
/// `PendingRemote` elsewhere in the program, corresponding to the same
/// `interface`, which holds the other end of the pipe.
///
/// This type represents a `Receiver` which has not yet been bound to a
/// sequence, and is therefore unable to send messages.
pub struct PendingReceiver<T>
where
    T: DynMojomInterface + ?Sized,
{
    endpoint: MessageEndpoint,
    _phantom: PhantomData<T>,
}

impl<T> PendingReceiver<T>
where
    T: DynMojomInterface + ?Sized,
{
    /// Create a new PendingReceiver from a raw pipe endpoint
    // This function isn't `pub` because users should always get their
    // `PendingReceiver`s from API functions, or by `unbind`ing a `Receiver`.
    pub(crate) fn new(endpoint: MessageEndpoint) -> Self {
        Self { endpoint, _phantom: PhantomData }
    }

    /// Bind this `PendingReceiver` to the provided state object and the
    /// current default sequence.
    pub fn bind<StateTy>(self, state: StateTy) -> Receiver<StateTy>
    where
        StateTy: MojomInterface<DynTy = T::DynTy> + Send + 'static,
    {
        Receiver::new(self.endpoint, state)
    }

    /// Bind this `PendingReceiver` to the provided state object and the
    /// provided sequence.
    pub fn bind_with_runner<StateTy>(
        self,
        state: StateTy,
        runner: SequencedTaskRunnerHandle,
    ) -> Receiver<StateTy>
    where
        StateTy: MojomInterface<DynTy = T::DynTy> + Send + 'static,
    {
        Receiver::new_with_runner(self.endpoint, state, runner)
    }
}

impl<StateTy> Receiver<StateTy>
where
    StateTy: MojomInterface + Sized + Send + 'static,
{
    /// Create a new Receiver from a raw pipe endpoint, bound to the default
    /// sequence.
    // This function isn't `pub` because users should always get their `Receiver`s
    // by `bind`ing a `PendingReceiver`.
    fn new(endpoint: MessageEndpoint, state: StateTy) -> Self {
        Self::new_with_runner(
            endpoint,
            state,
            SequencedTaskRunnerHandle::get_current_default()
                .expect("Must be called in a context with a default SequencedTaskRunner"),
        )
    }

    /// Create a new Receiver from a raw pipe endpoint, bound to the
    /// provided sequence.
    // This function isn't `pub` because users should always get their `Receiver`s
    // by `bind`ing a `PendingReceiver`.
    fn new_with_runner(
        endpoint: MessageEndpoint,
        state: StateTy,
        runner: SequencedTaskRunnerHandle,
    ) -> Self {
        let state = Arc::new(Mutex::new(state));
        let state_weak = Arc::downgrade(&state);

        let handler = move |raw_message, sender| {
            Self::incoming_message_handler(raw_message, &state_weak, sender)
        };

        let endpoint_watcher = MessagePipeWatcher::new_with_runner(endpoint, runner, handler, None)
            .expect("FOR_RELEASE: Figure out how to handle errors here");

        Self { endpoint_watcher, state }
    }

    // FOR_RELEASE: Provide a mutex-y function so the holder of this `Receiver` can
    // examine the state while it's in use? Might be risky if they can misuse the
    // lock though.

    /// Unbind the remote, returning the contained state object and a
    /// `PendingRemote` which can be re-bound later.
    // FOR_RELEASE: Figure out/document the implications for any already-posted
    // tasks
    // This function is not `pub` because it's a dangerous operation, so we're
    // restricting access until someone has a use-case.
    #[allow(unused)]
    fn unbind(self) -> (PendingReceiver<StateTy::DynTy>, StateTy) {
        // FOR_RELEASE: Figure out when it's safe to unwrap
        let state = Arc::into_inner(self.state).unwrap().into_inner().unwrap();
        let endpoint = self.endpoint_watcher.into_endpoint();
        (PendingReceiver::new(endpoint), state)
    }

    /// This is the function which is called by the endpoint watcher
    /// whenever a message comes in. Its job is to parse the message
    /// header, call the corresponding method on the state object, and then
    /// send a response back through the pipe (if the message expects one).
    fn incoming_message_handler(
        raw_message: RawMojoMessage,
        state_weak: &Weak<Mutex<StateTy>>,
        sender: ResponseSender,
    ) {
        let message: MojomMessage = match MojomMessage::from_raw(&raw_message) {
            Ok(msg) => msg,
            Err(err) => {
                // The error here is a reminder to return immediately, which we do
                let _ = raw_message.report_bad_message(&err.to_string());
                return;
            }
        };

        let expects_response = message
            .header
            .flags
            .contains(crate::message_header::MessageHeaderFlags::EXPECTS_RESPONSE);

        let Some(state) = state_weak.upgrade() else {
            // If we can't get the state, then the receiver must have just been
            // unbound, so there's nothing more for us to do.
            return;
        };

        // Call our internal state object's message handler, and provide a
        // callback that either sends the response or panics because no response
        // was expected.
        // We might be able to make this more readable by moving the state calls
        // out of the if statement using a type like itertools::Either, if we
        // get that approved for use in chromium.
        if expects_response {
            // Make sure the request ID in the response header matches the request.
            let request_id = message.header.request_id;
            state.lock().expect("Mutex should never be poisoned").handle_incoming_message(
                message,
                move |mut response: MojomMessage| {
                    response.header.request_id = request_id;
                    sender.try_send_response(
                        RawMojoMessage::new_with_bytes(&response.into_bytes()).unwrap(),
                    );
                },
            );
        } else {
            state
                .lock()
                .expect("Mutex should never be poisoned")
                .handle_incoming_message(message, |_| {
                    panic!("Tried to send a response to a message that didn't expect one!")
                })
        };
    }

    // We deliberately do not implement `From` and `Into` for
    // `Receiver/PendingReceiver` pairs, because binding and unbinding are
    // stateful operations that should be done explicitly.
}
