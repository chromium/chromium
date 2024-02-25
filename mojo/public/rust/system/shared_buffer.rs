// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::ptr;
use std::slice;

use crate::ffi;
// This full import is intentional; nearly every type in mojo_types needs to be
// used.
use crate::handle;
use crate::handle::{CastHandle, Handle};
use crate::mojo_types::*;

use super::UntypedHandle;

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct DuplicateFlags: u32 {
        /// The resulting duplicate shared buffer handle will map to a read only
        /// memory region. If a buffer is ever duplicated without this flag, no
        /// further duplicate calls can use this flag. Likewise, if a buffer is
        /// duplicated with this flag, all further duplicates must be read-only.
        const READ_ONLY = 1;
    }
}

/// A MappedBuffer represents the result of
/// calling map_buffer on a shared buffer handle.
///
/// The C API allocates a buffer which can then be
/// read or written to through this data structure.
///
/// The importance of this data structure is that
/// we bind the lifetime of the slice given to us
/// from the C API to this structure. Additionally,
/// reads and writes to this data structure are guaranteed
/// to be able to propagate through Mojo as they are
/// volatile. Language optimization backends are generally
/// unaware of other address spaces, and since this structure
/// represents some buffer from another address space, we
/// need to make sure loads and stores are volatile.
///
/// Changes to this data structure are propagated through Mojo
/// on the next Mojo operation (that is, Mojo operations are
/// considered barriers). So, unmapping the buffer, sending a
/// message across a pipe, duplicating a shared buffer handle,
/// etc. are all valid ways of propagating changes. The read
/// and write methods do NOT guarantee changes to propagate.
///
/// This structure also prevents resource leaks by
/// unmapping the buffer it contains on destruction.
pub struct MappedBuffer<'a> {
    buffer: &'a mut [u8],
}

impl<'a> MappedBuffer<'a> {
    /// Returns the length of the wrapped buffer.
    ///
    /// Part of reimplementing the array interface to be
    /// able to use the structure naturally.
    pub fn len(&self) -> usize {
        self.buffer.len()
    }

    /// Read safely from the shared buffer. Makes sure a real load
    /// is performed by marking the read as volatile.
    pub fn read(&self, index: usize) -> u8 {
        unsafe { ptr::read_volatile((&self.buffer[index]) as *const u8) }
    }

    /// Write safely to the shared buffer. Makes sure a real store
    /// is performed by marking the store as volatile.
    pub fn write(&mut self, index: usize, value: u8) {
        unsafe {
            ptr::write_volatile((&mut self.buffer[index]) as *mut u8, value);
        }
    }

    /// Returns the slice this buffer wraps.
    ///
    /// The reason this method is unsafe is because the way Rust maps
    /// reads and writes down to loads and stores may not be to real
    /// loads and stores which are required to allow changes to propagate
    /// through Mojo. If you are not careful, some writes and reads may be
    /// to incorrect data! Use at your own risk.
    pub unsafe fn as_slice(&'a mut self) -> &'a mut [u8] {
        self.buffer
    }
}

impl<'a> Drop for MappedBuffer<'a> {
    /// The destructor for MappedBuffer. Unmaps the buffer it
    /// encloses by using the original, raw pointer to the mapped
    /// memory region.
    fn drop(&mut self) {
        let r = MojoResult::from_code(unsafe {
            ffi::MojoUnmapBuffer(self.buffer.as_mut_ptr() as *mut ffi::c_void)
        });
        assert_eq!(r, MojoResult::Okay, "failed to unmap buffer");
    }
}

/// Represents a handle to a shared buffer in Mojo.
/// This data structure wraps a handle and acts
/// effectively as a typed handle.
pub struct SharedBuffer {
    handle: handle::UntypedHandle,
}

impl SharedBuffer {
    /// Creates a shared buffer in Mojo and returns a SharedBuffer
    /// structure which represents a handle to the shared buffer.
    pub fn new(num_bytes: u64) -> Result<SharedBuffer, MojoResult> {
        let opts = ffi::MojoCreateSharedBufferOptions::new(0);
        let mut handle = UntypedHandle::invalid();
        match MojoResult::from_code(unsafe {
            ffi::MojoCreateSharedBuffer(num_bytes, opts.inner_ptr(), handle.as_mut_ptr())
        }) {
            MojoResult::Okay => Ok(SharedBuffer { handle }),
            e => Err(e),
        }
    }

    /// Duplicates the shared buffer handle. This is NOT the same
    /// as cloning the structure which is illegal since cloning could
    /// lead to resource leaks. Instead this uses Mojo to duplicate the
    /// buffer handle (though the handle itself may not be represented by
    /// the same number) that maps to the same shared buffer as the original.
    pub fn duplicate(&self, flags: DuplicateFlags) -> Result<SharedBuffer, MojoResult> {
        let opts = ffi::MojoDuplicateBufferHandleOptions::new(flags.bits());
        let mut dup_h = UntypedHandle::invalid();
        match MojoResult::from_code(unsafe {
            ffi::MojoDuplicateBufferHandle(
                self.handle.get_native_handle(),
                opts.inner_ptr(),
                dup_h.as_mut_ptr(),
            )
        }) {
            MojoResult::Okay => Ok(SharedBuffer { handle: dup_h }),
            e => Err(e),
        }
    }

    /// Map the shared buffer into local memory. Generates a MappedBuffer
    /// structure. See MappedBuffer for more information on how to use it.
    pub fn map<'a>(&self, offset: u64, num_bytes: u64) -> Result<MappedBuffer<'a>, MojoResult> {
        let options = ffi::MojoMapBufferOptions::new(0);
        let mut ptr: *mut ffi::c_void = ptr::null_mut();
        match unsafe {
            MojoResult::from_code(ffi::MojoMapBuffer(
                self.handle.get_native_handle(),
                offset,
                num_bytes,
                options.inner_ptr(),
                &mut ptr,
            ))
        } {
            MojoResult::Okay => {
                let buffer =
                    unsafe { slice::from_raw_parts_mut(ptr as *mut u8, num_bytes as usize) };
                Ok(MappedBuffer { buffer })
            }
            e => Err(e),
        }
    }

    /// Retrieves information about this shared buffer. The return value is just
    /// the size of the shared buffer.
    pub fn get_info(&self) -> Result<u64, MojoResult> {
        let mut info = ffi::MojoSharedBufferInfo::new(0);
        let r = MojoResult::from_code(unsafe {
            ffi::MojoGetBufferInfo(
                self.handle.get_native_handle(),
                ptr::null(),
                info.inner_mut_ptr(),
            )
        });
        if r != MojoResult::Okay { Err(r) } else { Ok(info.size) }
    }
}

impl CastHandle for SharedBuffer {
    /// Generates a SharedBuffer from an untyped handle wrapper
    /// See crate::handle for information on untyped vs. typed
    unsafe fn from_untyped(handle: handle::UntypedHandle) -> Self {
        SharedBuffer { handle: handle }
    }

    /// Consumes this object and produces a plain handle wrapper
    /// See crate::handle for information on untyped vs. typed
    fn as_untyped(self) -> handle::UntypedHandle {
        self.handle
    }
}

impl Handle for SharedBuffer {
    /// Returns the native handle wrapped by this structure.
    ///
    /// See crate::handle for information on handle wrappers
    fn get_native_handle(&self) -> MojoHandle {
        self.handle.get_native_handle()
    }
}
