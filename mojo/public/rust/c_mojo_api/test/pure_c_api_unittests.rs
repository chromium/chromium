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

#[gtest(RustSystemAPITestSuite, MojoPlatformHandleTest)]
fn test_platform_handle() {
    let temp_dir = std::env::temp_dir();
    let temp_file_path = temp_dir.join("mojo_rust_test_file");
    let file = std::fs::File::create(&temp_file_path).unwrap();

    #[cfg(target_family = "unix")]
    {
        use std::os::fd::OwnedFd;
        let owned = OwnedFd::from(file);
        let platform_handle = mojo_ffi::platform::PlatformHandle::from(owned);

        // Wrap the platform handle
        let mojo_handle = mojo_ffi::platform::MojoWrapPlatformHandle(platform_handle).unwrap();

        // Unwrap the platform handle
        let unwrapped_platform_handle =
            mojo_ffi::platform::MojoUnwrapPlatformHandle(mojo_handle).unwrap();
        let unwrapped_owned = OwnedFd::from(unwrapped_platform_handle);

        // Verify the unwrapped handle works by creating a File from it and writing to
        // it.
        let mut unwrapped_file = std::fs::File::from(unwrapped_owned);
        use std::io::Write;
        unwrapped_file.write_all(b"hello").unwrap();
    }

    #[cfg(target_family = "windows")]
    {
        use std::os::windows::io::OwnedHandle;
        let owned = OwnedHandle::from(file);
        let platform_handle = mojo_ffi::platform::PlatformHandle::from(owned);

        // Wrap the platform handle
        let mojo_handle = mojo_ffi::platform::MojoWrapPlatformHandle(platform_handle).unwrap();

        // Unwrap the platform handle
        let unwrapped_platform_handle =
            mojo_ffi::platform::MojoUnwrapPlatformHandle(mojo_handle).unwrap();
        let unwrapped_owned = OwnedHandle::from(unwrapped_platform_handle);

        // Verify the unwrapped handle works
        let mut unwrapped_file = std::fs::File::from(unwrapped_owned);
        use std::io::Write;
        unwrapped_file.write_all(b"hello").unwrap();
    }

    // Clean up
    let _ = std::fs::remove_file(temp_file_path);
}
