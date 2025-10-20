// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: Delete this executable we've hooked up proper tests.
// This is just a convenience executable for quicker iteration in the meantime.

chromium::import! {
    pub "//mojo/public/rust:mojo_rust_system_api" as system;
}

fn main() {
    let _my_handle: system::mojo_types::MojoHandle = 1;
}
