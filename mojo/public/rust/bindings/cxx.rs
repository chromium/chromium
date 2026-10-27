// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This file provides access to simple C++ helper functions used by the rest
//! of the bindings.

#[cxx::bridge(namespace = "rust_mojo_bindings_api_bridge")]
pub mod ffi {
    unsafe extern "C++" {
        include!("base/trace_event/trace_id_helper.h");

        /// Produces a random number, for use in tracing
        #[namespace = "base::trace_event"]
        pub fn GetNextGlobalTraceId() -> u64;
    }

    unsafe extern "C++" {
        include!("mojo/public/rust/bindings/cxx_shim.h");

        /// Returns the number of ticks since base::TimeTicks().
        pub fn CurrentTimeTicksInMicroseconds() -> i64;

        /// Validates the header of the serialized message in `data`.
        ///
        /// `data` must contain the entire message (header and payload).
        ///
        /// Failures are not reported to the sender; that's up to the caller.
        pub fn ValidateMessageHeader(data: &[u8]) -> bool;
    }
}
