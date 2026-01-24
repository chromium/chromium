// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the representation of Mojom `interface`s in Rust. From
//! the user's perspective, this file is mostly implementation details: the
//! useful parts are the `Remote` and `Receiver` types in the corresponding
//! modules.
//!
//! FOR_RELEASE: Split this into multiple files, but for now it's convenient
//! to have it all together.
//! FOR_RELEASE: Make sure all the stuff here matches the final versions
//!
//! FOR_RELEASE: Per several discussions in the rustmojo chat, we may want to
//! think more about the designs for remotes, receivers, ownership and the way
//! they relate to each other. Commonly objects want to open a receiver that
//! lasts as long as it does, but also allow the object to operate on itself.
//! This is naturally expressed using circular or self-referential structures,
//! which are difficult to do in rust, so we need to consider use-cases
//! carefully.
//!
//! # High-level design
//! This module is designed to work in tandem with the Rust Mojom bindings
//! generator. Imagine we have a Mojom file with the following interface:
//!
//! ```
//! interface MathService {
//!   Add(int32 left, int32 right) => (int32 sum);
//!   Div(int32 dividend, int32 divisor) => (int32 quotient, int32 remainder);
//! }
//! ```
//!
//! This will be represented in Rust as a trait:
//! ```
//! trait MathService {
//!   fn Add(&mut self, left: i32, right: i32, response_callback: impl FnOnce(i32) + Send);
//!   fn Div(&mut self, dividend: i32, divisor: i32, response_callback: impl FnOnce(i32, i32) + Send);
//! }
//! ```
//!
//! This trait is then used to instantiate a `Remote` and `Receiver` pair,
//! which send messages of the typesd defined in the `MathService` interface:
//!
//! ```
//! let (p_rem: PendingRemote<dyn MathService>, p_rec: PendingReceiver<dyn MathService>) = ...;
//! ```
//!
//! Side note: the type parameters here are used to ensure you only send
//! messages between endpoints for the same interface. The `dyn` keyword exists
//! to satisfy the compiler, which does not permit instantiating type parameters
//! with traits, only with types).
//!
//! ## Sending Messages
//! Sending a Mojo message is a synchronous operation, but responses are
//! processed asynchronously. Therefore, before you can send messages, you must
//! `bind` the `PendingRemote` to a sequence, which will schedule response
//! processing.
//!
//! ```
//! // Binds to the current default sequence
//! // `bind_with_runner` can be used to specify a different sequence
//! let math_remote: Remote<dyn MathService> = p_rem.bind();
//! ```
//!
//! Once the remote is bound, you can start sending messages immediately (even
//! before) the receiver is bound. To send a message, invoke the corresponding
//! function on the remote object, and pass a callback specifying how to handle
//! the response.
//!
//! ```
//! // This sends a message to the receiver asking it to execute its `Add`
//! // implementation (possibly on a different sequence or in a different process).
//! // It will send the result back to us, and then we will print it on _this_ sequence.
//! math_remote.Add(1, 2, |sum| println!("1 + 2 = {sum}"));
//! ```
//!
//! ## Receiving Messages
//! Like remotes, receivers must be bound to a particular sequence on which they
//! will process incoming messages. However, unlike remotes, which merely send a
//! request to the other endpoint, receivers need to actually process the
//! request.
//!
//! Therefore, binding a receiver also requires you to pass a _state object_
//! which implements the `MathService` trait. When the receiver receives an
//! `Add` request, it invokes the `Add` method on that object.
//!
//! After implementing the trait for a state object, you must also call the
//! `add_mojom_state_object_impls` macro, which sets up some behind-the-scenes
//! information needed by the compiler.
//!
//! ```
//! // A MathService which counts the number of times you've called `Add`.
//! struct CountingMathService {
//!   num_times_added: usize;
//! };
//!
//! impl MathService for MathServiceImpl {
//!   fn Add(&mut self, left: i32, right: i32, response_callback: FnOnce(i32)) {
//!     self.num_times_added += 1;
//!     let ret = left + right;
//!     response_callback(ret);
//!   }
//!
//!   fn Div(&mut self, dividend: i32, divisor: i32, response_callback: FnOnce(i32, i32)) {
//!     ...
//!   }
//! }
//!
//! // Don't forget this!
//! add_mojom_state_object_impls!(CountingMathService, MathService);
//! ```
//!
//! If your service has different behaviors in different contexts, you can
//! define multiple state types and choose which one to use for each receiver:
//!
//! ```
//! // A MathService which uses saturating addition.
//! struct SaturatingMathService {};
//! impl MathService for SaturatingMathService { ... }
//! add_mojom_state_object_impls!(SaturatingMathService, MathService);
//! ```
//!
//! Because receivers can hold different kinds of state objects, the `Receiver`
//! type is parameterized on the type of state object it holds, rather than
//! the trait itself:
//!
//! ```
//! let count_state = CountingMathService { num_times_added: 0 };
//! // Bind a receiver to the current sequence which counts the number of times
//! // `Add` is called.
//! let counting_receiver: Receiver<CountingMathService> = p_rec.bind(count_state);
//! ... // Process some messages
//! // Undbind the receiver, getting the state object out
//! let (p_rec, count_state) = counting_receiver.unbind();
//! // Rebind the receiver to a different state object: now it has different behavior!
//! let saturating_receiver: Receiver<SaturatingMathService> p_rec.bind(SaturatingMathService {});
//!
//! Note that receivers can receive messages while they are pending, but those
//! messages simply sit in the pipe until the receiver is bound and begins scheduling
//! tasks to process them.

// # Implementation Details
//
// Under the hood, we use several tricks in order to provide the above interface,
// and to prevent misuse.
//
// The first problem is that, since each interface has its own trait, we need
// to define a supertrait, `MojomInterface`, to abstract over them. However,
// we cannot automatically implement `MojomInterface`, due to Rust's orphaning
// rules and lack of support for multiple blanket implementations of a trait.
//
// Therefore, we define the `add_mojom_state_object_impls` macro, which implements
// `MojomInterface` appropriately, and require users to call it so the code is in
// their crate.
//
// The second problem is the fact that, while we want our users to only ever use
// `dyn SomeInterface` as the parameter to our objects, there's no way to specify
// that. This could lead to confusing or even incorrect code, if a user accidentally
// bound a `Receiver` to a different interface than its `Remote`.
//
// To work around this, we require each `MojomInterface` implementation to
// declare the trait it corresponds to by using an associated type `DynTy`.
// We restrict the possible values using a marker trait `DynMojomInterface`,
// which is implemented for each generated interface type automatically.
// We then require in our remote/receiver types that their type argument is the
// same as the `DynTy` declared in that type's `MojomInterface` declaration.
//
// As long as the `DynMojomInterface` is correctly implemented (i.e. users use our
// macro like they're supposed to), this ensures that the only valid parameters
// to remote/receiver types are `dyn SomeInterface`.

// FOR_RELEASE: Remove this once the file is fully implemented
#![allow(unused)]

chromium::import! {
  "//mojo/public/rust:mojo_rust_system_api";
  "//mojo/public/rust/sequences:sequences";
}

use std::collections::HashMap;
use std::marker::PhantomData;
// FOR_RELEASE: Replace some/all Arc/Mutexes with the sequenced equivalents,
// where appropriate (maybe all of them?).
// TODO(crbug.com/477584253): Replace std::sync with std::nonpoison once
// it's stabilized, if any uses remain.
use std::sync::{Arc, Mutex, Weak};

use mojo_rust_system_api::message_pipe::{MessageEndpoint, RawMojoMessage};
use mojo_rust_system_api::mojo_types::{MojoResult, UntypedHandle};
use sequences::SequencedTaskRunnerHandle;

use crate::message::MojomMessage;
use crate::message_pipe_watcher::{MessagePipeWatcher, ResponseSender};

/// This trait abstracts over the parts of individual Mojom `interface`s, such
/// as `MathService`. This trait is what's used by generic `Remote`s and
/// `Receiver`s to handle incoming and outgoing messages.
///
/// You shouldn't implement this trait yourself; instead, implement the trait
/// for the specific interface you want to use, and then invoke
/// the `add_mojom_state_object_impls` macro.
pub trait MojomInterface {
    // We use this type to enforce that Remotes and Receivers are parameterized
    // on the right instance of the trait. Each mojom interface `I` should
    // set `DynTy = dyn I` when implementing `MojomInterface`.
    type DynTy: DynMojomInterface + ?Sized;

    /// This defines the type of response callbacks for this interface.
    /// It's used by the `Remote` to store callbacks for different message
    /// types, since they take different arguments.
    type ResponseCallbackTy: Send + 'static;

    /// For use in `Remote`s. Takes a message with a parsed header, examines
    /// it to determine which message type it contains, ensures the callback
    /// has the right type, then invokes it on the parsed message body.
    fn handle_incoming_response(message: MojomMessage, response_callback: Self::ResponseCallbackTy);

    /// For use in `Receiver`s. Takes a message with a parsed header, examines
    /// it to determine which message type it contains, then invokes the
    /// corresponding user-defined handler on the parsed message body
    fn handle_incoming_message(
        &mut self,
        message: MojomMessage,
        send_response: impl FnOnce(MojomMessage),
    );
}

#[doc(hidden)]
pub mod internal {
    use super::*;

    // This trait ensures users have called the `add_mojom_state_object_impls`
    // macro, by being a supertrait of all the bindings-generated interface
    // traits.
    #[diagnostic::on_unimplemented(
        message = "You must invoke the add_mojom_state_object_impls! macro after implementing an interface",
        label = "this type needs to be declared as a state object",
        note = "Hint: try calling the macro with the name of the interface trait:",
        note = "`add_mojom_state_object_impls!({Self}, ...)`"
    )]
    pub trait ImplementThisViaMacro {}

    // Users will implement this for their state objects via the macro. We also
    // need it for `Remote`s because they implement the bindings-generated
    // interface traits. A blanket implementation is fine because all the
    // `Remote` code is auto-generated, so there's nothing for a user to get
    // wrong.
    impl<T> ImplementThisViaMacro for remote::Remote<T> where T: DynMojomInterface + ?Sized {}
}

// FOR_RELEASE: Can/should this be a proc macro instead?
macro_rules! add_mojom_state_object_impls {
    ($ty:ty, $trait:ident) => {
        impl $crate::interface::MojomInterface for $ty {
            type DynTy = dyn $trait;

            // State objects are for use in Receivers, which don't store callbacks
            type ResponseCallbackTy = ();
            fn handle_incoming_response(
                _message: MojomMessage,
                _response_callback: Self::ResponseCallbackTy,
            ) {
                panic!("Receivers never get responses")
            }

            fn handle_incoming_message(
                message: MojomMessage,
                send_response: impl FnOnce(MojomMessage),
            ) {
                // This method is present, with a default implementation, on all the
                // traits generated by our bindings code.
                <Self as $trait>::handle_incoming_message(message, send_response)
            }
        }

        impl $crate::interface::internal::ImplementThisViaMacro for $ty {}
    };
}

/// This trait is used behind-the-scenes to indicate that a type is `dyn I` for
/// some mojom-generated interface type `I`. You should never implement it
/// yourself; generated code will implement it where required.
pub trait DynMojomInterface: MojomInterface {}

// FOR_RELEASE: Put in a different file
pub mod remote {
    type CallbackMap<T> = Arc<Mutex<HashMap<u64, <T as MojomInterface>::ResponseCallbackTy>>>;

    use super::*;
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
        runner: SequencedTaskRunnerHandle,
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
        /// Create a new PendingRemote from a raw pipe endpoint
        // This function isn't `pub` because users should always get their
        // `PendingRemote`s from API functions like `new_pipe`, or from
        // `unbind`ing a `Remote`.
        pub(crate) fn new(endpoint: MessageEndpoint) -> Self {
            Self { endpoint, _phantom: PhantomData }
        }

        /// Bind this pending remote to the current default sequence.
        pub fn bind(self) -> Remote<T> {
            Remote::new(self.endpoint)
        }

        /// Bind this pending remote to the provided sequence.
        pub fn bind_with_runner(self, runner: SequencedTaskRunnerHandle) -> Remote<T> {
            Remote::new_with_runner(self.endpoint, runner)
        }

        /// Create a new Mojo message pipe corresponding to `T`'s interface, and
        /// return the endpoints
        pub fn new_pipe(
        ) -> Result<(PendingRemote<T>, super::receiver::PendingReceiver<T>), MojoResult> {
            let (endpoint1, endpoint2) = MessageEndpoint::create_pipe()?;
            return Ok((
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
            Self::new_with_runner(
                endpoint,
                SequencedTaskRunnerHandle::get_current_default()
                    .expect("Must be called in a context with a default SequencedTaskRunner"),
            )
        }

        /// Create a new Remote from a raw pipe endpoint, bound to the given
        /// sequence.
        /// This function isn't `pub` because users should always get their
        /// `Remote`s by `bind`ing a `PendingRemote`.
        fn new_with_runner(endpoint: MessageEndpoint, runner: SequencedTaskRunnerHandle) -> Self {
            let pending_responses = Arc::new(Mutex::new(HashMap::new()));
            let pending_responses_clone = pending_responses.clone();
            let message_handler = move |raw_message, _sender| {
                Self::incoming_message_handler(raw_message, &pending_responses_clone)
            };
            let endpoint_watcher =
                MessagePipeWatcher::new_with_runner(endpoint, message_handler, runner.clone())
                    .expect("FOR_RELEASE: Figure out how to handle errors here");
            // FOR_RELEASE: We should clear out any existing messages in the endpoint
            // in case it's being re-used, so the new remote doesn't see responses to
            // the previous remote's messages.

            Self {
                pending_responses,
                runner,
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
        // directly. Instead, they should users should call one of the
        // interface-specific traits methods(e.g. `remote.Add(...)`, which will
        // call this under-the-hood).
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
                if !matches!(old_entry, None) {
                    // This is technically possible...if we wrap all the way around with request IDs
                    panic!("send_message_internal: Tried to insert duplicate response!")
                }
            }

            // Generate the next request ID. Skip 0 in case it gets a special meaning in the
            // future.
            self.next_request_id =
                if self.next_request_id == u64::MAX { 1 } else { self.next_request_id + 1 };

            // FOR_RELEASE: This returns a MojoResult, figure out what to do with it
            self.endpoint_watcher
                .send_message(RawMojoMessage::new_with_bytes(&message.into_bytes()).unwrap());
        }

        /// This is the function which is called by the endpoint watcher
        /// whenever a message comes in. Its job is to parse the message
        /// header, retrieve the corresponding response callback from
        /// the map, and invoke the interface's response handler with
        /// it.
        fn incoming_message_handler(
            raw_message: (Vec<u8>, Vec<UntypedHandle>),
            callback_map: &CallbackMap<T>,
        ) {
            // FOR_RELEASE: This indicates a malformed mojo message, we should figure out
            // what to do about those
            let message: MojomMessage = MojomMessage::from_bytes(raw_message.0)
                .expect("Incoming response failed to parse!");
            let response_callback = callback_map
                .lock()
                .expect("Callback map should never be poisoned")
                .remove(&message.header.request_id);
            // FOR_RELEASE: This indicates a malformed mojo message, we should figure out
            // what to do about those
            let response_callback =
                response_callback.expect("Incoming response had no request_id!");
            T::handle_incoming_response(message, response_callback);
        }
    }

    // We deliberately do not implement `From` and `Into` for
    // `Remote/PendingRemote` pairs, because binding and unbinding are
    // stateful operations that should be done explicitly.
} // End mod remote

// FOR_RELEASE: Put in a different file
pub mod receiver {
    use mojo_rust_system_api_61c68895::message_pipe::RawMojoMessage;

    use super::*;

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
        runner: SequencedTaskRunnerHandle,
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
                Self::incoming_message_handler(raw_message, &state_weak, &sender)
            };

            let endpoint_watcher =
                MessagePipeWatcher::new_with_runner(endpoint, handler, runner.clone())
                    .expect("FOR_RELEASE: Figure out how to handle errors here");

            Self { endpoint_watcher, runner, state }
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
            raw_message: (Vec<u8>, Vec<UntypedHandle>),
            state_weak: &Weak<Mutex<StateTy>>,
            sender: &ResponseSender,
        ) {
            // FOR_RELEASE: This indicates a malformed mojo message, we should figure out
            // what to do about those
            let message: MojomMessage = MojomMessage::from_bytes(raw_message.0)
                .expect("Incoming response failed to parse!");

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
                    |mut response: MojomMessage| {
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
} // End mod receiver
