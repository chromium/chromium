// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines (mostly) safe Rust wrappers around the Mojo data pipe
//! API.
//!
//! Not all C API functions are included yet. More can be added as-needed by
//! following the example of existing wrappers.

// We're re-exporting C functions with a different naming convention. We want
// to keep the same names to make sure the correspondence is clear.
#![allow(non_snake_case)]

chromium::import! {
  "//mojo/public/rust/system:mojo_c_system_bindings" as raw_ffi;
}

use crate::internal_options::declare_mojo_options;
use crate::mojo_handles::*;
use crate::mojo_result::*;

bitflags::bitflags! {
    /// The possible flags that can be passed to MojoWriteData
    #[derive(Clone, Copy, Default)]
    pub struct WriteFlags: u32 {
        /// Write all the data to the pipe if possible or none at all.
        const ALL_OR_NONE = 1 << 0;
    }
}

declare_mojo_options!(MojoWriteDataOptions, flags: raw_ffi::MojoWriteDataFlags);

/// Writes as much of the data in `elements` as possible to the given data pipe
/// producer. On success, returns the number of bytes written.
///
/// Requirements:
/// - The handle must actually be a data pipe producer handle
/// - The length of `elements` must be a multiple of the data pipe's element
///   size.
/// - If the length of `elements` is greater than u32::MAX, then u32::MAX must
///   be a multiple of the data pipe's element size.
///
/// Options:
/// - `ALL_OR_NONE`: Fail and write nothing if the pipe does not have enough
///   space to write all data in `elements`.
///
/// Possible Error Codes:
/// - `InvalidArgument`: If one of the requirements above was violated.
/// - `FailedPrecondition`: If the data pipe's consumer handle has been closed.
/// - `OutOfRange`: If the `ALL_OR_NONE` option was passed and the pipe did not
///   have enough room to write all data in `elements`.
/// - `Busy`: If there is a two-phase write ongoing with
///   `data_pipe_producer_handle` (i.e., `MojoBeginWriteData` has been called,
////   but not yet the matching `MojoEndWriteData`)
/// - `ShouldWait`: If no data can currently be written, but the corresponding
///   consumer handle is still open, and `ALL_OR_NONE` was not passed.
pub fn MojoWriteData(
    data_pipe_producer_handle: &mut UntypedHandle,
    elements: &[u8],
    flags: WriteFlags,
) -> MojoResult<u32> {
    let mut num_elements: u32 = elements.len().try_into().unwrap_or(u32::MAX);
    let options = MojoWriteDataOptions::new(flags.bits());

    // SAFETY: We have exclusive access to the handle so this is thread-safe.
    // The `UntypedHandle` type guarantees the handle is alive. The pointers are
    // obtained from references and hence valid. The `num_elements`
    // pointer stores the number of bytes in `elements`, so we will not read past
    // the end of `elements`.
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoWriteData(
            data_pipe_producer_handle.handle_value.into(),
            elements.as_ptr().cast(),
            std::ptr::from_mut(&mut num_elements),
            options.as_ptr(),
        )
    });

    ret.map(|_| num_elements)
}
