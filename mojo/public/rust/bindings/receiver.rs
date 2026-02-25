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

/// This type is used to represent a self-owned receiver, which keeps itself
/// alive until the other endpoint is disconnected.
pub type SelfOwnedReceiver<StateTy> = Arc<Receiver<StateTy>>;
pub type SelfOwnedReceiverWeak<StateTy> = Weak<Receiver<StateTy>>;

impl<T> PendingReceiver<T>
where
    T: DynMojomInterface + ?Sized,
{
    /// Create a new PendingReceiver from a raw pipe endpoint.
    ///
    /// If you want to create a new remote/receiver pair, use
    /// `PendingRemote::new_pipe` instead. This function is mostly useful
    /// for creating a new `Receiver` from an endpoint received via mojo or FFI.
    ///
    /// Note that the caller is responsible for ensuring that `Self` has the
    /// same instantiation of `T` as the other endpoint, or else incoming
    /// messages will be incomprehensible.
    pub fn new(endpoint: MessageEndpoint) -> Self {
        Self { endpoint, _phantom: PhantomData }
    }

    /// Consume this PendingReceiver and return the underlying endpoint.
    pub fn into_endpoint(self) -> MessageEndpoint {
        self.endpoint
    }

    /// Bind this `PendingReceiver` to the provided state object and the
    /// current default sequence.
    pub fn bind<StateTy>(self, state: StateTy) -> Receiver<StateTy>
    where
        StateTy: MojomInterface<DynTy = T> + Send + 'static,
    {
        Receiver::new(self.endpoint, state)
    }

    /// Bind this `PendingReceiver` to the provided state object with the
    /// provided options.
    pub fn bind_with_options<StateTy>(
        self,
        state: StateTy,
        runner: Option<SequencedTaskRunnerHandle>,
        disconnect_handler: Option<Box<dyn FnOnce() + Send + 'static>>,
    ) -> Receiver<StateTy>
    where
        StateTy: MojomInterface<DynTy = T> + Send + 'static,
    {
        let runner = runner.unwrap_or_else(|| {
            SequencedTaskRunnerHandle::get_current_default()
                .expect("Must be called in a context with a default SequencedTaskRunner")
        });
        Receiver::new_with_options(self.endpoint, state, runner, disconnect_handler)
    }

    /// Create a new Receiver which owns itself; it will continue to live until
    /// the other end is disconnected.
    pub fn bind_self_owned<StateTy>(self, state: StateTy) -> SelfOwnedReceiverWeak<StateTy>
    where
        StateTy: MojomInterface<DynTy = T> + Sized + Send + 'static,
    {
        // In order to provide a convenient user interface and also be fully memory
        // safe, the types here get a little convoluted. We begin by constructing
        // an `Arc<Mutex<Option<SelfOwnedReceiver>>`. We then pass a strong ref to
        // that arc into the disconnect handler, so that it will get dropped when
        // the handler runs. We then initialize the receiver with that handler, and
        // swap it into the `Option`.
        //
        // However, to hide the details from the user, we want to only return a
        // reference to the receiver itself. To do that, the `SelfOwnedReceiver`
        // type _also_ has an `Arc`, so we can return a weak reference to just that
        // part and hide the `Mutex` and `Option` from the user.
        //
        // Note that if we ever refit this class to be `unsafe`, then we could
        // eliminate the outer `Arc` and `Mutex`, and possible the `Option` as well.

        let receiver_holder = Arc::new(Mutex::new(None));
        let receiver_holder_clone = Arc::clone(&receiver_holder);

        let disconnect_handler = move || {
            // Drop our self-reference. This will cause the receiver
            // itself to be dropped, unless the user is holding a strong reference
            // (which it can get by `upgrade`ing the returned `Weak`).
            drop(receiver_holder_clone);
        };

        let receiver_strong =
            Arc::new(self.bind_with_options(state, None, Some(Box::new(disconnect_handler))));
        let receiver_weak = Arc::downgrade(&receiver_strong);

        *receiver_holder.lock().expect("Mutex should never be poisoned") = Some(receiver_strong);

        // At this point, the references are:
        // - receiver_holder: The ref we just wrote to is about to be dropped, but
        //   there's another strong ref in the disconnect handler, which is the only
        //   other reference to the holder.
        // - receiver: There's a strong reference inside the holder, and a weak
        //   reference which we return here.

        return receiver_weak;
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
        Self::new_with_options(
            endpoint,
            state,
            SequencedTaskRunnerHandle::get_current_default()
                .expect("Must be called in a context with a default SequencedTaskRunner"),
            None,
        )
    }

    /// Create a new Receiver from a raw pipe endpoint, bound to the
    /// provided sequence.
    // This function isn't `pub` because users should always get their `Receiver`s
    // by `bind`ing a `PendingReceiver`.
    fn new_with_options(
        endpoint: MessageEndpoint,
        state: StateTy,
        runner: SequencedTaskRunnerHandle,
        disconnect_handler: Option<Box<dyn FnOnce() + Send + 'static>>,
    ) -> Self {
        let state = Arc::new(Mutex::new(state));
        let state_weak = Arc::downgrade(&state);

        let handler = move |raw_message, sender| {
            Self::incoming_message_handler(raw_message, &state_weak, sender)
        };

        let endpoint_watcher =
            MessagePipeWatcher::new_with_runner(endpoint, runner, handler, disconnect_handler)
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
                    let (payload, handles) = response.into_data();
                    sender.try_send_response(
                        RawMojoMessage::new_with_data(&payload, handles).unwrap(),
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
