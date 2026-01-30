//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
  "//mojo/public/rust/system:ffi_new" as mojo_ffi;
}

use mojo_ffi::data_pipe;
use mojo_ffi::{MojoResult, UntypedHandle};

// FOR_RELEASE: Make these arguments to the functions instead of bitfields
pub use data_pipe::{ReadFlags, WriteFlags};

// FOR_RELEASE: Put this somewhere more general once we've migrated more.
// TODO(crbug.com/479878778): If the C API ever exposes the ability to check
// a handle's type, we could do the check here and change this to `TryFrom`.
/// Helper macro to declare strongly-typed wrappers around an UntypedHandle
/// which are inter-convertible with it.
macro_rules! declare_typed_handle {
    ($name:ident) => {
        pub struct $name {
            handle: UntypedHandle,
        }

        impl From<UntypedHandle> for $name {
            fn from(handle: UntypedHandle) -> Self {
                Self { handle }
            }
        }

        impl From<$name> for UntypedHandle {
            fn from(typed_handle: $name) -> UntypedHandle {
                typed_handle.handle
            }
        }
    };
}

declare_typed_handle!(DataPipeProducerHandle);
declare_typed_handle!(DataPipeConsumerHandle);

// FOR_RELEASE: Do this once we've converted traps to the new FFI
// Maybe even add it to the macro, if all non-message handles are trappable
// impl Trappable for DataPipeConsumerHandle {}
// impl Trappable for DataPipeProducerHandle {}

/// FOR_RELEASE: These impls are a fine starting point, but we can replace them
/// with more ergonomic interfaces:
/// - Implement Read/BufRead/Write/etc instead of standalone functions.
/// - Split `read_with_flags` into multiple functions (read, peek, query,
///   discard).

impl DataPipeConsumerHandle {
    pub fn read_with_flags(
        &mut self,
        buf: &mut [std::mem::MaybeUninit<u8>],
        flags: ReadFlags,
    ) -> MojoResult<usize> {
        data_pipe::MojoReadData(&mut self.handle, buf, flags).map(|n| n.try_into().unwrap())
    }
}

impl DataPipeProducerHandle {
    pub fn write_with_flags(&mut self, data: &[u8], flags: WriteFlags) -> MojoResult<usize> {
        data_pipe::MojoWriteData(&mut self.handle, data, flags).map(|n| n.try_into().unwrap())
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
