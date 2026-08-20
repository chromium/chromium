// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//mojo/public/rust/bindings";
    "//mojo/public/rust/system";
}

use crate::tests::BindRustMathServiceReceiver;

#[cxx::bridge(namespace = "bindings_unittests::mojom")]
pub mod ffi {
    #[namespace = "mojo::rust::bindings"]
    unsafe extern "C++" {
        include!("mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h");
        type AssociatedEndpointRustAdapter =
            super::bindings::cxx_associated_endpoint::ffi::AssociatedEndpointRustAdapter;
    }

    extern "Rust" {
        fn BindRustMathServiceReceiver(adapter: UniquePtr<AssociatedEndpointRustAdapter>);
    }

    unsafe extern "C++" {
        include!("mojo/public/rust/bindings/test/cpp/cxx_shim.h");
        include!("mojo/public/rust/bindings/test/cpp/add_seven_service.h");
        include!("mojo/public/rust/system/scoped_handle_interop.h");

        type PlusSevenMathService;

        #[namespace = "mojo::rust"]
        type ScopedMessagePipeHandleWrapper =
            super::system::scoped_handle_interop::ScopedMessagePipeHandleWrapper;

        #[namespace = "mojo::rust"]
        type ScopedHandleWrapper = crate::cxx::system::scoped_handle_interop::ScopedHandleWrapper;

        fn CreatePlusSevenMathService(
            handle: UniquePtr<ScopedMessagePipeHandleWrapper>,
        ) -> UniquePtr<PlusSevenMathService>;

        fn TestRemoteFromCpp(handle: UniquePtr<ScopedMessagePipeHandleWrapper>);

        fn CreatePlusSevenMathServiceAndRemote(
            service_out: &mut UniquePtr<PlusSevenMathService>,
            remote_out: &mut UniquePtr<ScopedMessagePipeHandleWrapper>,
        );

        fn CreateCppAssociatedSender(handle: UniquePtr<ScopedMessagePipeHandleWrapper>);
        fn CreateAssociatedSenderInteropTest(handle: UniquePtr<ScopedMessagePipeHandleWrapper>);

        type AssociatedSenderTestRemote;

        fn CreateAssociatedSenderTestRemote(
            handle: UniquePtr<ScopedMessagePipeHandleWrapper>,
        ) -> UniquePtr<AssociatedSenderTestRemote>;

        fn RequestRemote(
            self: Pin<&mut AssociatedSenderTestRemote>,
        ) -> UniquePtr<AssociatedEndpointRustAdapter>;

        fn SendReceiver(
            self: Pin<&mut AssociatedSenderTestRemote>,
            receiver_adapter: UniquePtr<AssociatedEndpointRustAdapter>,
        );
    }
}
