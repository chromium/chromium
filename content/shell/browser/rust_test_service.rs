// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the `RustTestServiceImpl` type, which implements the
//! `RustTestService` interface defined in
//! `//content/shell/common/rust_test.test-mojom`.

chromium::import! {
    "//mojo/public/rust/bindings";
    "//content/shell:rust_test_mojom_rust";
}

use rust_test_mojom_rust::rust_test::RustTestService;

// To define an implementation of the service, we create a new type, then
// implement the `RustTestService` trait. This service isn't stateful, so we
// can use a zero-sized type.
pub struct RustTestServiceImpl {}

impl RustTestServiceImpl {
    pub fn new() -> Self {
        Self {}
    }

    fn get_string_at_index(&self, index: u32) -> &'static str {
        let messages = [
            "This string only exists in Rust code!",
            "Rust says hi.",
            "I'm Commander Rust, and this is my favorite string in the list.",
            "100% certified UTF-8.",
            "Meow.",
        ];
        messages
            .get(index as usize)
            .unwrap_or(&"Index out of bounds! Glad this wasn't a raw pointer or something.")
    }
}

// We have to implement one function for each method on the interface. They all
// have the same pattern in their arguments: &mut self, then the message
// contents, and finally (if applicable) a callback to send the response.
impl RustTestService for RustTestServiceImpl {
    fn GetStringFromRust(&mut self, index: u32, send_response: impl FnOnce(String)) {
        send_response(self.get_string_at_index(index).to_string());
    }
}

// Once we've implemented the trait, we have to call this macro, which set up
// several more trait implementations that will be used behind-the-scenes.
bindings::register_mojom_state_object_impls!(impl RustTestService for RustTestServiceImpl);

impl Default for RustTestServiceImpl {
    fn default() -> Self {
        Self::new()
    }
}
