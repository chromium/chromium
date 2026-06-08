// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the [`Receiver`] and [`PendingReceiver`] types, which
//! represent the implementation side of a Mojo interface. Each `Receiver` or
//! `PendingReceiver` is associated with exactly one `Remote` or
//! `PendingRemote` elsewhere in the program. A `PendingReceiver` does nothing
//! until bound to a sequence and provided with a _state object_ to create a
//! `Receiver<StateTy>`.
//!
//! A `Receiver<StateTy>` listens for incoming messages from the corresponding
//! [`Remote`](super::remote::Remote); whenever it receives one, it calls the
//! corresponding methods on its state object.
//!
//! The standard way to obtain a `Receiver` is to first create a pipe via
//! [`PendingRemote::new_pipe`](super::remote::PendingRemote::new_pipe), then
//! bind the `PendingReceiver` to a sequence and a state object by calling
//! [`PendingReceiver::bind`]. `PendingReceiver`s can also be obtained by
//! manually wrapping a
//! [`MessageEndpoint`](system::message_pipe::MessageEndpoint).
//!
//! Incoming messages are processed asynchronously on the bound sequence.
//! Messages received while the receiver is still pending are queued and
//! processed immediately upon binding.
//!
//! For a more detailed explanation, see the documentation for the
//! [`interface`] module.

chromium::import! {
  "//mojo/public/rust/system";
  "//base:sequenced_task_runner";
}

use std::marker::PhantomData;
// TODO(crbug.com/470438844): Replace some/all Arc/Mutexes with the
// sequenced equivalents, where appropriate (maybe all of them?).
// TODO(crbug.com/477584253): Replace std::sync with std::nonpoison once
// it's stabilized, if any uses remain.
use std::sync::{Arc, Mutex, Weak};

use sequenced_task_runner::SequencedTaskRunnerHandle;

use crate::interface::{DynMojomInterface, MojomInterface};
use crate::marker_types::{Associated, Primary};
use crate::message::MojomMessage;
use crate::multiplex_router::{EndpointInfo, MultiplexRouterHandle, ResponseSender};
use crate::remote::RouterHandle;

/// This type represents either a regular mojom `Receiver`, or an
/// `AssociatedReceiver`. See the documentation of those types for details.
pub struct GenericReceiver<StateTy: MojomInterface, Marker> {
    // We never actually access either field after creation, we just need to
    // to keep them alive while the receiver is alive.
    // TODO(crbug.com/517519181): Use an enum instead of a trait object.
    _router: Box<dyn AsRef<MultiplexRouterHandle> + Send + Sync + 'static>,
    _state: Arc<Mutex<StateTy>>,
    // fn() -> T acts like T, but is always `Send` and `Sync`
    _phantom: PhantomData<fn() -> Marker>,
}

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
pub type Receiver<T> = GenericReceiver<T, Primary>;

/// This type is equivalent to `Receiver`, but does not own the underlying
/// message pipe across which it sends messages. Instead, it is entangled with
/// an associated remote, and exactly one end of each pair must be sent across
/// an existing message pipe. Once that's done, all messages between the pair
/// will utilize that pipe.
///
/// It is guaranteed that all messages across that pipe will be delivered in
/// strict FIFO order; if one endpoint sends a message and its counterpart isn't
/// yet ready to receive messages, _all_ messages on that pipe will be
/// blocked.
pub type AssociatedReceiver<T> = GenericReceiver<T, Associated>;

/// This type represents a `Receiver` which has not yet been bound to a
/// sequence, and is therefore unable to send messages.
///
/// A `PendingReceiver` holds one end of a Mojo pipe corresponding to with a
/// particular Mojom `interface`. The parameter `T` names the interface:
/// it will always be instantiated with `dyn SomeInterface`. Each
/// `PendingReceiver` is entangled with exactly one `Remote` or
/// `PendingRemote` elsewhere in the program, corresponding to the same
/// `interface`, which holds the other end of the pipe.
pub type PendingReceiver<T> = crate::pending_endpoint::PendingReceiver<T>;

/// This type is used to represent a self-owned receiver, which keeps itself
/// alive until the other endpoint is disconnected.
pub type SelfOwnedReceiver<StateTy> = Arc<Receiver<StateTy>>;
pub type SelfOwnedReceiverWeak<StateTy> = Weak<Receiver<StateTy>>;

impl<T> PendingReceiver<T>
where
    T: DynMojomInterface + ?Sized,
{
    /// Bind this `PendingReceiver` to the provided state object and the
    /// current default sequence.
    pub fn bind<StateTy>(self, state: StateTy) -> Receiver<StateTy>
    where
        StateTy: MojomInterface<DynTy = T> + Send + 'static,
    {
        self.bind_with_options(state, None, None)
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
        let make_handle = |endpoint_info| -> RouterHandle {
            let sets_high_bit = false; // Receivers set the high bit to 0
            Box::new(MultiplexRouterHandle::new(self.endpoint, sets_high_bit, endpoint_info))
        };
        Receiver::new(make_handle, state, runner, disconnect_handler)
    }

    /// Create a new Receiver which owns itself; it will continue to live until
    /// the other end is disconnected.
    pub fn bind_self_owned<StateTy>(self, state: StateTy) -> SelfOwnedReceiverWeak<StateTy>
    where
        StateTy: MojomInterface<DynTy = T> + Sized + Send + 'static,
    {
        Receiver::bind_self_owned_internal(move |disconnect_handler| {
            self.bind_with_options(state, None, disconnect_handler)
        })
    }
}

impl<StateTy, Marker> GenericReceiver<StateTy, Marker>
where
    StateTy: MojomInterface + Sized + Send + 'static,
    Marker: 'static,
{
    fn bind_self_owned_internal(
        bind_func: impl FnOnce(Option<Box<dyn FnOnce() + Send + 'static>>) -> Self,
    ) -> Weak<Self> {
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
            // (which they can get by `upgrade`ing the returned `Weak`).
            drop(receiver_holder_clone);
        };

        let receiver_strong = Arc::new(bind_func(Some(Box::new(disconnect_handler))));
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
    /// Create a new Receiver from a raw pipe endpoint, bound to the
    /// provided sequence.
    // This function isn't `pub` because users should always get their `Receiver`s
    // by `bind`ing a `PendingReceiver`.
    fn new(
        make_handle: impl FnOnce(EndpointInfo) -> RouterHandle,
        state: StateTy,
        runner: SequencedTaskRunnerHandle,
        disconnect_handler: Option<Box<dyn FnOnce() + Send + 'static>>,
    ) -> Self {
        let state = Arc::new(Mutex::new(state));
        let state_weak = Arc::downgrade(&state);

        let message_handler = move |raw_message, sender| {
            Self::incoming_message_handler(raw_message, &state_weak, sender)
        };

        let router = make_handle(EndpointInfo {
            incoming_message_handler: Arc::new(message_handler),
            disconnect_handler,
            runner,
        });

        Self { _router: router, _state: state, _phantom: PhantomData }
    }

    /// This is the function which is called by the endpoint watcher
    /// whenever a message comes in. Its job is to parse the message
    /// header, call the corresponding method on the state object, and then
    /// send a response back through the pipe (if the message expects one).
    fn incoming_message_handler(
        message: MojomMessage,
        state_weak: &Weak<Mutex<StateTy>>,
        sender: ResponseSender,
    ) {
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
                sender.clone(),
                move |mut response: MojomMessage| {
                    response.header.request_id = request_id;
                    sender.send_message(response);
                },
            );
        } else {
            state.lock().expect("Mutex should never be poisoned").handle_incoming_message(
                message,
                sender,
                |_| panic!("Tried to send a response to a message that didn't expect one!"),
            )
        };
    }

    // We deliberately do not implement `From` and `Into` for
    // `Receiver/PendingReceiver` pairs, because binding and unbinding are
    // stateful operations that should be done explicitly.
}
