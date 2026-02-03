// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines (mostly) safe Rust wrappers around miscellaneous Mojo
//! functions.
//!
//! Not all C API functions are included yet. More can be added as-needed by
//! following the example of existing wrappers.

chromium::import! {
  "//mojo/public/rust/system:mojo_c_system_bindings" as raw_ffi;
}

/// Returns the time, in microseconds, since some unspecified point in the past.
/// The values are only meaningful relative to other values that were obtained
/// from the same device without an intervening system restart. Such values are
/// guaranteed to be monotonically non-decreasing with the passage of real time.
/// Although the units are microseconds, the resolution of the clock may vary
/// and is typically in the range of ~1-15 ms.
pub fn MojoGetTimeTicksNow() -> i64 {
    unsafe { raw_ffi::MojoGetTimeTicksNow() }
}
