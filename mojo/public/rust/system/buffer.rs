// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::mojo_types::declare_typed_handle;
use core::slice;
use std::ptr::NonNull;

chromium::import! {
    "//mojo/public/rust/c_mojo_api" as mojo_ffi;
}

use mojo_ffi::MojoResult;

declare_typed_handle!(SharedBuffer);

/// A shared buffer that can be mapped into the memory of multiple processes.
///
/// `SharedBuffer` handles are used to share large amounts of data across
/// process boundaries without copying. The typical model for sharing memory is:
/// 1. One process creates a `SharedBuffer` using `create()`.
/// 2. The process calls `clone_handle()` or `clone_read_only()` to obtain a new
///    handle to the same buffer.
/// 3. The duplicated handle is sent to another process over a Mojo message
///    pipe.
/// 4. Both processes call `map()` on their respective handles to get a
///    `MappedBuffer`, which provides direct memory access, but does not handle
///    aliasing, so care must be taken when accessing the same memory from
///    multiple processes.
///
/// A buffer remains valid as long as at least one handle referencing it exists.
impl SharedBuffer {
    /// Creates a buffer of `num_bytes` bytes that can be shared between
    /// processes.
    ///
    /// # Possible Error Codes:
    /// - `ResourceExhausted`: if the system is out of memory.
    /// - `InvalidArgument`: if `num_bytes` is 0.
    pub fn create(num_bytes: u64) -> MojoResult<SharedBuffer> {
        mojo_ffi::buffer::MojoCreateSharedBuffer(num_bytes).map(|h| h.into())
    }

    /// Create a new handle pointing to the same shared buffer as `self`.
    /// The new handle will have full read/write permissions.
    ///
    /// # Warning
    /// Duplicating a handle permanently restricts future duplications of the
    /// *entire underlying shared buffer*. If you call this method, all future
    /// calls to `clone_read_only()` on this handle, or on **any other handle**
    /// pointing to the same buffer, will fail with `FailedPrecondition`.
    ///
    /// # Possible Error Codes:
    /// - `FailedPrecondition`: if this handle (or any other handle to the same
    ///   buffer) was previously duplicated as read-only via
    ///   `clone_read_only()`.
    pub fn clone_handle(&self) -> MojoResult<SharedBuffer> {
        mojo_ffi::buffer::MojoDuplicateBufferHandle(&self.handle, false).map(|h| h.into())
    }

    /// Create a new read-only handle pointing to the same shared buffer as
    /// `self`.
    ///
    /// # Warning
    /// Duplicating a handle permanently restricts future duplications of the
    /// *entire underlying shared buffer*. If you call this method, all future
    /// calls to `clone_handle()` on this handle, or on **any other handle**
    /// pointing to the same buffer, will fail with `FailedPrecondition`.
    ///
    /// # Possible Error Codes:
    /// - `FailedPrecondition`: if this handle (or any other handle to the same
    ///   buffer) was previously duplicated as read-write via `clone_handle()`.
    pub fn clone_read_only(&self) -> MojoResult<SharedBuffer> {
        mojo_ffi::buffer::MojoDuplicateBufferHandle(&self.handle, true).map(|h| h.into())
    }

    /// Retrieves the size, in bytes, of the shared buffer.
    pub fn size(&self) -> u64 {
        // This cannot fail because `self.handle` is guaranteed to be a valid
        // `SharedBuffer` handle by the type system.
        mojo_ffi::buffer::MojoGetBufferInfo(&self.handle).unwrap()
    }

    /// Maps the part (at offset `offset` of length `num_bytes`) of the buffer
    /// into memory.
    ///
    /// # Possible Error Codes:
    /// - `InvalidArgument`: if `num_bytes` is 0, or `offset` is not a multiple
    ///   of the system page size.
    /// - `ResourceExhausted`: if the mapping operation itself failed.
    pub fn map(&self, offset: u64, num_bytes: usize) -> MojoResult<MappedBuffer> {
        let ptr = mojo_ffi::buffer::MojoMapBuffer(&self.handle, offset, num_bytes)?;
        // MappedBuffer takes ownership of this pointer and will unmap it on Drop.
        // MojoMapBuffer returns a pointer valid for `num_bytes`.
        Ok(MappedBuffer { ptr: NonNull::new(ptr).unwrap(), len: num_bytes })
    }
}

/// A wrapper around a mapped region of a `SharedBuffer`.
///
/// This provides direct memory access to the contents of a shared buffer.
/// Mapped memory is unmapped automatically when this struct is dropped.
///
/// Since the memory may be concurrently modified by other processes, callers
/// are responsible for coordinating access and for soundly accessing the data
/// via `unsafe` blocks.
pub struct MappedBuffer {
    ptr: NonNull<u8>,
    len: usize,
}

impl MappedBuffer {
    /// Returns a read-only slice covering the mapped memory for access.
    ///
    /// # Safety
    /// The caller must ensure that the memory is not freed or mutated,
    /// and that no mutable references to this memory exist,
    /// while the slice is alive (including by another process or thread).
    pub unsafe fn as_slice(&self) -> &[u8] {
        // SAFETY: The memory is valid for `self.len` bytes and we hold a reference to
        // `self` ensuring it won't be unmapped while the slice is alive. The
        // caller guarantees no mutable aliasing.
        unsafe { slice::from_raw_parts(self.ptr.as_ptr(), self.len) }
    }

    /// Returns a mutable slice covering the mapped memory for access.
    ///
    /// This will not allow you to write to memory when the read-only handle was
    /// used to clone.
    ///
    /// # Safety
    /// The caller must ensure that the memory is not read or mutated,
    /// and that no other references (mutable or immutable) to this memory
    /// exist, while the slice is alive (including by another process or
    /// thread).
    pub unsafe fn as_mut_slice(&mut self) -> &mut [u8] {
        // SAFETY: The memory is valid for `self.len` bytes and we hold a mutable
        // reference to `self` ensuring exclusive access locally, and that it
        // won't be unmapped while the slice is alive. The caller guarantees no
        // aliasing at all from other processes.
        unsafe { slice::from_raw_parts_mut(self.ptr.as_ptr(), self.len) }
    }
}

/// We need to explicitly release the mapped memory when the `MappedBuffer` is
/// dropped.
impl Drop for MappedBuffer {
    fn drop(&mut self) {
        // SAFETY: The pointer was returned by MojoMapBuffer. Because `MappedBuffer`
        // owns the pointer and it is only unmapped here in Drop, we know it hasn't
        // been unmapped before and is valid to unmap.
        let result = unsafe { mojo_ffi::buffer::MojoUnmapBuffer(self.ptr.as_ptr()) };

        // The only way this fails is if the pointer is invalid, which means
        // our struct is broken.
        debug_assert!(result.is_ok());
    }
}

// SAFETY: All methods that access the underlying memory are `unsafe` and
// require the caller to assume responsibility for thread-safety and
// cross-process synchronization.
unsafe impl Send for MappedBuffer {}
// SAFETY: See `Send`.
unsafe impl Sync for MappedBuffer {}
