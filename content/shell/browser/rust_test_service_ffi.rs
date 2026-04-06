// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This crate defines a small FFI bridge that lets us receive a receiver handle
//! from C++ code. In production code, hopefully most users will be able to get
//! their Mojo handles from existing C++-Rust pipes. However, since there aren't
//! any of those pipes yet, we have to pass the handle over FFI instead.

chromium::import! {
    "//mojo/public/rust/bindings";
    "//mojo/public/rust/system";
    "//content/shell:rust_test_mojom_rust";
    "//content/shell:rust_test_service";
}

use rust_test_mojom_rust::rust_test::RustTestService;
use rust_test_service::RustTestServiceImpl;
use system::scoped_handle_interop;

#[cxx::bridge(namespace = "content::rust_test")]
pub mod ffi {
    // This block only exists to import the ScopedMessagePipeHandleWrapper type
    // from the rust mojo bindings, so that cxx knows it's the same type.
    unsafe extern "C++" {
        include!("mojo/public/rust/system/scoped_handle_interop.h");
        #[namespace = "mojo::rust"]
        type ScopedMessagePipeHandleWrapper =
            system::scoped_handle_interop::ScopedMessagePipeHandleWrapper;
    }

    extern "Rust" {
        fn BindRustTestService(handle: UniquePtr<ScopedMessagePipeHandleWrapper>);
    }
}

/// This function is called from C++ to create a self-owned receiver in Rust.
#[allow(non_snake_case)]
fn BindRustTestService(handle: cxx::UniquePtr<ffi::ScopedMessagePipeHandleWrapper>) {
    use bindings::receiver::PendingReceiver;

    let endpoint =
        scoped_handle_interop::ScopedMessagePipeHandleWrapper::into_message_endpoint(handle)
            .unwrap();
    let pending_receiver = PendingReceiver::<dyn RustTestService>::new(endpoint);
    // This function returns a weak reference to the receiver; we have no use for it
    let _ = pending_receiver.bind_self_owned(RustTestServiceImpl::new());
}
