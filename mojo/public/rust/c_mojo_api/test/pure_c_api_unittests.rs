// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use rust_gtest_interop::prelude::*;

chromium::import! {
    "//mojo/public/rust/c_mojo_api" as mojo_ffi;
    "//mojo/public/rust/system/test_util";
}

// This file is meant to mimic the tests in
// //mojo/public/c/system/tests/core_unittest_pure_c.c.
// These tests are thus somewhat redundant, but useful for ensuring that we're
// wrapping certain C API functions in a sensible way.
#[gtest(RustSystemAPITestSuite, MojoTimeTicksTest)]
fn test_ticks() {
    // get_time_ticks_now should increase monotonically.
    let ticks = mojo_ffi::functions::MojoGetTimeTicksNow();
    assert_ne!(ticks, 0);
}

// TODO(crbug.com/498965233): Fill out the remaining (relevant) tests from
// core_unittest_pure_c.c.
