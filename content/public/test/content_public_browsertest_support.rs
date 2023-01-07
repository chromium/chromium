// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Crate root for the content_public_browsertest_support crate.
//!
//! This file lists the set of source files included in the crate and defines what modules and
//! things are made public to users of the crate.

#[path = "content_browser_test.rs"]
mod content_browser_test;

pub use content_browser_test::ContentBrowserTest;
