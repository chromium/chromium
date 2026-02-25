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

use bindings_unittests_mojom_rust::bindings_unittests as test_mojom;
use test_mojom::{HandleService, MathService, TwoInts};

use std::sync::{Arc, Mutex};

use bindings::register_mojom_state_object_impls;

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
