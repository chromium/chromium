// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! TODO: Module docs (link to interface.rs?)

chromium::import! {
  "//mojo/public/rust/system";
  "//mojo/public/rust/sequences";
}

use std::collections::HashMap;
use std::marker::PhantomData;
// FOR_RELEASE: Replace some/all Arc/Mutexes with the sequenced equivalents,
// where appropriate (maybe all of them?).
// TODO(crbug.com/477584253): Replace std::sync with std::nonpoison once
// it's stabilized, if any uses remain.
use std::sync::{Arc, Mutex};

use sequences::SequencedTaskRunnerHandle;
use system::message::RawMojoMessage;
use system::message_pipe::MessageEndpoint;

use crate::interface::{DynMojomInterface, MojomInterface};
use crate::message::MojomMessage;
use crate::message_pipe_watcher::MessagePipeWatcher;

type CallbackMap<T> = Arc<Mutex<HashMap<u64, <T as MojomInterface>::ResponseCallbackTy>>>;

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
pub struct Remote<T>
where
    T: DynMojomInterface + ?Sized,
{
    endpoint_watcher: MessagePipeWatcher,
    // Stores the callbacks to be invoked when we get a response, using the
    // request ID as a key. May be accessed out-of-sequence (when sending a
    // new message), so requires synchronization.
    pending_responses: CallbackMap<T>,
    // Starts at 1, increments each time we send a message.
    // 0 is reserved in case it gets a special meaning later.
    next_request_id: u64,
    _phantom: PhantomData<T>,
}

/// This type represents one end of a Mojo pipe corresponding to a
/// particular Mojom `interface`. The parameter `T` names the interface:
/// it will always be instantiated with `dyn SomeInterface`. Each
/// `PendingRemote` is entangled with exactly one `Receiver` or
/// `PendingReceiver` elsewhere in the program, corresponding to the
/// same `interface`, which holds the other end of the pipe.
///
/// This type represents a `Remote` which has not yet been bound to a
/// sequence, and is therefore unable to send messages.
pub struct PendingRemote<T>
where
    T: DynMojomInterface + ?Sized,
{
    endpoint: MessageEndpoint,
    _phantom: PhantomData<T>,
}

impl<T> PendingRemote<T>
where
    T: DynMojomInterface + ?Sized,
{
    /// Create a new PendingRemote from a raw pipe endpoint.
    ///
    /// If you want to create a new remote/receiver pair, use
    /// `new_pipe` instead. This function is mostly useful for creating a new
    /// `Remote` from an endpoint received via mojo or FFI.
    ///
    /// Note that the caller is responsible for ensuring that `Self` has the
    /// right instantiation of `T` as the other endpoint, or else incoming
    /// messages will be incomprehensible.
    pub fn new(endpoint: MessageEndpoint) -> Self {
        Self { endpoint, _phantom: PhantomData }
    }

    /// Consume this PendingRemote and return the underlying endpoint.
    pub fn into_endpoint(self) -> MessageEndpoint {
        self.endpoint
    }

    /// Bind this pending remote to the current default sequence.
    pub fn bind(self) -> Remote<T> {
        Remote::new(self.endpoint)
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
        Remote::new_with_options(self.endpoint, runner, disconnect_handler)
    }

    /// Create a new Mojo message pipe corresponding to `T`'s interface, and
    /// return the endpoints
    ///
    /// This can only fail if the system has run out of resources to create new
    /// pipes.
    pub fn new_pipe() -> Option<(PendingRemote<T>, super::receiver::PendingReceiver<T>)> {
        let (endpoint1, endpoint2) = MessageEndpoint::create_pipe().ok()?;
        return Some((
            PendingRemote::new(endpoint1),
            super::receiver::PendingReceiver::new(endpoint2),
        ));
    }
}

impl<T> Remote<T>
where
    T: DynMojomInterface + ?Sized,
{
    /// Create a new Remote from a raw pipe endpoint, bound to the default
    /// sequence.
    // This function isn't `pub` because users should always get their `Remote`s
    // by `bind`ing a `PendingRemote`.
    fn new(endpoint: MessageEndpoint) -> Self {
        Self::new_with_options(
            endpoint,
            SequencedTaskRunnerHandle::get_current_default()
                .expect("Must be called in a context with a default SequencedTaskRunner"),
            None,
        )
    }

    /// Create a new Remote from a raw pipe endpoint, bound to the given
    /// sequence.
    /// This function isn't `pub` because users should always get their
    /// `Remote`s by `bind`ing a `PendingRemote`.
    fn new_with_options(
        endpoint: MessageEndpoint,
        runner: SequencedTaskRunnerHandle,
        disconnect_handler: Option<Box<dyn FnOnce() + Send + 'static>>,
    ) -> Self {
        let pending_responses = Arc::new(Mutex::new(HashMap::new()));
        let pending_responses_clone = pending_responses.clone();
        let message_handler = move |raw_message, _sender| {
            Self::incoming_message_handler(raw_message, &pending_responses_clone)
        };
        let endpoint_watcher = MessagePipeWatcher::new_with_runner(
            endpoint,
            runner,
            message_handler,
            disconnect_handler,
        )
        .expect("FOR_RELEASE: Figure out how to handle errors here");
        // FOR_RELEASE: We should clear out any existing messages in the endpoint
        // in case it's being re-used, so the new remote doesn't see responses to
        // the previous remote's messages.

        Self {
            pending_responses,
            endpoint_watcher,
            next_request_id: 1, // Reserve 0 in case it gets a special meaning later
            _phantom: PhantomData,
        }
    }

    /// Unbind this `Remote` from the current sequence, turning it back into
    /// a `PendingRemote` which can be rebound later.
    ///
    /// If the remote has responses pending, they will be silently ignored.
    pub fn unbind(self) -> PendingRemote<T> {
        PendingRemote::new(self.endpoint_watcher.into_endpoint())
    }

    // Construct a header for the provided message ID and payload, and send it
    // through the pipe. When we get a response, call the provided response
    // handler.
    //
    // This function is public because we need to call it from
    // generated code, but doc(hidden) because users shouldn't call it
    // directly. Instead, they should call one of the interface-specific traits
    // methods(e.g. `remote.Add(...)`, which will call this under-the-hood).
    // FOR_RELEASE: Can this just be &self? (depends on the final interface for
    // message pipes)
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
        let (payload, handles) = message.into_data();
        let _ = self
            .endpoint_watcher
            .send_message(RawMojoMessage::new_with_data(&payload, handles).unwrap());
    }

    /// This is the function which is called by the endpoint watcher
    /// whenever a message comes in. Its job is to parse the message
    /// header, retrieve the corresponding response callback from
    /// the map, and invoke the interface's response handler with
    /// it.
    fn incoming_message_handler(raw_message: RawMojoMessage, callback_map: &CallbackMap<T>) {
        let message: MojomMessage = match MojomMessage::from_raw(&raw_message) {
            Ok(msg) => msg,
            Err(err) => {
                // The error here is a reminder to return immediately, which we do
                let _ = raw_message.report_bad_message(&err.to_string());
                return;
            }
        };
        let response_callback = callback_map
            .lock()
            .expect("Callback map should never be poisoned")
            .remove(&message.header.request_id);
        let response_callback = match response_callback {
            Some(callback) => callback,
            None => {
                // The error here is a reminder to return immediately, which we do
                let _ = raw_message.report_bad_message(&format!(
                    "Received message with unknown request_id {}",
                    message.header.request_id
                ));
                return;
            }
        };
        T::handle_incoming_response(message, response_callback);
    }
}

// We deliberately do not implement `From` and `Into` for
// `Remote/PendingRemote` pairs, because binding and unbinding are
// stateful operations that should be done explicitly.
