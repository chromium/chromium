// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use autocxx::prelude::*;
use content_private_test_support::extern_browsertest_suite;

include_cpp! {
    #include "content/public/test/content_browser_test.h"
    generate!("content::ContentBrowserTest")

    // TODO(danakj): Declare `extern_cpp_type()` for other types that are exposed by and/or used by
    // the browser_test_base.h file:
    // https://docs.rs/autocxx/latest/autocxx/macro.extern_cpp_type.html
}

/// The TestSuite class for writing content-based browser tests. To write a content browsertest from
/// rust, use the `#[gtest(GroupName, TestName)]` macro to mark your function as a test, and the
/// `#[gtest_suite(ContentBrowserTest)]` macro to have the test use the C++ ContentBrowserTest class
/// as the Gtest TestSuite. When the `#[gtest_suite]` macro is used, it also defines what the test's
/// input argument should be, which is a reference to that same type.
///
/// # Example
/// ```
/// #[gtest(MyFeatureTest, Example)]
/// #[gtest_suite(ContentBrowserTest)]
/// fn test(_bt: Pin<&mut ContentBrowserTest>) {
/// }
/// ```
pub use ffi::content::ContentBrowserTest;

#[extern_browsertest_suite("content::ContentBrowserTest")]
unsafe impl rust_gtest_interop::TestSuite for ContentBrowserTest {}
