// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

pub use ffi::rust_gtest_interop::{TestSubclass, TestSubclassWithCustomTemplate};

// TODO(adetaylor): Use autcxx here, but it is failing to work on android.
// use autocxx::prelude::*;
// include_cpp! {
//     #include "testing/rust_gtest_interop/test/test_subclass.h"
//     generate!("rust_gtest_interop::TestSubclass")
//     generate!("rust_gtest_interop::TestSubclassWithCustomTemplate")
//     safety!(unsafe_ffi)
// }

mod ffi {
    #[cxx::bridge(namespace = "rust_gtest_interop")]
    pub mod rust_gtest_interop {
        unsafe extern "C++" {
            include!("testing/rust_gtest_interop/test/test_subclass.h");
            type TestSubclass;
            fn get_true(self: Pin<&mut TestSubclass>) -> bool;
            fn get_false(self: Pin<&mut TestSubclass>) -> bool;
            fn num_calls(self: &TestSubclass) -> i32;

            type TestSubclassWithCustomTemplate;
            fn get_three(self: Pin<&mut TestSubclassWithCustomTemplate>) -> i32;
            fn get_four(self: Pin<&mut TestSubclassWithCustomTemplate>) -> i32;
            fn num_calls(self: &TestSubclassWithCustomTemplate) -> i32;
        }
    }
}

#[extern_test_suite("rust_gtest_interop::TestSubclass")]
unsafe impl rust_gtest_interop::TestSuite for TestSubclass {}

// This type uses a custom testing::Test subclass, instead of `RustTest`. The macro that constructs
// the C++ type uses the prefix "RunTestFromSetupTestFactory_" for the factory function.
#[extern_test_suite("rust_gtest_interop::TestSubclassWithCustomTemplate")]
#[cpp_prefix("RunTestFromSetupTestFactory_")]
unsafe impl rust_gtest_interop::TestSuite for TestSubclassWithCustomTemplate {}
