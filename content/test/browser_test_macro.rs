// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use proc_macro2::TokenStream;
use quote::quote;

/// The prefix attached to a Gtest factory function by the RUST_BROWSERTEST_TEST_SUITE_FACTORY()
/// macro.
const RUST_BROWSERTEST_FACTORY_PREFIX: &str = "RustBrowserTestFactory_";

/// This macro should be used to implement `rust_gtest_interop::TestSuite` for Rust wrappers around
/// content::BrowserTestBase subclasses.
///
/// See `rust_gtest_interop::extern_test_suite()` macro for more.
///
/// This macro forwards through to `rust_gtest_interop::extern_test_suite()` but tells it to
/// look for use of the browser-test-specfic RUST_BROWSERTEST_TEST_SUITE_FACTORY() C++ macro.
#[proc_macro_attribute]
pub fn extern_browsertest_suite(
    arg_stream: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let arg_stream = TokenStream::from(arg_stream);
    let input = TokenStream::from(input);
    quote! {
      #[::rust_gtest_interop::prelude::extern_test_suite(#arg_stream)]
      #[cpp_prefix(#RUST_BROWSERTEST_FACTORY_PREFIX)]
      #input
    }
    .into()
}
