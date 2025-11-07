// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used to initialize Mojo in contexts where it has not already been
// initialized, e.g. in standalone Rust binaries or outside of Chromium/gtest.
pub fn init_mojo() {
    ffi::Init();
}

// FOR_RELEASE(https://crbug.com/457920507): Make this idempotent if it's not.
#[cxx::bridge(namespace = "mojo::core")]
mod ffi {
    unsafe extern "C++" {
        include!("mojo/core/embedder/embedder.h");

        fn Init();
    }
}
