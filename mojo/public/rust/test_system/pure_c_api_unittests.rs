//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use rust_gtest_interop::prelude::*;

chromium::import! {
    pub "//mojo/public/rust:mojo_rust_system_api" as system;
    pub "//mojo/public/rust/test_support:test_util" as test_util;
}

// This file is meant to mimic the tests in
// //mojo/public/c/system/tests/core_unittest_pure_c.c.
// These tests are thus somewhat redundant, but useful for ensuring that we're
// wrapping certain C API functions in a sensible way.
#[gtest(RustSystemAPITestSuite, MojoTimeTicksTest)]
fn test_ticks() {
    // FOR_RELEASE: Right now we're calling init_mojo on a per-test basis, but
    // may be worth seeing if there's some way to do gtest setup/teardown in
    // Rust.
    test_util::init_mojo_if_needed();

    // get_time_ticks_now should increase monotonically.
    let ticks: system::mojo_types::MojoTimeTicks = system::mojo_types::get_time_ticks_now();
    assert_ne!(ticks, 0);
}

// FOR_RELEASE: Fill out the remaining (relevant) tests from
// core_unittest_pure_c.c.
