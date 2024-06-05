// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    // `pub` since a macro refers to `$crate::system`.
    pub "//mojo/public/rust:mojo_system" as system;
}

pub mod data;
