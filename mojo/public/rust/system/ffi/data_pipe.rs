// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines (mostly) safe Rust wrappers around the Mojo data pipe
//! API.
//!
//! Not all C API functions are included yet. More can be added as-needed by
//! following the example of existing wrappers.

chromium::import! {
  "//mojo/public/rust/system:mojo_c_system_bindings" as raw_ffi;
}

use crate::handles::*;
use crate::internal_options::declare_mojo_options;
use crate::result::*;

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
/// # Requirements:
/// - The handle must actually be a data pipe producer handle
/// - The length of `elements` must be a multiple of the data pipe's element
///   size.
/// - If the length of `elements` is greater than u32::MAX, then u32::MAX must
///   be a multiple of the data pipe's element size.
///
/// # Options:
/// - `ALL_OR_NONE`: Fail and write nothing if the pipe does not have enough
///   space to write all data in `elements`.
///
/// # Possible Error Codes:
/// - `InvalidArgument`: If one of the requirements above was violated.
/// - `FailedPrecondition`: If the data pipe's consumer handle has been closed.
/// - `OutOfRange`: If the `ALL_OR_NONE` option was passed and the pipe did not
///   have enough room to write all data in `elements`.
/// - `Busy`: If there is a two-phase write ongoing with
///   `data_pipe_producer_handle` (i.e., `MojoBeginWriteData` has been called,
///   but not yet the matching `MojoEndWriteData`).
/// - `ShouldWait`: If no data can currently be written, but the corresponding
///   consumer handle is still open, and `ALL_OR_NONE` was not passed.
pub fn MojoWriteData(
    data_pipe_producer_handle: &UntypedHandle,
    elements: &[u8],
    flags: WriteFlags,
) -> MojoResult<u32> {
    let mut num_elements: u32 = elements.len().try_into().unwrap_or(u32::MAX);
    let options = MojoWriteDataOptions::new(flags.bits());

    // SAFETY: The `UntypedHandle` type guarantees the handle is alive. The pointers
    // are obtained from references and hence valid. The `num_elements`
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

bitflags::bitflags! {
    /// The possible flags that can be passed to MojoReadData
    /// All flags except `ALL_OR_NONE` are mutually exclusive
    #[derive(Clone, Copy, Default)]
    pub struct ReadFlags: u32 {
        /// Read all the data from the pipe if possible, or none at all.
        const ALL_OR_NONE = 1 << 0;
        /// Dequeue the message received rather than reading it.
        const DISCARD = 1 << 1;
        /// Get information about the queue on the pipe but don't read it
        const QUERY = 1 << 2;
        /// Read data off the pipe's queue but do not dequeue it.
        const PEEK = 1 << 3;
    }
}

declare_mojo_options!(MojoReadDataOptions, flags: raw_ffi::MojoReadDataFlags);

// FOR_RELEASE: We should probably replace this with separate `read`, `discard`,
// `query`, and `peek` functions. The various caveats in the description are
// silly, and I see no downside to doing it here.
// FOR_RELEASE: We should also have fully-safe equivalents that take in an array
// of known-initialized bytes, or allocate the memory themselves.

/// Read as much data as possible from given data pipe into `elements`. On
/// success, returns the number of bytes read. Flags may be passed to make this
/// function discard data instead of copying it, leave it in the pipe after
/// reading, or simply query the amount of bytes that may be read.
///
/// # Safety Guarantees:
/// If the `DISCARD` or `QUERY` flags were not passed, and this function
/// returns Ok(n), then the first `n` bytes of `elements` have been initialized.
///
/// # Requirements:
/// - The handle must actually be a data pipe consumer handle.
/// - The length of `elements` must be a multiple of the data pipe's element
///   size, unless the `QUERY` or `DISCARD` flags are passed.
/// - If the length of `elements` is greater than u32::MAX, then u32::MAX must
///   be a multiple of the data pipe's element size, unless the `QUERY` or
///   `DISCARD` flags are passed.
/// - The flags DISCARD, QUERY, and PEEK are mutually exclusive.
///
/// # Options:
/// - `ALL_OR_NONE`: Fail and do nothing if the pipe does not contain at least
///   `elements.len()` bytes. Ignored if `QUERY` is passed.
/// - `PEEK`: Don't remove the data from the pipe after reading.
/// - `DISCARD`: Only remove data from the pipe; don't modify `elements`.
/// - `QUERY`: Don't modify the pipe or `elements`, just return the number of
///   bytes available to be read.
///
/// # Possible Error Codes:
/// - `InvalidArgument`: If one of the requirements above was violated.
/// - `FailedPrecondition`: If the data pipe's producer handle has been closed
///   and there was not (enough) data to be read.
/// - `OutOfRange`: If the `ALL_OR_NONE` option was passed, the producer is
///   still open, and the pipe did not have enough data to completely fill
///   `elements`.
/// - `Busy`: If there is a two-phase read ongoing with
///   `data_pipe_producer_handle` (i.e., `MojoBeginReadData` has been called,
///   but not yet the matching `MojoEndReadData`).
/// - `ShouldWait`: If no data can currently be read, but the corresponding
///   consumer handle is still open, and `ALL_OR_NONE` was not passed.
pub fn MojoReadData(
    data_pipe_consumer_handle: &UntypedHandle,
    elements: &mut [std::mem::MaybeUninit<u8>],
    flags: ReadFlags,
) -> MojoResult<u32> {
    let mut num_elements: u32 = elements.len().try_into().unwrap_or(u32::MAX);
    let options = MojoReadDataOptions::new(flags.bits());

    // SAFETY: The `UntypedHandle` type guarantees the handle is alive. The pointers
    // are obtained from references and hence valid. The `num_elements`
    // pointer stores the number of bytes in `elements`, so we will not write past
    // the end of `elements`.
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoReadData(
            data_pipe_consumer_handle.handle_value.into(),
            options.as_ptr(),
            elements.as_mut_ptr().cast(),
            std::ptr::from_mut(&mut num_elements),
        )
    });

    ret.map(|_| num_elements)
}

declare_mojo_options!(
    MojoCreateDataPipeOptions,
    // No flags actually exist for this function, but the struct still has a
    // slot for them.
    flags: raw_ffi::MojoCreateDataPipeFlags,
    element_num_bytes: u32,
    capacity_num_bytes: u32
);

/// This is a wrapper type for data pipe handles to make it clear which handle
/// belongs to the producer vs. the consumer.
pub struct MojoDataPipe {
    pub producer: UntypedHandle,
    pub consumer: UntypedHandle,
}

/// Create a data pipe, which is a unidirectional channel for transmitting raw
/// bytes. Bytes must be sent in chunks of `element_num_bytes`. The pipe's
/// capacity is the minimum number of bytes that can be queued in the pipe.
///
/// If successful, this function returns the newly-created pipe's handles as a
/// (Producer, Consumer) pair.
///
/// # Requirements:
/// - `element_num_bytes` must be nonzero.
/// - `capacity_num_bytes` must be zero or a multiple of `element_num_bytes`. If
///   zero, the capacity is arbitrary (but at least 1).
///
/// # Possible Error Codes:
/// - `InvalidArgument`: If one of the requirements above was violated.
/// - `ResourceExhausted`: If a process/system/quota/etc. limit has been reached
///   (e.g., if the requested capacity was too large, or if the maximum number
///   of handles was exceeded).
pub fn MojoCreateDataPipe(
    element_num_bytes: u32,
    capacity_num_bytes: u32,
) -> MojoResult<MojoDataPipe> {
    let flag_bits = 0; // This function doesn't have any flags
    let options = MojoCreateDataPipeOptions::new(flag_bits, element_num_bytes, capacity_num_bytes);
    let mut producer_handle: raw_ffi::MojoHandle = 0;
    let mut consumer_handle: raw_ffi::MojoHandle = 0;

    // SAFETY: All these pointers are to stack variables, hence valid.
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoCreateDataPipe(
            options.as_ptr(),
            std::ptr::from_mut(&mut producer_handle),
            std::ptr::from_mut(&mut consumer_handle),
        )
    });

    ret.map(|_| MojoDataPipe {
        // SAFETY: We just got these handles from Mojo, so they are live and unowned.
        producer: unsafe { UntypedHandle::wrap_raw_value(producer_handle) },
        consumer: unsafe { UntypedHandle::wrap_raw_value(consumer_handle) },
    })
}
