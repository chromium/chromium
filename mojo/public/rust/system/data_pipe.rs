// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::marker;
use std::mem;
use std::ops;
use std::ptr;
use std::slice;
use std::vec;

use crate::ffi;
use crate::handle::{self, CastHandle, Handle, UntypedHandle};
use crate::mojo_types::*;

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct WriteFlags: u32 {
        /// Write all the data to the pipe if possible or none at all.
        const ALL_OR_NONE = 1 << 0;
    }
}

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct ReadFlags: u32 {
        /// Write all the data to the pipe if possible or none at all.
        const ALL_OR_NONE = 1 << 0;

        /// Dequeue the message recieved rather than reading it.
        const DISCARD = 1 << 1;

        /// Get information about the queue on the pipe but do not perform the
        /// read.
        const QUERY = 1 << 2;

        /// Read data off the pipe's queue but do not dequeue it.
        const PEEK = 1 << 3;
    }
}

/// Exposes available data in a two-phase read on a `Consumer`. This structure
/// derefs to a `[T]` so all slice methods work. When done reading, `commit`
/// should be called with the number of elements actually read. The remaining
/// elements will be returned in a future read operation.
///
/// If dropped without `commit`ing, the read will be implicitly committed as if
/// zero elements were read.
pub struct ReadDataBuffer<'b, 'p, T> {
    buffer: &'b [T],

    /// Parent pipe endpoint to commit read. Implicitly requires `parent` to
    /// outlive `self`.
    parent: &'p Consumer<T>,
}

impl<'b, 'p, T> ReadDataBuffer<'b, 'p, T> {
    /// Commit the two-phase read. Consumes `self`. `elems_read` indicates the
    /// number of T consumed. Future reads will not yield the consumed elements.
    pub fn commit(self, elems_read: usize) {
        // SAFETY: commit_impl is only called from here and in drop. We take
        // self by value so we can only be called once. drop by definition can
        // only be called once, and we can be sure drop was not called yet, so
        // we know commit_impl has not been called. Further, by calling
        // mem::forget(self) after commit_impl we ensure it will not be called
        // again.
        unsafe {
            self.commit_impl(elems_read);
        }

        // Ensure drop() is not called since we also call commit_impl() there.
        // No more cleanup is necessary since we only hold shared references.
        mem::forget(self);
    }

    // Safety: may only be called once for `self`.
    unsafe fn commit_impl(&self, elems_read: usize) {
        assert!(elems_read <= self.buffer.len(), "{} > {}", elems_read, self.buffer.len());
        let elem_size = mem::size_of::<T>();
        let result = MojoResult::from_code(unsafe {
            ffi::MojoEndReadData(
                self.parent.handle.get_native_handle(),
                (elems_read * elem_size) as u32,
                ptr::null(),
            )
        });

        if result != MojoResult::Okay {
            // The Mojo function can return two possible errors:
            // * MojoResult::InvalidArgument indicating either the handle is invalid or
            //   elems_read is larger than self.buffer.
            // * MojoResult::FailedPrecondition if not in a two-phase read.
            //
            // Either indicates a bug in the wrapper code and other errors are
            // undocumented in Mojo, so panic.
            unreachable!("unexpected Mojo error {:?}", result);
        }
    }
}

impl<'b, 'p, T> Drop for ReadDataBuffer<'b, 'p, T> {
    fn drop(&mut self) {
        // SAFETY: commit cannot have been called since it takes self by value
        // and calls mem::forget(self).
        unsafe {
            self.commit_impl(0);
        }
    }
}

impl<'b, 'p, T> ops::Deref for ReadDataBuffer<'b, 'p, T> {
    type Target = [T];
    fn deref(&self) -> &Self::Target {
        self.buffer
    }
}

/// Exposes an output buffer in a two-phase write on a `Producer`. This
/// structure derefs to a `[mem::MaybeUninit<T>]` so all slice methods work.
/// When done writing, `commit` should be called with the number of elements
/// actually written.
///
/// The buffer starts out uninitialized. It is up to the user to ensure the
/// write is committed safely without referring to uninitialized data.
///
/// If dropped without `commit`ing, the write will be implicitly committed as if
/// zero elements were written.
pub struct WriteDataBuffer<'b, 'p, T> {
    buffer: &'b mut [mem::MaybeUninit<T>],

    /// Parent pipe endpoint to commit write. Implicitly requires `parent` to
    /// outlive `self`.
    parent: &'p Producer<T>,
}

impl<'b, 'p, T> WriteDataBuffer<'b, 'p, T> {
    /// Commit the two-phase write. Consumes `self`. `elems_written` indicates
    /// the number of T sent.
    ///
    /// # Safety
    ///
    /// The inner buffer starts with uninitialized values. The caller must
    /// ensure that the first `elems_written` values have been initialized.
    pub unsafe fn commit(self, elems_written: usize) {
        // SAFETY: commit_impl is only called from here and in drop. We take
        // self by value so we can only be called once. drop by definition can
        // only be called once, and we can be sure drop was not called yet, so
        // we know commit_impl has not been called. Further, by calling
        // mem::forget(self) after commit_impl we ensure it will not be called
        // again.
        unsafe {
            self.commit_impl(elems_written);
        }

        // Ensure drop() is not called since we also call commit_impl() there.
        // No more cleanup is necessary since we only hold shared references.
        mem::forget(self);
    }

    // Safety: may only be called once for `self`, and first `elems_written`
    // values in `self.buffer` must be initialized.
    unsafe fn commit_impl(&self, elems_written: usize) {
        assert!(elems_written <= self.buffer.len(), "{} > {}", elems_written, self.buffer.len());
        let elem_size = mem::size_of::<T>();
        let result = MojoResult::from_code(unsafe {
            ffi::MojoEndWriteData(
                self.parent.handle.get_native_handle(),
                (elems_written * elem_size) as u32,
                ptr::null(),
            )
        });
        if result != MojoResult::Okay {
            // The Mojo function can return two possible errors:
            // * MojoResult::InvalidArgument indicating either the handle is invalid or
            //   elems_written is larger than self.buffer.
            // * MojoResult::FailedPrecondition if not in a two-phase read.
            //
            // Either indicates a bug in the wrapper code and other errors are
            // undocumented in Mojo, so panic.
            unreachable!("unexpected Mojo error {:?}", result);
        }
    }
}

impl<'b, 'p, T> Drop for WriteDataBuffer<'b, 'p, T> {
    fn drop(&mut self) {
        // SAFETY: commit cannot have been called since it takes self by value
        // and calls mem::forget(self). `elems_written` is zero so no elems must
        // be initialized.
        unsafe {
            self.commit_impl(0);
        }
    }
}

impl<'b, 'p, T> ops::Deref for WriteDataBuffer<'b, 'p, T> {
    type Target = [mem::MaybeUninit<T>];
    fn deref(&self) -> &Self::Target {
        self.buffer
    }
}

impl<'b, 'p, T> ops::DerefMut for WriteDataBuffer<'b, 'p, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.buffer
    }
}

/// Creates a data pipe represented as a consumer and a producer. The producer
/// and consumer read and write elements of type `T`.
///
/// `capacity` is the minimum number of elements that can be queued. If creation
/// is successful it is guaranteed the user can write at least this many
/// elements before they are read. If `capacity` is 0 it is chosen by the system
/// (but is at least 1).
///
/// Values written to a `Producer` will not have drop() called unless
/// successfully received by a `Consumer` in Rust and the consuming code drops
/// it.
///
/// # Safety
///
/// Elements of type `T` are sent as bitwise moves across the pipe. The caller
/// must ensure `T` is:
/// * `Send`, if both ends of the pipe reside in the same process, or
/// * "Plain-old data" if the pipe endpoints reside in different processes.
///
/// There is no trait to represent the latter. Even if `T: Copy` we cannot
/// assume it's safe since T may contain immutable references or pointers.
///
/// Any other use is extremely unsafe since arbitrary `T` could contain
/// pointers, handles, and anything else that is only valid within a process.
pub unsafe fn create<T>(capacity: u32) -> Result<(Consumer<T>, Producer<T>), MojoResult> {
    let elem_size = mem::size_of::<T>() as u32;
    let opts = ffi::MojoCreateDataPipeOptions::new(0, elem_size, capacity * elem_size);
    // TODO(mknyszek): Make sure handles are valid
    let mut chandle = UntypedHandle::invalid();
    let mut phandle = UntypedHandle::invalid();
    match MojoResult::from_code(unsafe {
        ffi::MojoCreateDataPipe(opts.inner_ptr(), phandle.as_mut_ptr(), chandle.as_mut_ptr())
    }) {
        MojoResult::Okay => Ok((
            Consumer::<T> { handle: chandle, _elem_type: marker::PhantomData },
            Producer::<T> { handle: phandle, _elem_type: marker::PhantomData },
        )),
        e => Err(e),
    }
}

/// Creates a data pipe, represented as a consumer and a producer, using the
/// default Mojo options. This is safe since `u8` can be sent between processes.
pub fn create_default() -> Result<(Consumer<u8>, Producer<u8>), MojoResult> {
    unsafe { create(0) }
}

/// Consumer half of a Mojo data pipe.
pub struct Consumer<T> {
    handle: UntypedHandle,
    _elem_type: marker::PhantomData<T>,
}

impl<T> Consumer<T> {
    /// Perform a read operation. Returns a Vec<T> containing the values read.
    /// Fails with `Err(MojoResult::Busy)` if a two-phase read is in progress.
    pub fn read(&self, flags: ReadFlags) -> Result<Vec<T>, MojoResult> {
        let mut options = ffi::MojoReadDataOptions::new(ReadFlags::QUERY.bits());
        let mut num_bytes: u32 = 0;
        let r_prelim = unsafe {
            ffi::MojoReadData(
                self.handle.get_native_handle(),
                options.inner_ptr(),
                ptr::null_mut() as *mut ffi::c_void,
                &mut num_bytes as *mut u32,
            )
        };
        if r_prelim != 0 || num_bytes == 0 {
            return Err(MojoResult::from_code(r_prelim));
        }

        options.flags = flags.bits();
        let elem_size: u32 = mem::size_of::<T>() as u32;
        // TODO(mknyszek): make sure elem_size divides into num_bytes
        let mut buf: vec::Vec<T> = vec::Vec::with_capacity((num_bytes / elem_size) as usize);
        let r = MojoResult::from_code(unsafe {
            ffi::MojoReadData(
                self.handle.get_native_handle(),
                options.inner_ptr(),
                buf.as_mut_ptr() as *mut ffi::c_void,
                &mut num_bytes as *mut u32,
            )
        });
        unsafe { buf.set_len((num_bytes / elem_size) as usize) }
        if r != MojoResult::Okay { Err(r) } else { Ok(buf) }
    }

    /// Begin two-phase read. Returns a ReadDataBuffer to perform read and
    /// commit.
    pub fn begin(&self) -> Result<ReadDataBuffer<T>, MojoResult> {
        let mut buffer_num_bytes: u32 = 0;
        let mut buffer_ptr: *const ffi::c_void = ptr::null();
        let r = MojoResult::from_code(unsafe {
            ffi::MojoBeginReadData(
                self.handle.get_native_handle(),
                ptr::null(),
                &mut buffer_ptr as *mut _,
                &mut buffer_num_bytes as *mut u32,
            )
        });
        if r != MojoResult::Okay {
            return Err(r);
        }

        let buffer_ptr = buffer_ptr as *const T;
        let buffer_len = (buffer_num_bytes as usize) / mem::size_of::<T>();
        let buffer = unsafe { slice::from_raw_parts(buffer_ptr, buffer_len) };
        Ok(ReadDataBuffer { buffer, parent: self })
    }
}

impl<T> CastHandle for Consumer<T> {
    /// Generates a Consumer from an untyped handle wrapper
    /// See crate::handle for information on untyped vs. typed
    unsafe fn from_untyped(handle: handle::UntypedHandle) -> Self {
        Consumer::<T> { handle: handle, _elem_type: marker::PhantomData }
    }

    /// Consumes this object and produces a plain handle wrapper
    /// See crate::handle for information on untyped vs. typed
    fn as_untyped(self) -> handle::UntypedHandle {
        self.handle
    }
}

impl<T> Handle for Consumer<T> {
    /// Returns the native handle wrapped by this structure.
    ///
    /// See crate::handle for information on handle wrappers
    fn get_native_handle(&self) -> MojoHandle {
        self.handle.get_native_handle()
    }
}

/// Producer half of a Mojo data pipe.
pub struct Producer<T> {
    handle: UntypedHandle,
    _elem_type: marker::PhantomData<T>,
}

impl<T> Producer<T> {
    /// Perform a write operation on the producer end of the data pipe. Returns
    /// the number of elements actually written. Fails with
    /// `Err(MojoResult::Busy)` if a two-phase write is in progress.
    pub fn write(&self, data: &[T], flags: WriteFlags) -> Result<usize, MojoResult> {
        let mut num_bytes = (data.len() * mem::size_of::<T>()) as u32;
        let options = ffi::MojoWriteDataOptions::new(flags.bits());
        match MojoResult::from_code(unsafe {
            ffi::MojoWriteData(
                self.handle.get_native_handle(),
                data.as_ptr() as *const ffi::c_void,
                &mut num_bytes as *mut u32,
                options.inner_ptr(),
            )
        }) {
            MojoResult::Okay => Ok(num_bytes as usize),
            e => Err(e),
        }
    }

    /// Begin two-phase write. Returns a WriteDataBuffer to perform write and
    /// commit.
    pub fn begin(&self) -> Result<WriteDataBuffer<T>, MojoResult> {
        let mut buffer_num_bytes: u32 = 0;
        let mut buffer_ptr: *mut ffi::c_void = ptr::null_mut();
        let r = MojoResult::from_code(unsafe {
            ffi::MojoBeginWriteData(
                self.handle.get_native_handle(),
                ptr::null(),
                &mut buffer_ptr,
                &mut buffer_num_bytes as *mut u32,
            )
        });
        if r != MojoResult::Okay {
            return Err(r);
        }

        let buffer_ptr = buffer_ptr as *mut mem::MaybeUninit<T>;
        let buffer_len = (buffer_num_bytes as usize) / mem::size_of::<T>();
        let buffer: &mut [mem::MaybeUninit<T>] =
            unsafe { slice::from_raw_parts_mut(buffer_ptr, buffer_len) };

        Ok(WriteDataBuffer { buffer: buffer, parent: self })
    }
}

impl<T> CastHandle for Producer<T> {
    /// Generates a Consumer from an untyped handle wrapper
    /// See crate::handle for information on untyped vs. typed
    unsafe fn from_untyped(handle: handle::UntypedHandle) -> Self {
        Producer::<T> { handle: handle, _elem_type: marker::PhantomData }
    }

    /// Consumes this object and produces a plain handle wrapper
    /// See crate::handle for information on untyped vs. typed
    fn as_untyped(self) -> handle::UntypedHandle {
        self.handle
    }
}

impl<T> Handle for Producer<T> {
    /// Returns the native handle wrapped by this structure.
    ///
    /// See crate::handle for information on handle wrappers
    fn get_native_handle(&self) -> MojoHandle {
        self.handle.get_native_handle()
    }
}
