// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines safe Rust wrappers around Mojo's Shared Buffer API.

use crate::handles::UntypedHandle;
use crate::internal_options::declare_mojo_options;
use crate::result::*;
use core::ffi::c_void;

chromium::import! {
  "//mojo/public/rust/c_mojo_api:mojo_c_system_bindings" as raw_ffi;
}

declare_mojo_options!(MojoDuplicateBufferHandleOptions, flags: raw_ffi::MojoDuplicateBufferHandleFlags);

/// Creates a buffer of `num_bytes` bytes that can be shared between
/// processes.
///
/// To access the buffer's storage, one must call `MojoMapBuffer`. The
/// returned handle may be duplicated any number of times. Note that closing
/// the buffer handle does not automatically unmap any memory mapped by
/// `MojoMapBuffer`. You must explicitly call `MojoUnmapBuffer` to release it.
///
/// # Possible Error Codes:
/// - `ResourceExhausted`: if the system is out of memory.
/// - `InvalidArgument`: if `num_bytes` is 0.
pub fn MojoCreateSharedBuffer(num_bytes: u64) -> MojoResult<UntypedHandle> {
    let mut mojo_handle: raw_ffi::MojoHandle = 0;

    // SAFETY: All pointers are to local stack variables, hence valid.
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoCreateSharedBuffer(
            num_bytes,
            std::ptr::null(), // Pass null for default options since there are no flags
            &mut mojo_handle,
        )
    });

    ret.map(|_| {
        // SAFETY: We just got this handle from Mojo, so it is live and unowned.
        unsafe { UntypedHandle::wrap_raw_value(mojo_handle) }
    })
}

/// Create a new handle pointing to the same shared buffer as `handle`.
///
/// A shared buffer remains allocated as long as there is at least one
/// handle referencing it. The `read_only` flag determines if the new handle
/// will allow writing to the buffer. You must use the same value for the
/// `read_only` flag every time you duplicate a handle.
///
/// # Possible Error Codes:
/// - `InvalidArgument`: if `handle` is not of type `SharedBuffer`.
/// - `FailedPrecondition`: `read_only` has not been the same when used to
///   duplicate this handle in the past.
pub fn MojoDuplicateBufferHandle(
    handle: &UntypedHandle,
    read_only: bool,
) -> MojoResult<UntypedHandle> {
    let flags = if read_only { 1 } else { 0 };
    let options = MojoDuplicateBufferHandleOptions::new(flags);
    let mut new_mojo_handle: raw_ffi::MojoHandle = 0;

    // SAFETY: All pointers are to local stack variables, hence valid.
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoDuplicateBufferHandle(
            handle.handle_value.into(),
            options.as_ptr(),
            &mut new_mojo_handle,
        )
    });

    ret.map(|_| {
        // SAFETY: We just got this handle from Mojo, so it is live and unowned.
        unsafe { UntypedHandle::wrap_raw_value(new_mojo_handle) }
    })
}

/// Retrieves the size of the shared buffer referenced by `handle`.
///
/// Note that despite the name `MojoGetBufferInfo`, this function currently
/// only retrieves the size of the buffer.
///
/// # Possible Error Codes:
/// - `InvalidArgument`: if `handle` is not of type `SharedBuffer`.
pub fn MojoGetBufferInfo(handle: &UntypedHandle) -> MojoResult<u64> {
    let mut info = raw_ffi::MojoSharedBufferInfo {
        struct_size: std::mem::size_of::<raw_ffi::MojoSharedBufferInfo>() as u32,
        size: 0,
    };

    // SAFETY: All pointers are to local stack variables, hence valid.
    // struct_size has also been initialized, which is a requirement of the API.
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoGetBufferInfo(
            handle.handle_value.into(),
            std::ptr::null(),
            std::ptr::from_mut(&mut info),
        )
    });

    ret.map(|_| info.size)
}

/// Maps the part (at offset `offset` of length `num_bytes`) of the buffer given
/// by `handle` into memory.
///
/// Returns a raw pointer to the mapped memory. The returned pointer is
/// guaranteed to be valid for reads and writes up to `num_bytes` from its
/// beginning. The memory must be unmapped by calling `MojoUnmapBuffer` when
/// it is no longer needed.
///
/// This function returns a raw pointer because it cannot guarantee that no
/// other pointers or references to the mapped memory region exist. This means
/// that it cannot uphold Rust's aliasing rules, so you must verify them
/// yourself if you want to convert the pointer to a reference.
///
/// # Possible Error Codes:
/// - `InvalidArgument`: if `handle` is not of type `SharedBuffer`, `num_bytes`
///   is 0, or `offset` is not a multiple of the system page size.
/// - `ResourceExhausted`: if the mapping operation itself failed.
pub fn MojoMapBuffer(handle: &UntypedHandle, offset: u64, num_bytes: usize) -> MojoResult<*mut u8> {
    let mut buffer_ptr: *mut c_void = std::ptr::null_mut();

    // SAFETY: All pointers are to local stack variables, hence valid.
    let ret = MojoError::result_from_code(unsafe {
        raw_ffi::MojoMapBuffer(
            handle.handle_value.into(),
            offset,
            num_bytes as u64,
            std::ptr::null(), // default options
            std::ptr::from_mut(&mut buffer_ptr),
        )
    });

    ret.map(|_| buffer_ptr as *mut u8)
}

/// Unmaps a buffer pointer that was mapped by `MojoMapBuffer`.
///
/// # Safety
/// `buffer` must be a valid pointer previously returned by `MojoMapBuffer`,
/// and it must not have been unmapped already.
///
/// # Possible Error Codes:
/// - `InvalidArgument`: if `buffer` is invalid (e.g., is not the result of
///   `MojoMapBuffer` or has already been unmapped).
pub unsafe fn MojoUnmapBuffer(buffer: *mut u8) -> MojoResult<()> {
    // SAFETY: The caller has guaranteed that `buffer` is a valid pointer
    // returned by `MojoMapBuffer`.
    MojoError::result_from_code(unsafe { raw_ffi::MojoUnmapBuffer(buffer as *mut c_void) })
}
