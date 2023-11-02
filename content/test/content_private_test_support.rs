// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! The content-internal test support library.

/// The `#[extern_browsertest_suite("cpp::Type")]` macro, which is used to implement  the
/// `rust_gtest_interop::TestSuite` trait for Rust wrapper types of C++ subclasses of
/// BrowserTestBase.
///
/// TODO(danakj): This will need to move to //content/public/ in order to support subclasses
/// of BrowserTestBase outside of content, such as InProcessBrowserTest in //chrome.
pub use browser_test_macro::extern_browsertest_suite;
