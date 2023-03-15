// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern crate mojo_system_test_support as test_support;
extern crate test_util as util;

/// Macro to produce a test which uses the stub Mojo backend. This is used
/// instead of the macro from `test_util`, which initializes the full Mojo
/// implementation.
macro_rules! stubbed_mojo_test {
    {$i: ident, $(#[$attr:meta])* $b:block} => {
        #[test]
        $(
        #[ $attr ]
        )*
        fn $i() {
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
