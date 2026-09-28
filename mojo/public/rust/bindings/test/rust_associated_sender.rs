// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Helper shim allowing C++ to invoke methods on a Rust Remote<dyn
//! AssociatedSender>.

chromium::import! {
    "//mojo/public/rust/bindings";
    "//mojo/public/rust/bindings/test:bindings_unittests_mojom_rust";
    "//base:run_loop";
}

use std::sync::{Arc, Mutex};

use bindings::remote::Remote;
use bindings_unittests_mojom_rust::bindings_unittests as test_mojom;
use cxx::UniquePtr;
use run_loop::RunLoop;
use test_mojom::AssociatedSender;

use bindings::CxxPendingAssociatedEndpoint;

/// Wraps a Rust Remote<dyn AssociatedSender> so C++ can invoke its methods.
pub struct RustAssociatedSender {
    remote: Remote<dyn AssociatedSender>,
}

impl RustAssociatedSender {
    /// Creates a new RustAssociatedSender wrapping remote.
    pub fn new(remote: Remote<dyn AssociatedSender>) -> Self {
        Self { remote }
    }
}

/// Calls RequestRemote, waits for the response, and returns the endpoint to
/// C++.
#[allow(non_snake_case)]
pub fn RequestRemote(sender: &mut RustAssociatedSender) -> UniquePtr<CxxPendingAssociatedEndpoint> {
    let pending_remote = Arc::new(Mutex::new(None));
    let pending_remote_clone = pending_remote.clone();
    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    sender.remote.RequestRemote(move |remote| {
        *pending_remote_clone.lock().unwrap() = Some(remote);
        quit();
    });
    run_loop.run();

    let remote = pending_remote.lock().unwrap().take().expect("Should have received remote");
    remote.into_cpp()
}

/// Calls RequestHandleRemote, waits for the response, and returns the endpoint
/// to C++.
#[allow(non_snake_case)]
pub fn RequestHandleRemote(
    sender: &mut RustAssociatedSender,
) -> UniquePtr<CxxPendingAssociatedEndpoint> {
    let pending_remote = Arc::new(Mutex::new(None));
    let pending_remote_clone = pending_remote.clone();
    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    sender.remote.RequestHandleRemote(move |remote| {
        *pending_remote_clone.lock().unwrap() = Some(remote);
        quit();
    });
    run_loop.run();

    let remote = pending_remote.lock().unwrap().take().expect("Should have received remote");
    remote.into_cpp()
}
