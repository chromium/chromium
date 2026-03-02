// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//mojo/public/rust/system";
}

#[cxx::bridge(namespace = "bindings_unittests::mojom")]
pub mod ffi {
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
    }
}
