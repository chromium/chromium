//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::mojo_types::{Handle, MojoResult, UntypedHandle};
use std::ffi::c_void;
use std::ptr;

chromium::import! {
  pub "//mojo/public/rust:mojo_ffi";
}

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct ReadFlags: u32 {
        /// Read all the data from the pipe if possible, or none at all.
        ///
        /// The flags DISCARD, QUERY, and PEEK are mutually exclusive.
        /// Attempting to use more than one of these in a call will result
        /// in MojoResult::InvalidArgument.
        ///
        /// FOR_RELEASE: Would the API be nicer if we simply had `read`,
        /// `discard`, `query`, and `peek` as distinction functions, and didn't
        /// expose flags to the user at all? Or should that be a higher-level
        /// API?
        ///
        /// FOR_RELEASE(https://crbug.com/458796903): It'd be nicer to access
        /// MOJO_READ_DATA_FLAG_ALL_OR_NONE here, but the bindings we
        /// have right now don't export that. We should change that.
        const ALL_OR_NONE = 1 << 0;

        /// Dequeue the message received rather than reading it.
        const DISCARD = 1 << 1;

        /// Get information about the queue on the pipe but do not perform the
        /// read.
        const QUERY = 1 << 2;

        /// Read data off the pipe's queue but do not dequeue it.
        const PEEK = 1 << 3;
    }
}

bitflags::bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct WriteFlags: u32 {
        /// Write all the data to the pipe if possible or none at all.
        /// FOR_RELEASE(https://crbug.com/458796903): It'd be nicer to access
        /// MOJO_READ_DATA_FLAG_ALL_OR_NONE here, but the bindings we
        /// have right now don't export that. We should change that.
        const ALL_OR_NONE = 1 << 0;
    }
}

pub struct DataPipeConsumerHandle {
    handle: UntypedHandle,
}

impl DataPipeConsumerHandle {
    /// Perform a read operation. Returns the number of bytes read.
    ///
    /// FOR_RELEASE: Implement `read` and `read_exact` from the Read trait.
    pub fn read_with_flags(&self, buf: &mut [u8], flags: ReadFlags) -> Result<usize, MojoResult> {
        // Query the queue, but don't actually perform the read.
        let mut options = mojo_ffi::MojoReadDataOptions::new(ReadFlags::QUERY.bits());
        let mut num_bytes: u32 = 0;
        // SAFETY: This call is safe because num_bytes is not null.
        let r_prelim = unsafe {
            mojo_ffi::MojoReadData(
                self.handle.get_native_handle(),
                options.as_ptr(),
                ptr::null_mut() as *mut c_void,
                &mut num_bytes as *mut u32,
            )
        };
        if num_bytes == 0 {
            return Ok(0);
        }
        if r_prelim != 0 {
            return Err(MojoResult::from_code(r_prelim));
        }

        // Assuming no error, read the actual data.
        options.flags = flags.bits();
        // This call reads until the buffer is full OR there
        // is no more data in the pipe, whichever comes first.
        // SAFETY: This call is safe because num_bytes is not null.
        let r = MojoResult::from_code(unsafe {
            mojo_ffi::MojoReadData(
                self.handle.get_native_handle(),
                options.as_ptr(),
                buf.as_mut_ptr() as *mut c_void,
                &mut num_bytes as *mut u32,
            )
        });
        if r != MojoResult::Okay {
            Err(r)
        } else {
            Ok(num_bytes as usize)
        }
    }
}

pub struct DataPipeProducerHandle {
    handle: UntypedHandle,
}

impl DataPipeProducerHandle {
    /// Perform a write operation on the producer end of the data pipe. Returns
    /// the number of elements actually written.
    ///
    /// # Implementation notes
    ///
    /// The underlying C API is thread agnostic, leaving the choice of how to
    /// synchronize writes to the caller. We take `&mut self`
    /// here to ensure thread safety.
    ///
    /// FOR_RELEASE: Implement `write` and `write_all` from the `Write` trait.
    pub fn write_with_flags(
        &mut self,
        data: &[u8],
        flags: WriteFlags,
    ) -> Result<usize, MojoResult> {
        let Ok(mut num_bytes) = u32::try_from(data.len()) else {
            return Err(MojoResult::ResourceExhausted);
        };
        let options = mojo_ffi::MojoWriteDataOptions::new(flags.bits());
        // SAFETY: This is safe because we have exclusive access to `handle` while
        // writing, data.as_ptr() points to num_bytes of valid memory, and num_bytes is
        // not null.
        match MojoResult::from_code(unsafe {
            mojo_ffi::MojoWriteData(
                self.handle.get_native_handle(),
                data.as_ptr() as *const c_void,
                &mut num_bytes as *mut u32,
                options.as_ptr(),
            )
        }) {
            MojoResult::Okay => Ok(num_bytes as usize),
            e => Err(e),
        }
    }
}

/// Creates a data pipe and returns the DataPipeConsumerHandle and
/// DataPipeProducerHandle for that pipe. `capacity` is the minimum number of
/// bytes that can be queued. A user can write at least this many bytes to
/// the pipe before they are read. If `capacity` is zero it is chosen by the
/// system but is at least one.
///
/// The pipe solely produces and consumes raw bytes. Interpreting those bytes
/// into higher-level types is a job for the caller.
pub fn create(
    capacity: u32,
) -> Result<(DataPipeConsumerHandle, DataPipeProducerHandle), MojoResult> {
    let mut consumer_handle = UntypedHandle::invalid();
    let mut producer_handle = UntypedHandle::invalid();
    let options = mojo_ffi::MojoCreateDataPipeOptions::new(0, 1, capacity);

    // SAFETY: This is safe because we are creating a pipe with two newly-created
    // handles that have not already been assigned a value, so they are valid
    // arguments for the function.
    match MojoResult::from_code(unsafe {
        mojo_ffi::MojoCreateDataPipe(
            options.as_ptr(),
            producer_handle.as_mut_ptr(),
            consumer_handle.as_mut_ptr(),
        )
    }) {
        MojoResult::Okay => Ok((
            DataPipeConsumerHandle { handle: consumer_handle },
            DataPipeProducerHandle { handle: producer_handle },
        )),
        e => Err(e),
    }
}
