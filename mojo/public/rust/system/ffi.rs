// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This ffi module is used to interact with the
//! Mojo C bindings API. The structures below are
//! undocumented because they are pulled exactly
//! from the header files of the Mojo C bindings
//! API which can be found in the Mojo repository[1]
//! under //mojo/public/c/include/mojo/system. Very
//! clear documentation on these structures and
//! functions can be found there. It is not worth
//! elaborating on these all again here.
//!
//! [1] https://github.com/domokit/mojo
//!
//! TODO(https://crbug.com/1274864):
//! * Remove references to the now-nonexistent mojo Github
//! * Automatically generate these FFI bindings, or at least add validation
//!   (a la cxx)

// This full import is intentional; nearly every type in mojo_types needs to be used.
use crate::system::mojo_types::*;

#[allow(bad_style)]
/// This empty enum is used solely to provide
/// a notion of void from C. The truth is, the
/// correct move here is to use the libc Rust
/// package but, as it turns out, that's the only
/// part of libc we actually need. Rather than
/// force ourselves to pull in a dependency, we
/// instead implement libc's notion of c_void
/// here.
pub enum c_void {}

pub mod types {
    //! Defines some C-compatible types for the ffi layer of
    //! the bindings.

    pub type MojoCreateSharedBufferOptionsFlags = u32;
    pub type MojoDuplicateBufferHandleOptionsFlags = u32;
    pub type MojoGetBufferInfoFlags = u32;
    pub type MojoMapBufferFlags = u32;
    pub type MojoCreateDataPipeOptionsFlags = u32;
    pub type MojoWriteDataFlags = u32;
    pub type MojoBeginWriteDataFlags = u32;
    pub type MojoEndWriteDataFlags = u32;
    pub type MojoReadDataFlags = u32;
    pub type MojoBeginReadDataFlags = u32;
    pub type MojoEndReadDataFlags = u32;
    pub type MojoHandleSignals = u32;
    pub type MojoCreateMessagePipeOptionsFlags = u32;
    pub type MojoCreateMessageFlags = u32;
    pub type MojoAppendMessageDataFlags = u32;
    pub type MojoGetMessageDataFlags = u32;
    pub type MojoWriteMessageFlags = u32;
    pub type MojoReadMessageFlags = u32;
    pub type MojoCreateWaitSetOptionsFlags = u32;
    pub type MojoWaitSetAddOptionsFlags = u32;
    pub type MojoResultCode = u32;
}

use crate::system::ffi::types::*;

// Most FFI functions take an options struct as input. Each one contains a `struct_size` member for versioning. To reduce boilerplate, make a macro to define each struct with a `new` function that fills in the size.
// The macro is used as follows: declare_mojo_options!(<struct name>, <struct member 1>, <struct member 2>, ...)
macro_rules! declare_mojo_options {
    ($name:ident, $( $mem:ident : $t:ty ),*) => {
        // Mojo requires each options struct to be 8-byte aligned.
        #[repr(C, align(8))]
        pub struct $name {
            // This field is intentionally private.
            struct_size: u32,
            $(pub $mem : $t),*
        }

        impl $name {
            // Avoid a warning if nobody ever uses this function.
            #[allow(dead_code)]
            pub fn new($($mem : $t),*) -> $name {
                $name {
                    struct_size: ::std::mem::size_of::<$name>() as u32,
                    $($mem : $mem),*
                }
            }
        }
    }
}

declare_mojo_options!(MojoCreateSharedBufferOptions, flags: MojoCreateSharedBufferOptionsFlags);
declare_mojo_options!(MojoGetBufferInfoOptions, flags: MojoGetBufferInfoFlags);
declare_mojo_options!(MojoMapBufferOptions, flags: MojoMapBufferFlags);
declare_mojo_options!(
    MojoDuplicateBufferHandleOptions,
    flags: MojoDuplicateBufferHandleOptionsFlags
);
declare_mojo_options!(MojoSharedBufferInfo, size: u64);
declare_mojo_options!(
    MojoCreateDataPipeOptions,
    flags: MojoCreateDataPipeOptionsFlags,
    element_num_bytes: u32,
    capacity_num_bytes: u32
);
declare_mojo_options!(MojoWriteDataOptions, flags: MojoWriteDataFlags);
declare_mojo_options!(MojoBeginWriteDataOptions, flags: MojoBeginWriteDataFlags);
declare_mojo_options!(MojoEndWriteDataOptions, flags: MojoEndWriteDataFlags);
declare_mojo_options!(MojoReadDataOptions, flags: MojoReadDataFlags);
declare_mojo_options!(MojoBeginReadDataOptions, flags: MojoBeginReadDataFlags);
declare_mojo_options!(MojoEndReadDataOptions, flags: MojoEndReadDataFlags);
declare_mojo_options!(MojoCreateMessagePipeOptions, flags: MojoCreateMessagePipeOptionsFlags);
declare_mojo_options!(MojoWriteMessageOptions, flags: MojoWriteMessageFlags);
declare_mojo_options!(MojoReadMessageOptions, flags: MojoReadMessageFlags);
declare_mojo_options!(MojoCreateMessageOptions, flags: MojoCreateMessageFlags);
declare_mojo_options!(MojoAppendMessageDataOptions, flags: MojoAppendMessageDataFlags);
declare_mojo_options!(MojoGetMessageDataOptions, flags: MojoGetMessageDataFlags);
declare_mojo_options!(MojoCreateWaitSetOptions, flags: MojoCreateWaitSetOptionsFlags);
declare_mojo_options!(MojoWaitSetAddOptions, flags: MojoWaitSetAddOptionsFlags);

extern "C" {
    // From //mojo/public/c/include/mojo/system/buffer.h
    pub fn MojoCreateSharedBuffer(
        num_bytes: u64,
        options: *const MojoCreateSharedBufferOptions,
        shared_buffer_handle: *mut MojoHandle,
    ) -> MojoResultCode;

    pub fn MojoDuplicateBufferHandle(
        handle: MojoHandle,
        options: *const MojoDuplicateBufferHandleOptions,
        new_buffer_handle: *mut MojoHandle,
    ) -> MojoResultCode;

    pub fn MojoGetBufferInfo(
        buffer_handle: MojoHandle,
        options: *const MojoGetBufferInfoOptions,
        info: *mut MojoSharedBufferInfo,
    ) -> MojoResultCode;

    pub fn MojoMapBuffer(
        buffer_handle: MojoHandle,
        offset: u64,
        num_bytes: u64,
        options: *const MojoMapBufferOptions,
        buffer: *mut *mut c_void,
    ) -> MojoResultCode;

    pub fn MojoUnmapBuffer(buffer: *const c_void) -> MojoResultCode;

    // From //mojo/public/c/include/mojo/system/data_pipe.h
    pub fn MojoCreateDataPipe(
        options: *const MojoCreateDataPipeOptions,
        data_pipe_producer_handle: *mut MojoHandle,
        data_pipe_consumer_handle: *mut MojoHandle,
    ) -> MojoResultCode;

    pub fn MojoWriteData(
        data_pipe_producer_handle: MojoHandle,
        elements: *const c_void,
        num_bytes: *mut u32,
        options: *const MojoWriteDataOptions,
    ) -> MojoResultCode;

    pub fn MojoBeginWriteData(
        data_pipe_producer_handle: MojoHandle,
        options: *const MojoBeginWriteDataOptions,
        buffer: *mut *mut c_void,
        buffer_num_bytes: *mut u32,
    ) -> MojoResultCode;

    pub fn MojoEndWriteData(
        data_pipe_producer_handle: MojoHandle,
        num_bytes_written: u32,
        options: *const MojoEndWriteDataOptions,
    ) -> MojoResultCode;

    pub fn MojoReadData(
        data_pipe_consumer_handle: MojoHandle,
        options: *const MojoReadDataOptions,
        elements: *const c_void,
        num_bytes: *mut u32,
    ) -> MojoResultCode;

    pub fn MojoBeginReadData(
        data_pipe_consumer_handle: MojoHandle,
        options: *const MojoBeginReadDataOptions,
        buffer: *mut *mut c_void,
        buffer_num_bytes: *mut u32,
    ) -> MojoResultCode;

    pub fn MojoEndReadData(
        data_pipe_consumer_handle: MojoHandle,
        num_bytes_written: u32,
        options: *const MojoEndReadDataOptions,
    ) -> MojoResultCode;

    // From //mojo/public/c/include/mojo/system/handle.h
    pub fn MojoClose(handle: MojoHandle) -> MojoResultCode;

    // From //mojo/public/c/include/mojo/system/message_pipe.h
    pub fn MojoCreateMessagePipe(
        options: *const MojoCreateMessagePipeOptions,
        message_pipe_handle0: *mut MojoHandle,
        message_pipe_handle1: *mut MojoHandle,
    ) -> MojoResultCode;

    pub fn MojoWriteMessage(
        message_pipe_handle: MojoHandle,
        message_handle: MojoMessageHandle,
        options: *const MojoWriteMessageOptions,
    ) -> MojoResultCode;

    pub fn MojoReadMessage(
        message_pipe_handle: MojoHandle,
        options: *const MojoReadMessageOptions,
        message_handle: *mut MojoMessageHandle,
    ) -> MojoResultCode;

    pub fn MojoCreateMessage(
        options: *const MojoCreateMessageOptions,
        message_handle: *mut MojoMessageHandle,
    ) -> MojoResultCode;

    pub fn MojoDestroyMessage(message_handle: MojoMessageHandle) -> MojoResultCode;

    pub fn MojoAppendMessageData(
        message_handle: MojoMessageHandle,
        payload_size: u32,
        handles: *const MojoHandle,
        num_handles: u32,
        options: *const MojoAppendMessageDataOptions,
        buffer: *mut *mut c_void,
        buffer_size: *mut u32,
    ) -> MojoResultCode;

    pub fn MojoGetMessageData(
        message_handle: MojoMessageHandle,
        options: *const MojoGetMessageDataOptions,
        buffer: *mut *const c_void,
        num_bytes: *mut u32,
        handles: *mut MojoHandle,
        num_handles: *mut u32,
    ) -> MojoResultCode;

    // From //mojo/public/c/include/mojo/system/time.h
    pub fn MojoGetTimeTicksNow() -> MojoTimeTicks;

    // From //mojo/public/c/include/mojo/system/wait.h
    pub fn MojoWait(
        handle: MojoHandle,
        signals: HandleSignals,
        signals_state: *mut SignalsState,
    ) -> MojoResultCode;

    pub fn MojoWaitMany(
        handles: *const MojoHandle,
        signals: *const HandleSignals,
        num_handles: usize,
        result_index: *mut usize,
        signals_states: *mut SignalsState,
    ) -> MojoResultCode;

    // From //mojo/public/c/include/mojo/system/wait_set.h
    pub fn MojoCreateWaitSet(
        options: *const MojoCreateWaitSetOptions,
        handle: *mut MojoWaitSetHandle,
    ) -> MojoResultCode;

    pub fn MojoWaitSetAdd(
        wait_set_handle: MojoWaitSetHandle,
        handle: MojoHandle,
        signals: HandleSignals,
        cookie: u64,
        options: *const MojoWaitSetAddOptions,
    ) -> MojoResultCode;

    pub fn MojoWaitSetRemove(wait_set_handle: MojoWaitSetHandle, cookie: u64) -> MojoResultCode;

    pub fn MojoWaitSetWait(
        wait_set_handle: MojoWaitSetHandle,
        num_results: *mut u32,
        results: *mut WaitSetResult,
    ) -> MojoResultCode;
}
