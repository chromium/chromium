// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

pub use ffi::TestSubclass;

// TODO(adetaylor): Use autcxx here, but since TestSubclass is an abstract class it falls over
// trying to make a unique_ptr constructor for the type.
// include_cpp! {
//     #include "testing/rust_gtest_interop/test/test_subclass.h"
//     generate!("rust_gtest_interop::TestSubclass")
//     safety!(unsafe_ffi)
// }
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("testing/rust_gtest_interop/test/test_subclass.h");
        #[namespace = "rust_gtest_interop"]
        type TestSubclass;
        fn get_true(self: Pin<&mut TestSubclass>) -> bool;
        fn get_false(self: Pin<&mut TestSubclass>) -> bool;
        fn num_calls(self: &TestSubclass) -> i32;
    }
}

#[extern_test_suite("rust_gtest_interop::TestSubclass")]
unsafe impl rust_gtest_interop::TestSuite for ffi::TestSubclass {}
