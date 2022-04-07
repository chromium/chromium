// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::marker;
use std::mem;
use std::ops;
use std::ptr;
use std::slice;
use std::vec;

use crate::system::ffi;
// This full import is intentional; nearly every type in mojo_types needs to be used.
use crate::system::handle;
use crate::system::handle::{CastHandle, Handle};
use crate::system::mojo_types::*;

#[repr(u32)]
/// Create flags for data pipes
pub enum Create {
    None = 0,
}

#[repr(u32)]
/// Write flags for data pipes
pub enum Write {
    None = 0,

    /// Write all the data to the pipe if possible or none at all
    AllOrNone = 1 << 0,
}

#[repr(u32)]
/// Read flags for data pipes
pub enum Read {
    None = 0,

    /// Read all the data from the pipe if possible, or none at all
    AllOrNone = 1 << 0,

    /// Dequeue the message recieved rather than reading it
    Discard = 1 << 1,

    /// Get information about the queue on the pipe but do not perform the
    /// read
    Query = 1 << 2,

    /// Read data off the pipe's queue but do not dequeue it
    Peek = 1 << 3,
}

/// Intermediary structure in a two-phase read.
/// Reads of the requested buffer must be done directly
/// through this data structure which must then be committed.
pub struct ReadDataBuffer<'b, 'p, T>
where
    'p: 'b,
    T: 'p,
{
    buffer: &'b [T],

    /// Contains a reference to parent to end commit
    /// and prevent it from outliving its parent handle.
    parent: &'p Consumer<T>,
}

impl<'b, 'p, T> ReadDataBuffer<'b, 'p, T>
where
    'p: 'b,
    T: 'p,
{
    /// Attempts to commit the read, that is, end the two-phase read
    /// started by the parent Consumer<T> object. On a successful
    /// commit, consumes self, otherwise returns self to try again.
    pub fn commit(self, bytes_read: usize) -> Option<(Self, MojoResult)> {
        let result = unsafe { self.parent.end_read(bytes_read) };
        if result == MojoResult::Okay { None } else { Some((self, result)) }
    }

    /// Returns the length of the underlying buffer
    pub fn len(&self) -> usize {
        self.buffer.len()
    }
}

impl<'b, 'p, T> ops::Index<usize> for ReadDataBuffer<'b, 'p, T>
where
    'p: 'b,
    T: 'p,
{
    type Output = T;

    /// Overloads the indexing ([]) operator for reads.
    ///
    /// Part of reimplementing the array interface to be
    /// able to use the structure naturally.
    fn index(&self, index: usize) -> &T {
        &self.buffer[index]
    }
}

/// Intermediary structure in a two-phase write.
/// Writes to the requested buffer must be done directly
/// through this data structure which must then be committed.
pub struct WriteDataBuffer<'b, 'p, T>
where
    'p: 'b,
    T: 'p,
{
    buffer: &'b mut [T],

    /// Contains a reference to parent to end commit
    /// and prevent it from outliving its parent handle.
    parent: &'p Producer<T>,
}

impl<'b, 'p, T> WriteDataBuffer<'b, 'p, T>
where
    'p: 'b,
    T: 'p,
{
    /// Attempts to commit the write, that is, end the two-phase
    /// write started by a Producer. On a successful
    /// commit, consumes self, otherwise returns self to try again.
    pub fn commit(self, bytes_written: usize) -> Option<(Self, MojoResult)> {
        let result = unsafe { self.parent.end_write(bytes_written) };
        if result == MojoResult::Okay { None } else { Some((self, result)) }
    }

    /// Returns the length of the underlying buffer
    pub fn len(&self) -> usize {
        self.buffer.len()
    }
}

impl<'b, 'p, T> ops::Index<usize> for WriteDataBuffer<'b, 'p, T>
where
    'p: 'b,
    T: 'p,
{
    type Output = T;

    /// Overloads the indexing ([]) operator for reads.
    ///
    /// Part of reimplementing the array interface to be
    /// able to use the structure naturally.
    fn index(&self, index: usize) -> &T {
        &self.buffer[index]
    }
}

impl<'b, 'p, T> ops::IndexMut<usize> for WriteDataBuffer<'b, 'p, T>
where
    'p: 'b,
    T: 'p,
{
    /// Overloads the indexing ([]) operator for writes.
    ///
    /// Part of reimplementing the array interface to be
    /// able to use the structure naturally.
    fn index_mut(&mut self, index: usize) -> &mut T {
        &mut self.buffer[index]
    }
}

/// Creates a data pipe, represented as a consumer
/// and a producer. Additionally, we associate a type
/// T with the data pipe, as data pipes operate in terms
/// of elements. In this way we can enforce type safety.
///
/// Capacity, as an input, must be given in number of elements.
/// Use a capacity of 0 in order to use some system-dependent
/// default capacity.
pub fn create<T>(
    flags: CreateFlags,
    capacity: u32,
) -> Result<(Consumer<T>, Producer<T>), MojoResult> {
    let elem_size = mem::size_of::<T>() as u32;
    let opts = ffi::MojoCreateDataPipeOptions::new(flags, elem_size, capacity * elem_size);
    // TODO(mknyszek): Make sure handles are valid
    let mut chandle: MojoHandle = 0;
    let mut phandle: MojoHandle = 0;
    let raw_opts = opts.inner_ptr();
    let r = MojoResult::from_code(unsafe {
        ffi::MojoCreateDataPipe(
            raw_opts,
            &mut phandle as *mut MojoHandle,
            &mut chandle as *mut MojoHandle,
        )
    });
    if r != MojoResult::Okay {
        Err(r)
    } else {
        Ok((
            Consumer::<T> {
                handle: unsafe { handle::acquire(chandle) },
                _elem_type: marker::PhantomData,
            },
            Producer::<T> {
                handle: unsafe { handle::acquire(phandle) },
                _elem_type: marker::PhantomData,
            },
        ))
    }
}

/// Creates a data pipe, represented as a consumer
/// and a producer, using the default Mojo options.
pub fn create_default() -> Result<(Consumer<u8>, Producer<u8>), MojoResult> {
    create::<u8>(Create::None as u32, 0)
}

/// Represents the consumer half of a data pipe.
/// This data structure wraps a handle and acts
/// effectively as a typed handle.
///
/// The purpose of the _elem_type field is to associate
/// a type with the consumer, as a data pipe works
/// in elements.
pub struct Consumer<T> {
    handle: handle::UntypedHandle,
    _elem_type: marker::PhantomData<T>,
}

impl<T> Consumer<T> {
    /// Perform a read operation on the consumer end of the data pipe. As
    /// a result, we get an std::vec::Vec filled with whatever was written.
    pub fn read(&self, flags: ReadFlags) -> Result<vec::Vec<T>, MojoResult> {
        let mut options = ffi::MojoReadDataOptions::new(Read::Query as ReadFlags);
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

        options.flags = flags;
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

    /// Start two-phase read and return a ReadDataBuffer to perform
    /// read and commit.
    pub fn begin(&self) -> Result<ReadDataBuffer<T>, MojoResult> {
        let wrapped_result = unsafe { self.begin_read() };
        match wrapped_result {
            Ok(arr) => Ok(ReadDataBuffer::<T> { buffer: arr, parent: self }),
            Err(r) => Err(r),
        }
    }

    /// A private function that performs the first half of two-phase reading.
    /// Kept private because it is unsafe to use (the array received may not
    /// be valid if end_read is performed).
    unsafe fn begin_read(&self) -> Result<&[T], MojoResult> {
        let mut buf_num_bytes: u32 = 0;
        let mut pbuf: *const ffi::c_void = ptr::null();
        let r = MojoResult::from_code(ffi::MojoBeginReadData(
            self.handle.get_native_handle(),
            ptr::null(),
            &mut pbuf as *mut _,
            &mut buf_num_bytes as *mut u32,
        ));
        if r != MojoResult::Okay {
            Err(r)
        } else {
            let buf_elems = (buf_num_bytes as usize) / mem::size_of::<T>();
            let buf = slice::from_raw_parts(pbuf as *mut T, buf_elems);
            Ok(buf)
        }
    }

    /// A private function that performs the second half of two-phase reading.
    /// Kept private because it is unsafe to use (the array received from start_read
    /// may not be valid if end_read is performed).
    ///
    /// Also assumes loads/stores aren't reordered such that a load/store may be
    /// optimized to be run AFTER MojoEndReadData(). In general, this is true as long
    /// as raw pointers are used, but Rust's memory model is still undefined. If you're
    /// getting a bad/strange runtime error, it might be for this reason.
    unsafe fn end_read(&self, elems_read: usize) -> MojoResult {
        let elem_size = mem::size_of::<T>();
        MojoResult::from_code(ffi::MojoEndReadData(
            self.handle.get_native_handle(),
            (elems_read * elem_size) as u32,
            ptr::null(),
        ))
    }
}

impl<T> CastHandle for Consumer<T> {
    /// Generates a Consumer from an untyped handle wrapper
    /// See mojo::system::handle for information on untyped vs. typed
    unsafe fn from_untyped(handle: handle::UntypedHandle) -> Self {
        Consumer::<T> { handle: handle, _elem_type: marker::PhantomData }
    }

    /// Consumes this object and produces a plain handle wrapper
    /// See mojo::system::handle for information on untyped vs. typed
    fn as_untyped(self) -> handle::UntypedHandle {
        self.handle
    }
}

impl<T> Handle for Consumer<T> {
    /// Returns the native handle wrapped by this structure.
    ///
    /// See mojo::system::handle for information on handle wrappers
    fn get_native_handle(&self) -> MojoHandle {
        self.handle.get_native_handle()
    }
}

/// Represents the consumer half of a data pipe.
/// This data structure wraps a handle and acts
/// effectively as a typed handle.
///
/// The purpose of the _elem_type field is to associate
/// a type with the consumer, as a data pipe works
/// in elements.
pub struct Producer<T> {
    handle: handle::UntypedHandle,
    _elem_type: marker::PhantomData<T>,
}

impl<T> Producer<T> {
    /// Perform a write operation on the producer end of the data pipe.
    /// Returns the number of elements actually written.
    pub fn write(&self, data: &[T], flags: WriteFlags) -> Result<usize, MojoResult> {
        let mut num_bytes = (data.len() * mem::size_of::<T>()) as u32;
        let options = ffi::MojoWriteDataOptions::new(flags);
        let r = MojoResult::from_code(unsafe {
            ffi::MojoWriteData(
                self.handle.get_native_handle(),
                data.as_ptr() as *const ffi::c_void,
                &mut num_bytes as *mut u32,
                options.inner_ptr(),
            )
        });
        if r != MojoResult::Okay { Err(r) } else { Ok(num_bytes as usize) }
    }

    /// Start two-phase write and return a WriteDataBuffer to perform
    /// write and commit.
    ///
    /// Borrows self as mutable so that no other operation may happen on
    /// the producer until the two-phase write is committed.
    pub fn begin(&self) -> Result<WriteDataBuffer<T>, MojoResult> {
        let wrapped_result = unsafe { self.begin_write() };
        match wrapped_result {
            Ok(arr) => Ok(WriteDataBuffer::<T> { buffer: arr, parent: self }),
            Err(r) => Err(r),
        }
    }

    /// A private function that performs the first half of two-phase writing.
    /// Kept private because it is unsafe to use (the array received may not
    /// be valid if end_write is performed).
    unsafe fn begin_write(&self) -> Result<&mut [T], MojoResult> {
        let mut buf_num_bytes: u32 = 0;
        let mut pbuf: *mut ffi::c_void = ptr::null_mut();
        let r = MojoResult::from_code(ffi::MojoBeginWriteData(
            self.handle.get_native_handle(),
            ptr::null(),
            &mut pbuf,
            &mut buf_num_bytes as *mut u32,
        ));
        if r != MojoResult::Okay {
            Err(r)
        } else {
            let buf_elems = (buf_num_bytes as usize) / mem::size_of::<T>();
            let buf = slice::from_raw_parts_mut(pbuf as *mut T, buf_elems);
            Ok(buf)
        }
    }

    /// A private function that performs the second half of two-phase writing.
    /// Kept private because it is unsafe to use (the array received from start_write
    /// may not be valid if end_write is performed).
    ///
    /// Also assumes loads/stores aren't reordered such that a load/store may be
    /// optimized to be run AFTER MojoEndWriteData(). In general, this is true as long
    /// as raw pointers are used, but Rust's memory model is still undefined. If you're
    /// getting a bad/strange runtime error, it might be for this reason.
    unsafe fn end_write(&self, elems_written: usize) -> MojoResult {
        let elem_size = mem::size_of::<T>();
        MojoResult::from_code(ffi::MojoEndWriteData(
            self.handle.get_native_handle(),
            (elems_written * elem_size) as u32,
            ptr::null(),
        ))
    }
}

impl<T> CastHandle for Producer<T> {
    /// Generates a Consumer from an untyped handle wrapper
    /// See mojo::system::handle for information on untyped vs. typed
    unsafe fn from_untyped(handle: handle::UntypedHandle) -> Self {
        Producer::<T> { handle: handle, _elem_type: marker::PhantomData }
    }

    /// Consumes this object and produces a plain handle wrapper
    /// See mojo::system::handle for information on untyped vs. typed
    fn as_untyped(self) -> handle::UntypedHandle {
        self.handle
    }
}

impl<T> Handle for Producer<T> {
    /// Returns the native handle wrapped by this structure.
    ///
    /// See mojo::system::handle for information on handle wrappers
    fn get_native_handle(&self) -> MojoHandle {
        self.handle.get_native_handle()
    }
}
