// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_RUST_GTEST_INTEROP_RUST_GTEST_INTEROP_H_
#define TESTING_RUST_GTEST_INTEROP_RUST_GTEST_INTEROP_H_

#include <stdint.h>

#include "third_party/rust/cxx/v1/crate/include/cxx.h"

namespace rust_gtest_interop {

// Register a test to be run via GTest. This must be called before main(), as
// there's no calls from C++ into Rust to collect tests. Any function given to
// this function will be included in the set of tests run by the RUN_ALL_TESTS()
// invocation.
//
// This function is meant to be called from Rust, for any test functions
// decorated by a #[gtest(Suite, Test)] macro, which is provided to Rust by this
// same GN target.
//
// SAFETY: This function makes copies of the strings so the pointers do not need
// to outlive the function call.
void rust_gtest_add_test(void (*test_fn)(),
                         const char* test_suite_name,
                         const char* test_name,
                         const char* file,
                         int32_t line);

// Report a test failure at a given file and line tuple, with a provided
// message.
//
// This function is meant to be called from Rust tests, via expect_eq!() and
// similar macros. It's present in a header for generating bindings from Rust.
//
// We use `unsigned char` and `int32_t` because CXX does not support
// std::os::raw::c_char or std::os::raw::c_int. See
// https://github.com/dtolnay/cxx/issues/1015.
//
// SAFETY: This function makes copies of the strings so the pointers do not need
// to outlive the function call.
void rust_gtest_add_failure_at(const unsigned char* file,
                               int32_t line,
                               rust::Str message);

}  // namespace rust_gtest_interop

#endif  // TESTING_RUST_GTEST_INTEROP_RUST_GTEST_INTEROP_H_
