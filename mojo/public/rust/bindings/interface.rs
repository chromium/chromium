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
  "//mojo/public/rust/system";
  "//mojo/public/rust/sequences";
}

use std::collections::HashMap;
use std::marker::PhantomData;
// FOR_RELEASE: Replace some/all Arc/Mutexes with the sequenced equivalents,
// where appropriate (maybe all of them?).
// TODO(crbug.com/477584253): Replace std::sync with std::nonpoison once
// it's stabilized, if any uses remain.
use std::sync::{Arc, Mutex, Weak};

use sequences::SequencedTaskRunnerHandle;
use system::message::RawMojoMessage;
use system::message_pipe::MessageEndpoint;
use system::mojo_types::{MojoResult, UntypedHandle};

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
    impl<T> ImplementThisViaMacro for crate::remote::Remote<T> where T: DynMojomInterface + ?Sized {}
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
