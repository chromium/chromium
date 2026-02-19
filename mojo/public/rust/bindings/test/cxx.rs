// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge(namespace = "bindings_unittests::mojom")]
pub mod ffi {
    unsafe extern "C++" {
        include!("mojo/public/rust/bindings/test/cpp/cxx_shim.h");
        fn BindPlusSevenMathService(handle: usize);
        fn TestRemoteFromCpp(handle: usize);
    }
}
