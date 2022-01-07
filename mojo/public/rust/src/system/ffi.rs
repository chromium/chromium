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

// This full import is intentional; nearly every type in mojo_types needs to be used.
use system::mojo_types::*;

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
    pub type MojoBufferInfoFlags = u32;
    pub type MojoMapBufferFlags = u32;
    pub type MojoCreateDataPipeOptionsFlags = u32;
    pub type MojoWriteDataFlags = u32;
    pub type MojoReadDataFlags = u32;
    pub type MojoHandleSignals = u32;
    pub type MojoCreateMessagePipeOptionsFlags = u32;
    pub type MojoWriteMessageFlags = u32;
    pub type MojoReadMessageFlags = u32;
    pub type MojoCreateWaitSetOptionsFlags = u32;
    pub type MojoWaitSetAddOptionsFlags = u32;
    pub type MojoResultCode = u32;
}

use system::ffi::types::*;

#[repr(C)]
pub struct MojoCreateSharedBufferOptions {
    pub struct_size: u32,
    pub flags: MojoCreateSharedBufferOptionsFlags,
    pub _align: [u64; 0], // Hack to align struct to 8 byte boundary
}

#[repr(C)]
pub struct MojoDuplicateBufferHandleOptions {
    pub struct_size: u32,
    pub flags: MojoDuplicateBufferHandleOptionsFlags,
    pub _align: [u64; 0], // Hack to align struct to 8 byte boundary
}

#[repr(C)]
pub struct MojoBufferInformation {
    pub struct_size: u32,
    pub flags: MojoBufferInfoFlags,
    pub num_bytes: u64,
    pub _align: [u64; 0], // Hack to align struct to 8 byte boundary
}

#[repr(C)]
pub struct MojoCreateDataPipeOptions {
    pub struct_size: u32,
    pub flags: MojoCreateDataPipeOptionsFlags,
    pub element_num_bytes: u32,
    pub capacity_num_bytes: u32,
    pub _align: [u64; 0], // Hack to align struct to 8 byte boundary
}

#[repr(C)]
pub struct MojoCreateMessagePipeOptions {
    pub struct_size: u32,
    pub flags: MojoCreateMessagePipeOptionsFlags,
    pub _align: [u64; 0], // Hack to align struct to 8 byte boundary
}

#[repr(C)]
pub struct MojoCreateWaitSetOptions {
    pub struct_size: u32,
    pub flags: MojoCreateWaitSetOptionsFlags,
    pub _align: [u64; 0], // Hack to align struct to 8 byte boundary
}

#[repr(C)]
pub struct MojoWaitSetAddOptions {
    pub struct_size: u32,
    pub flags: MojoWaitSetAddOptionsFlags,
    pub _align: [u64; 0], // Hack to align struct to 8 byte boundary
}

#[link]
extern "C" {
    // From //mojo/public/c/include/mojo/system/buffer.h
    pub fn MojoCreateSharedBuffer(options: *const MojoCreateSharedBufferOptions,
                                  num_bytes: u64,
                                  shared_buffer_handle: *mut MojoHandle)
                                  -> MojoResultCode;

    pub fn MojoDuplicateBufferHandle(handle: MojoHandle,
                                     options: *const MojoDuplicateBufferHandleOptions,
                                     new_buffer_handle: *mut MojoHandle)
                                     -> MojoResultCode;

    pub fn MojoGetBufferInformation(buffer_handle: MojoHandle,
                                    info: *mut MojoBufferInformation,
                                    info_num_bytes: u32)
                                    -> MojoResultCode;

    pub fn MojoMapBuffer(buffer_handle: MojoHandle,
                         offset: u64,
                         num_bytes: u64,
                         buffer: *mut *mut c_void,
                         flags: MojoMapBufferFlags)
                         -> MojoResultCode;

    pub fn MojoUnmapBuffer(buffer: *const c_void) -> MojoResultCode;

    // From //mojo/public/c/include/mojo/system/data_pipe.h
    pub fn MojoCreateDataPipe(options: *const MojoCreateDataPipeOptions,
                              data_pipe_producer_handle: *mut MojoHandle,
                              data_pipe_consumer_handle: *mut MojoHandle)
                              -> MojoResultCode;

    pub fn MojoWriteData(data_pipe_producer_handle: MojoHandle,
                         elements: *const c_void,
                         num_bytes: *mut u32,
                         flags: MojoWriteDataFlags)
                         -> MojoResultCode;

    pub fn MojoBeginWriteData(data_pipe_producer_handle: MojoHandle,
                              buffer: *mut *mut c_void,
                              buffer_num_bytes: *mut u32,
                              flags: MojoWriteDataFlags)
                              -> MojoResultCode;

    pub fn MojoEndWriteData(data_pipe_producer_handle: MojoHandle,
                            num_bytes_written: u32)
                            -> MojoResultCode;

    pub fn MojoReadData(data_pipe_consumer_handle: MojoHandle,
                        elements: *const c_void,
                        num_bytes: *mut u32,
                        flags: MojoReadDataFlags)
                        -> MojoResultCode;

    pub fn MojoBeginReadData(data_pipe_consumer_handle: MojoHandle,
                             buffer: *mut *mut c_void,
                             buffer_num_bytes: *mut u32,
                             flags: MojoReadDataFlags)
                             -> MojoResultCode;

    pub fn MojoEndReadData(data_pipe_consumer_handle: MojoHandle,
                           num_bytes_written: u32)
                           -> MojoResultCode;

    // From //mojo/public/c/include/mojo/system/handle.h
    pub fn MojoClose(handle: MojoHandle) -> MojoResultCode;

    // From //mojo/public/c/include/mojo/system/message_pipe.h
    pub fn MojoCreateMessagePipe(options: *const MojoCreateMessagePipeOptions,
                                 message_pipe_handle0: *mut MojoHandle,
                                 message_pipe_handle1: *mut MojoHandle)
                                 -> MojoResultCode;

    pub fn MojoWriteMessage(message_pipe_handle: MojoHandle,
                            bytes: *const c_void,
                            num_bytes: u32,
                            handles: *const MojoHandle,
                            num_handles: u32,
                            flags: MojoWriteMessageFlags)
                            -> MojoResultCode;

    pub fn MojoReadMessage(message_pipe_handle: MojoHandle,
                           bytes: *mut c_void,
                           num_bytes: *mut u32,
                           handles: *mut MojoHandle,
                           num_handles: *mut u32,
                           flags: MojoWriteMessageFlags)
                           -> MojoResultCode;

    // From //mojo/public/c/include/mojo/system/time.h
    pub fn MojoGetTimeTicksNow() -> MojoTimeTicks;

    // From //mojo/public/c/include/mojo/system/wait.h
    pub fn MojoWait(handle: MojoHandle,
                    signals: HandleSignals,
                    deadline: MojoDeadline,
                    signals_state: *mut SignalsState)
                    -> MojoResultCode;

    pub fn MojoWaitMany(handles: *const MojoHandle,
                        signals: *const HandleSignals,
                        num_handles: u32,
                        deadline: MojoDeadline,
                        result_index: *mut u32,
                        signals_states: *mut SignalsState)
                        -> MojoResultCode;

    // From //mojo/public/c/include/mojo/system/wait_set.h
    pub fn MojoCreateWaitSet(options: *const MojoCreateWaitSetOptions,
                             handle: *mut MojoHandle)
                             -> MojoResultCode;

    pub fn MojoWaitSetAdd(wait_set_handle: MojoHandle,
                          handle: MojoHandle,
                          signals: HandleSignals,
                          cookie: u64,
                          options: *const MojoWaitSetAddOptions)
                          -> MojoResultCode;

    pub fn MojoWaitSetRemove(wait_set_handle: MojoHandle, cookie: u64) -> MojoResultCode;

    pub fn MojoWaitSetWait(wait_set_handle: MojoHandle,
                           deadline: MojoDeadline,
                           num_results: *mut u32,
                           results: *mut WaitSetResult,
                           max_results: *mut u32)
                           -> MojoResultCode;
}
