// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mojo/Rust tests use constants like `3.14` to verify round-tripping.
// The tests don't care about using an accurate value of PI.
#![allow(clippy::approx_constant)]

mod helpers;
mod test_headers;
mod test_mojomparse;
mod test_parser;
