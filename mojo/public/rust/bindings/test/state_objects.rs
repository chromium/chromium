// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines several state objects corresponding to the definitions
//! in `bindings_unittests.test-mojom`. They can be used conveniently in tests,
//! and also serve as coverage for the syntax of the
//! `add_mojom_state_object_impls` macro.

chromium::import! {
    "//mojo/public/rust/bindings";
    "//mojo/public/rust/bindings/test:bindings_unittests_mojom_rust";
    "//mojo/public/rust/system";
}

use std::sync::{Arc, Mutex};

use bindings::receiver::{AssociatedReceiver, PendingAssociatedReceiver};
use bindings::register_mojom_state_object_impls;
use bindings::remote::{AssociatedRemote, PendingAssociatedRemote};

use bindings_unittests_mojom_rust::bindings_unittests as test_mojom;
use test_mojom::{AssociatedSender, HandleService, MathService, TwoInts, TypemapService};

// Various implementers of the `MathService` interface

// Wraps around if overflow would happen
pub struct WrappingMathService {}

impl MathService for WrappingMathService {
    fn Add(&mut self, a: u32, b: u32, send_response: impl FnOnce(u32)) {
        send_response(u32::wrapping_add(a, b))
    }

    fn AddTwoInts(&mut self, ns: TwoInts, send_response: impl FnOnce(u32)) {
        // Too small to overflow!
        send_response(u32::from(ns.a) + u32::from(ns.b))
    }
}

register_mojom_state_object_impls!(impl MathService for WrappingMathService);

// Uses saturating operations
pub struct SaturatingMathService {}

impl MathService for SaturatingMathService {
    fn Add(&mut self, a: u32, b: u32, send_response: impl FnOnce(u32)) {
        send_response(u32::saturating_add(a, b))
    }

    fn AddTwoInts(&mut self, ns: TwoInts, send_response: impl FnOnce(u32)) {
        // Too small to overflow!
        send_response(u32::from(ns.a) + u32::from(ns.b))
    }
}

register_mojom_state_object_impls!(impl MathService for SaturatingMathService);

// Calls a user-provided function with the result of each addition before
// sending a response
pub struct NotifyingMathService<F: FnMut(u32) + Send> {
    pub f: F,
}

impl<F: FnMut(u32) + Send> MathService for NotifyingMathService<F> {
    fn Add(&mut self, a: u32, b: u32, send_response: impl FnOnce(u32)) {
        (self.f)(a + b);
        send_response(a + b)
    }

    fn AddTwoInts(&mut self, ns: TwoInts, send_response: impl FnOnce(u32)) {
        // Too small to overflow!
        let ret = u32::from(ns.a) + u32::from(ns.b);
        (self.f)(ret);
        send_response(ret)
    }
}

register_mojom_state_object_impls!(
    impl<F> MathService for NotifyingMathService<F> where F: FnMut(u32) + Send);

// Implementer of the `HandleService` interface which notifies a
// user-provided closure when it receives handles.
pub struct HandleServiceImpl<F>
where
    F: FnMut(
            system::message_pipe::MessageEndpoint,
            system::message_pipe::MessageEndpoint,
            system::message_pipe::MessageEndpoint,
            system::mojo_types::UntypedHandle,
        ) + Send,
{
    pub f: F,
}

impl<F> test_mojom::HandleService for HandleServiceImpl<F>
where
    F: FnMut(
            system::message_pipe::MessageEndpoint,
            system::message_pipe::MessageEndpoint,
            system::message_pipe::MessageEndpoint,
            system::mojo_types::UntypedHandle,
        ) + Send,
{
    fn PassHandles(
        &mut self,
        h1: system::message_pipe::MessageEndpoint,
        h2: system::message_pipe::MessageEndpoint,
        h3: system::message_pipe::MessageEndpoint,
        h4: system::mojo_types::UntypedHandle,
    ) {
        (self.f)(h1, h2, h3, h4)
    }
}

register_mojom_state_object_impls!(
    impl<F> HandleService for HandleServiceImpl<F>
    where F: FnMut(
        system::message_pipe::MessageEndpoint,
        system::message_pipe::MessageEndpoint,
        system::message_pipe::MessageEndpoint,
        system::mojo_types::UntypedHandle,
    ) + Send
);

// A service that notifies when it is dropped.
pub struct DropNotifyingService {
    pub dropped: Arc<Mutex<bool>>,
    pub quit_loop: Box<dyn Fn() + Send>,
}

impl MathService for DropNotifyingService {
    fn Add(&mut self, _a: u32, _b: u32, _send_response: impl FnOnce(u32)) {}
    fn AddTwoInts(&mut self, _ns: TwoInts, _send_response: impl FnOnce(u32)) {}
}

impl Drop for DropNotifyingService {
    fn drop(&mut self) {
        *self.dropped.lock().expect("Mutex poisoned") = true;
        (self.quit_loop)();
    }
}

register_mojom_state_object_impls!(impl MathService for DropNotifyingService);

pub struct TypemapServiceImpl {}

impl TypemapService for TypemapServiceImpl {
    fn Echo(
        &mut self,
        s: test_mojom::MyCustomStruct,
        response_callback: impl FnOnce(test_mojom::MyCustomStruct),
    ) {
        response_callback(s);
    }
}

bindings::register_mojom_state_object_impls!(impl TypemapService for TypemapServiceImpl);

// Convenience Wrapper
type SharedOpt<T> = Arc<Mutex<Option<T>>>;

/// This object is used for testing associated interfaces.
/// It can both create and receive associated remotes and receivers,
/// and stores them in an Arc. Each field corresponds to one message
/// of the interface, and contains the last thing send for that message.
/// It is expected that the test will use one of these as a state object,
/// but clone it first so it can access the fields for testing purposes.
#[derive(Clone)] // Cloning makes a shallow copy
pub struct AssociatedSenderImpl {
    // Stores the remote/receiver that was sent to this object
    pub send_remote: SharedOpt<AssociatedRemote<dyn MathService>>,
    pub send_receiver: SharedOpt<PendingAssociatedReceiver<dyn MathService>>,
    // Stores the other end of the remote/receiver that we sent to the client
    pub request_remote: SharedOpt<AssociatedReceiver<SaturatingMathService>>,
    pub request_receiver: SharedOpt<AssociatedRemote<dyn MathService>>,
}

impl AssociatedSenderImpl {
    pub fn new() -> Self {
        Self {
            send_remote: Arc::new(Mutex::new(None)),
            send_receiver: Arc::new(Mutex::new(None)),
            request_remote: Arc::new(Mutex::new(None)),
            request_receiver: Arc::new(Mutex::new(None)),
        }
    }
}

impl AssociatedSender for AssociatedSenderImpl {
    fn SendRemote(&mut self, remote: PendingAssociatedRemote<dyn MathService>) {
        *self.send_remote.lock().unwrap() = Some(remote.bind());
    }
    fn SendReceiver(&mut self, receiver: PendingAssociatedReceiver<dyn MathService>) {
        *self.send_receiver.lock().unwrap() = Some(receiver);
    }
    fn RequestRemote(
        &mut self,
        response_callback: impl Send + 'static + FnOnce(PendingAssociatedRemote<dyn MathService>),
    ) {
        let (remote, receiver) = PendingAssociatedRemote::new_pair();
        *self.request_remote.lock().unwrap() = Some(receiver.bind(SaturatingMathService {}));
        response_callback(remote);
    }
    fn RequestReceiver(
        &mut self,
        response_callback: impl Send + 'static + FnOnce(PendingAssociatedReceiver<dyn MathService>),
    ) {
        let (remote, receiver) = PendingAssociatedRemote::new_pair();
        *self.request_receiver.lock().unwrap() = Some(remote.bind());
        response_callback(receiver);
    }
    fn ClearActiveEndpoints(&mut self) {
        *self.send_remote.lock().unwrap() = None;
        *self.send_receiver.lock().unwrap() = None;
        *self.request_remote.lock().unwrap() = None;
        *self.request_receiver.lock().unwrap() = None;
    }
}

bindings::register_mojom_state_object_impls!(impl AssociatedSender for AssociatedSenderImpl);

pub struct AssociatedSenderInteropRustImpl;

impl AssociatedSender for AssociatedSenderInteropRustImpl {
    fn SendRemote(&mut self, _remote: PendingAssociatedRemote<dyn MathService>) {}

    fn SendReceiver(&mut self, receiver: PendingAssociatedReceiver<dyn MathService>) {
        receiver.bind_self_owned(SaturatingMathService {});
    }

    fn RequestRemote(
        &mut self,
        response_callback: impl Send + 'static + FnOnce(PendingAssociatedRemote<dyn MathService>),
    ) {
        let (remote, receiver) = PendingAssociatedRemote::<dyn MathService>::new_pair();
        receiver.bind_self_owned(SaturatingMathService {});
        response_callback(remote);
    }

    fn RequestReceiver(
        &mut self,
        _response_callback: impl Send + 'static + FnOnce(PendingAssociatedReceiver<dyn MathService>),
    ) {
    }

    fn ClearActiveEndpoints(&mut self) {}
}

bindings::register_mojom_state_object_impls!(impl AssociatedSender for AssociatedSenderInteropRustImpl);
