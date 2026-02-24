//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
  "//mojo/public/rust/system:ffi_bindings" as mojo_ffi;
}

use crate::mojo_types::declare_trappable_typed_handle;
use mojo_ffi::data_pipe;
use mojo_ffi::{MojoResult, UntypedHandle};

// FOR_RELEASE: Make these arguments to the functions instead of bitfields
pub use data_pipe::{ReadFlags, WriteFlags};

declare_trappable_typed_handle!(DataPipeProducerHandle);
declare_trappable_typed_handle!(DataPipeConsumerHandle);

// FOR_RELEASE: These impls are a fine starting point, but we can replace them
// with more ergonomic interfaces:
// - Implement Read/BufRead/Write/etc instead of standalone functions.
// - Split `read_with_flags` into multiple functions (read, peek, query,
//   discard).

impl DataPipeConsumerHandle {
    pub fn read_with_flags(
        &self,
        buf: &mut [std::mem::MaybeUninit<u8>],
        flags: ReadFlags,
    ) -> MojoResult<usize> {
        data_pipe::MojoReadData(&self.handle, buf, flags).map(|n| n.try_into().unwrap())
    }
}

impl DataPipeProducerHandle {
    pub fn write_with_flags(&self, data: &[u8], flags: WriteFlags) -> MojoResult<usize> {
        data_pipe::MojoWriteData(&self.handle, data, flags).map(|n| n.try_into().unwrap())
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
pub fn create(capacity: u32) -> MojoResult<(DataPipeProducerHandle, DataPipeConsumerHandle)> {
    data_pipe::MojoCreateDataPipe(1, capacity)
        .map(|pipe| (pipe.producer.into(), pipe.consumer.into()))
}
