// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//mojo/public/rust:mojo_system_test_support" as test_support;
}

/// Macro to produce a test which uses the stub Mojo backend. This is used
/// instead of the macro from `test_util`, which initializes the full Mojo
/// implementation.
macro_rules! stubbed_mojo_test {
    {$suite: ident, $t: ident, $(#[$attr:meta])* $b:block} => {
        #[::rust_gtest_interop::prelude::gtest($suite, $t)]
        $(
        #[ $attr ]
        )*
        fn test() {
            crate::init();
            $b
        }
    }
}

mod encoding;
mod mojom_validation;
mod regression;
mod validation;

/// Initialize the stub Mojo implementation.
fn init() {
    use std::sync::Once;
    static START: Once = Once::new();

    START.call_once(|| unsafe {
        test_support::set_stub_thunks();
    });
}
