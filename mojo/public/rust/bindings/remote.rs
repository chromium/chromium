// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the [`Remote`] and [`PendingRemote`] types, which
//! represent the caller side of a Mojo interface. Each `Remote` or
//! `PendingRemote` is associated with exactly one `Receiver` or
//! `PendingReceiver` elsewhere in the program. A `PendingRemote` does
//! nothing until bound to a sequence to create a `Remote<dyn SomeInterface>`.
//!
//! A `Remote<dyn SomeInterface>` sends messages to the corresponding
//! `Receiver` and schedules response callbacks on its bound sequence.
//!
//! The standard way to obtain a `Remote` is to first create a pipe via
//! [`PendingRemote::new_pipe`], then bind the `PendingRemote` to a sequence
//! by calling [`PendingRemote::bind`]. `PendingRemote`s can also be obtained
//! by manually wrapping a
//! [`MessageEndpoint`](system::message_pipe::MessageEndpoint).
//!
//! Messages can be sent immediately after binding, even before the
//! corresponding `Receiver` is bound. Response callbacks are processed
//! asynchronously on the bound sequence.
//!
//! For a more detailed explanation, see the documentation for the
//! [`interface`] module.

chromium::import! {
  "//mojo/public/rust/system";
  "//base:sequenced_task_runner";
}

use std::collections::HashMap;
use std::marker::PhantomData;
// TODO(crbug.com/470438844): Replace some/all Arc/Mutexes with the sequenced
// equivalents, where appropriate (maybe all of them?).
// TODO(crbug.com/477584253): Replace std::sync with std::nonpoison once
// it's stabilized, if any uses remain.
use std::sync::{Arc, Mutex};

use sequenced_task_runner::SequencedTaskRunnerHandle;

use crate::interface::{DynMojomInterface, MojomInterface};
use crate::marker_types::{Associated, Primary};
use crate::message::MojomMessage;
use crate::multiplex_router::{EndpointInfo, MultiplexRouterHandle, ResponseSender};
use crate::pending_associated_endpoint_parsing::Registrar;

/// This type looks very scary, but mostly it's just a map from request ID to
/// the function to invoke when we get a response to that request. For examples
/// of what a real `ResponseCallbackTy` looks like, it's easiest to take a look
/// at some generated code from a mojom file with an `interface`.
type CallbackMap<T> = Arc<Mutex<HashMap<u64, <T as MojomInterface>::ResponseCallbackTy>>>;

// TODO(crbug.com/517519181): Use an enum instead of a trait object.
pub(crate) type RouterHandle = Box<dyn AsRef<MultiplexRouterHandle> + Send + Sync + 'static>;

/// This type represents either a regular mojom `Remote`, or an
/// `AssociatedRemote`. See the documentation of those types for details.
pub struct GenericRemote<T, Marker>
where
    T: DynMojomInterface + ?Sized,
{
    router: RouterHandle,
    // Stores the callbacks to be invoked when we get a response, using the
    // request ID as a key. May be accessed out-of-sequence (when sending a
    // new message), so requires synchronization.
    pending_responses: CallbackMap<T>,
    // Starts at 1, increments each time we send a message.
    // 0 is reserved in case it gets a special meaning later.
    next_request_id: u64,
    // fn() -> T acts like T, but is always `Send` and `Sync`
    #[allow(clippy::type_complexity)]
    _phantom: PhantomData<(fn() -> T, fn() -> Marker)>,
}

/// This type represents one end of a Mojo pipe corresponding to a
/// particular Mojom `interface`. The parameter `T` names the interface:
/// it will always be instantiated with `dyn SomeInterface`. Each
/// `Remote` is entangled with exactly one `Receiver` or
/// `PendingReceiver` elsewhere in the program, corresponding to with
/// the same `interface`, which holds the other end of the pipe.
///
/// To obtain a `Remote`, `bind` a `PendingRemote` to a sequence. While
/// bound, the `Remote` provides the ability to send the messages
/// defined in the `interface`'s Mojom definition to the entangled
/// `Receiver`. The `Receiver` will process the messages on its own
/// sequence (possibly in a different process). If the message involves
/// a response, the `Remote` will execute a user-provided callback on
/// its sequence when it receives the response message.
//
// # Implementation details
// Conceptually, the remote has two jobs: sending messages, and orchestrating
// callbacks for responses. It does this by generating an ID for each message
// it sends, and storing the callback in a map. When it gets a response, it
// looks up the ID in that map, and executes the stored callback. The type
// of the callbacks is also defined by the bindings.
//
// The `Remote` type works in close coordination with code generated from the
// Rust Mojom bindings, which provide necessary trait implementations.
//
// For each interface `SomeInterface`, the bindings will implement
// `MojomInterface` for `dyn SomeInterface`, and will also implement
// `SomeInterface` for `Remote<dyn SomeInterface>`, thus providing access to
// the interface's methods.
//
// The bindings will implement the trait such that calling, e.g.
// `remote.Add(...)` will send an `Add` message to the receiver, and stash a
// response callback in the map if necessary.
pub type Remote<T> = GenericRemote<T, Primary>;

/// This type is equivalent to `Remote`, but does not own the underlying message
/// pipe across which it sends messages. Instead, it is entangled with an
/// associated receiver, and exactly one end of each pair must be sent across an
/// existing message pipe. Once that's done, all messages between the pair will
/// utilize that pipe.
///
/// It is guaranteed that all messages across that pipe will be delivered in
/// strict FIFO order; if one endpoint sends a message and its counterpart isn't
/// yet ready to receive messages, _all_ messages on that pipe will be blocked.
pub type AssociatedRemote<T> = GenericRemote<T, Associated>;

/// This type represents one end of a Mojo pipe corresponding to a
/// particular Mojom `interface`. The parameter `T` names the interface:
/// it will always be instantiated with `dyn SomeInterface`. Each
/// `PendingRemote` is entangled with exactly one `Receiver` or
/// `PendingReceiver` elsewhere in the program, corresponding to the
/// same `interface`, which holds the other end of the pipe.
///
/// This type represents a `Remote` which has not yet been bound to a
/// sequence, and is therefore unable to send messages.
pub type PendingRemote<T> = crate::pending_endpoint::PendingRemote<T>;

/// This type is equivalent to a `PendingRemote`, but it does not hold a
/// message pipe endpoint; instead, it will use an existing message pipe to send
/// its messages.
///
/// Each `PendingAssociatedRemote` is entangled with a
/// `(Pending)AssociatedReceiver` elsewhere in the program. Exactly one of the
/// pair should be sent over a pipe via a mojo message; afterwards, they will
/// be associated with that pipe and communicate using it.
pub type PendingAssociatedRemote<T> =
    crate::pending_associated_endpoint::PendingAssociatedRemote<T>;

impl<T> PendingRemote<T>
where
    T: DynMojomInterface + ?Sized,
{
    /// Bind this pending remote to the current default sequence.
    pub fn bind(self) -> Remote<T> {
        Self::bind_with_options(self, None, None)
    }

    /// Bind this pending remote with the provided options.
    pub fn bind_with_options(
        self,
        runner: Option<SequencedTaskRunnerHandle>,
        disconnect_handler: Option<Box<dyn FnOnce() + Send + 'static>>,
    ) -> Remote<T> {
        let runner = runner.unwrap_or_else(|| {
            SequencedTaskRunnerHandle::get_current_default()
                .expect("Must be called in a context with a default SequencedTaskRunner")
        });
        let make_handle = |endpoint_info| -> RouterHandle {
            let sets_high_bit = true; // Remotes set the high bit to 1
            Box::new(MultiplexRouterHandle::new(self.endpoint, sets_high_bit, endpoint_info))
        };
        Remote::new(make_handle, runner, disconnect_handler)
    }
}

impl<T> PendingAssociatedRemote<T>
where
    T: DynMojomInterface + ?Sized,
{
    /// Bind this pending remote to the current default sequence.
    pub fn bind(self) -> AssociatedRemote<T> {
        Self::bind_with_options(self, None, None)
    }

    /// Bind this pending remote with the provided options.
    pub fn bind_with_options(
        self,
        runner: Option<SequencedTaskRunnerHandle>,
        disconnect_handler: Option<Box<dyn FnOnce() + Send + 'static>>,
    ) -> AssociatedRemote<T> {
        let runner = runner.unwrap_or_else(|| {
            SequencedTaskRunnerHandle::get_current_default()
                .expect("Must be called in a context with a default SequencedTaskRunner")
        });
        let make_handle = |endpoint_info| self.register_bound(endpoint_info);
        AssociatedRemote::new(make_handle, runner, disconnect_handler)
    }
}

impl<T, Marker> GenericRemote<T, Marker>
where
    T: DynMojomInterface + ?Sized,
{
    /// Create a new Remote from a raw pipe endpoint, bound to the given
    /// sequence.
    /// This function isn't `pub` because users should always get their
    /// `Remote`s by `bind`ing a `PendingRemote`.
    fn new(
        make_handle: impl FnOnce(EndpointInfo) -> RouterHandle,
        runner: SequencedTaskRunnerHandle,
        disconnect_handler: Option<Box<dyn FnOnce() + Send + 'static>>,
    ) -> Self {
        let pending_responses = Arc::new(Mutex::new(HashMap::new()));
        let pending_responses_clone = pending_responses.clone();
        let message_handler = move |message, sender| {
            Self::incoming_message_handler(message, sender, &pending_responses_clone)
        };
        let router = make_handle(crate::multiplex_router::EndpointInfo {
            incoming_message_handler: Arc::new(message_handler),
            disconnect_handler,
            runner,
        });

        Self {
            pending_responses,
            router,
            next_request_id: 1, // Reserve 0 in case it gets a special meaning later
            _phantom: PhantomData,
        }
    }

    // Construct a header for the provided message ID and payload, and send it
    // through the pipe. When we get a response, call the provided response
    // handler.
    //
    // This function is public because we need to call it from
    // generated code, but doc(hidden) because users shouldn't call it
    // directly. Instead, they should call one of the interface-specific trait
    // methods (e.g. `remote.Add(...)`, which will call this under-the-hood).
    #[doc(hidden)]
    pub fn send_message_internal(
        &mut self,
        mut message: MojomMessage,
        response_callback: Option<T::ResponseCallbackTy>,
    ) {
        // Set the request ID and stash the callback in the map with that ID as the key
        message.header.request_id = self.next_request_id;
        if let Some(callback) = response_callback {
            let old_entry = self
                .pending_responses
                .lock()
                .expect("Mutex should never be poisoned")
                .insert(self.next_request_id, callback);
            if old_entry.is_some() {
                // This is technically possible...if we wrap all the way around with request IDs
                panic!("send_message_internal: Tried to insert duplicate response!")
            }
        }

        // Generate the next request ID. Skip 0 in case it gets a special meaning in the
        // future.
        self.next_request_id =
            if self.next_request_id == u64::MAX { 1 } else { self.next_request_id + 1 };

        // This can only fail if the other end is closed, in which case we've nothing to
        // do here (we'll get a disconnection notification separately).
        self.router.as_ref().as_ref().send_message(message);
    }

    /// This is the function which is called by the `MultiplexRouter`
    /// whenever a message comes in. Its job is to parse the message
    /// header, retrieve the corresponding response callback from
    /// the map, and invoke the interface's response handler with
    /// it.
    fn incoming_message_handler(
        mut message: MojomMessage,
        sender: ResponseSender,
        callback_map: &CallbackMap<T>,
    ) {
        let response_callback = callback_map
            .lock()
            .expect("Callback map should never be poisoned")
            .remove(&message.header.request_id);
        let response_callback = match response_callback {
            Some(callback) => callback,
            None => {
                // The error here is a reminder to return immediately, which we do
                let _ = message.report_bad_message(&format!(
                    "Received message with unknown request_id {}",
                    message.header.request_id
                ));
                return;
            }
        };
        T::handle_incoming_response(message, sender, response_callback);
    }

    pub fn as_registrar(&self) -> &impl Registrar {
        self.router.as_ref().as_ref()
    }
}

// We deliberately do not implement `From` and `Into` for
// `Remote/PendingRemote` pairs, because binding and unbinding are
// stateful operations that should be done explicitly.
