// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This crate contains the definition and FFI bindings for the RustOnceClosure
//! type, and possibly other base callback types later.

#[cxx::bridge(namespace = "rust_sequences")]
mod ffi {
    extern "Rust" {
        #[derive(ExternType)]
        type RustOnceClosure;

        #[Self = "RustOnceClosure"]
        fn run(boxed: Box<RustOnceClosure>);
    }
}
