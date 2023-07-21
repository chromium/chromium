// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Code to generate an artificial Rust panic to exercise our crash handling
//! logic and ensure it works for Rust crashes.

#[cxx::bridge(namespace = "blink")]
mod ffi {
    extern "Rust" {
        fn crash_in_rust(); // step 1 of main crash trigger. We bounce back to C++
        // then back to Rust to ensure crossing the language
        // boundary in both directions is represented in crash
        // dumps.
        fn reenter_rust(); // step 3

        fn crash_in_rust_with_overflow(); // separate crash trigger
    }

    #[namespace = "blink"]
    unsafe extern "C++" {
        include!("third_party/blink/common/rust_crash/rust_crash.h");
        fn EnterCppForRustCrash(); // step 2
    }
}

/// Create a crash within Rust code. This will call into C++ first
/// then return to Rust to ensure we can cope with fully mixed call stacks.
#[inline(never)]
fn crash_in_rust() {
    ffi::EnterCppForRustCrash();
}

#[inline(never)]
fn reenter_rust() {
    some_mod::another_rust_function();
}

/// Unnecessary mod to ensure reasonable representation of Rust
/// name mangled mods in our crash dumps.
mod some_mod {
    #[inline(never)]
    pub fn another_rust_function() {
        panic!("Crash from within Rust code.");
    }
}

/// Code that's only enabled if we're doing an ASAN build.
#[cfg(feature = "rust_crash_asan_enabled")]
#[inline(never)]
fn crash_in_rust_with_overflow() {
    let mut some_array = Box::new([1usize, 2usize, 3usize, 4usize]);
    let array_ptr = &mut some_array[0] as *mut usize;
    unsafe {
        let bad_array_ptr = array_ptr.offset(4);
        std::ptr::write_volatile(bad_array_ptr, 42);
    }
}

#[cfg(not(feature = "rust_crash_asan_enabled"))]
fn crash_in_rust_with_overflow() {
    unreachable!()
}
