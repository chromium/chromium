// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::mem;
use std::vec;
use std::ptr;

use system::ffi;
use system::handle;
use system::handle::{CastHandle, Handle};
// This full import is intentional; nearly every type in mojo_types needs to be used.
use system::mojo_types::*;

#[repr(u32)]
/// Create flags for message pipes
pub enum Create {
    None = 0,
}

#[repr(u32)]
/// Write flags for message pipes
pub enum Write {
    None = 0,
}

#[repr(u32)]
/// Read flags for message pipes
pub enum Read {
    None = 0,

    /// If the message is unable to be
    /// read for whatever reason, dequeue
    /// it anyway
    MayDiscard = 1 << 0,
}

/// Creates a message pipe in Mojo and gives back two
/// MessageEndpoints which represent the endpoints of the
/// message pipe
pub fn create(flags: CreateFlags) -> Result<(MessageEndpoint, MessageEndpoint), MojoResult> {
    let mut handle0: MojoHandle = 0;
    let mut handle1: MojoHandle = 0;
    let opts = ffi::MojoCreateMessagePipeOptions {
        struct_size: mem::size_of::<ffi::MojoCreateMessagePipeOptions>() as u32,
        flags: flags,
        _align: [],
    };
    let raw_opts = &opts as *const ffi::MojoCreateMessagePipeOptions;
    let r = MojoResult::from_code(unsafe {
        ffi::MojoCreateMessagePipe(raw_opts,
                                   &mut handle0 as *mut MojoHandle,
                                   &mut handle1 as *mut MojoHandle)
    });
    if r != MojoResult::Okay {
        Err(r)
    } else {
        Ok((MessageEndpoint { handle: unsafe { handle::acquire(handle0) } },
            MessageEndpoint { handle: unsafe { handle::acquire(handle1) } }))
    }
}

/// Represents the one endpoint of a message pipe.
/// This data structure wraps a handle and acts
/// effectively as a typed handle.
pub struct MessageEndpoint {
    handle: handle::UntypedHandle,
}

impl MessageEndpoint {
    /// Read the next message from the endpoint. Messages in Mojo
    /// are some set of bytes plus a bunch of handles, so we
    /// return both a vector of bytes and a vector of untyped handles.
    ///
    /// Because the handles are untyped, it is up to the user of this
    /// library to know what type the handle actually is and to use
    /// from_untyped in order to convert the handle to the correct type.
    /// This is abstracted away, however, when using the Mojo bindings
    /// generator where you may specify your interface in Mojom.
    ///
    /// If an empty message (that is, it has neither data nor handles)
    /// is received, it will show up as an Err() containing MojoResult::Okay.
    pub fn read(&self,
                flags: ReadFlags)
                -> Result<(vec::Vec<u8>, vec::Vec<handle::UntypedHandle>), MojoResult> {
        let mut num_bytes: u32 = 0;
        let mut num_handles: u32 = 0;
        let result_prelim = MojoResult::from_code(unsafe {
            ffi::MojoReadMessage(self.handle.get_native_handle(),
                                 ptr::null_mut(),
                                 &mut num_bytes as *mut u32,
                                 ptr::null_mut(),
                                 &mut num_handles as *mut u32,
                                 flags)
        });
        if result_prelim != MojoResult::ResourceExhausted {
            return Err(result_prelim);
        }
        let mut buf: vec::Vec<u8> = vec::Vec::with_capacity(num_bytes as usize);
        let mut raw_handles: vec::Vec<MojoHandle> = vec::Vec::with_capacity(num_handles as usize);
        let buf_ptr;
        if num_bytes == 0 {
            buf_ptr = ptr::null_mut();
        } else {
            buf_ptr = buf.as_mut_ptr() as *mut ffi::c_void;
        }
        let raw_handles_ptr;
        if num_handles == 0 {
            raw_handles_ptr = ptr::null_mut();
        } else {
            raw_handles_ptr = raw_handles.as_mut_ptr();
        }
        let r = MojoResult::from_code(unsafe {
            ffi::MojoReadMessage(self.handle.get_native_handle(),
                                 buf_ptr,
                                 &mut num_bytes as *mut u32,
                                 raw_handles_ptr,
                                 &mut num_handles as *mut u32,
                                 flags)
        });
        unsafe {
            buf.set_len(num_bytes as usize);
            raw_handles.set_len(num_handles as usize);
        }
        let mut handles: vec::Vec<handle::UntypedHandle> =
            vec::Vec::with_capacity(num_handles as usize);
        for raw_handle in raw_handles.iter() {
            handles.push(unsafe { handle::acquire(*raw_handle) });
        }
        if r != MojoResult::Okay {
            Err(r)
        } else {
            Ok((buf, handles))
        }
    }

    /// Write a message to the endpoint. Messages in Mojo
    /// are some set of bytes plus a bunch of handles, so we
    /// return both a vector of bytes and a vector of untyped handles.
    ///
    /// Because the handles are untyped, it is up to the user of this
    /// library to know what type the handle actually is and to use
    /// from_untyped in order to convert the handle to the correct type.
    /// This is abstracted away, however, when using the Mojo bindings
    /// generator where you may specify your interface in Mojom.
    ///
    /// Additionally, the handles passed in are consumed. This is because
    /// Mojo handles operate on move semantics much like Rust data types.
    /// When a handle is sent through a message pipe it is invalidated and
    /// may not even be represented by the same integer on the other side,
    /// so care must be taken to design your application with this in mind.
    pub fn write(&self,
                 bytes: &[u8],
                 mut handles: vec::Vec<handle::UntypedHandle>,
                 flags: WriteFlags)
                 -> MojoResult {
        let bytes_ptr;
        if bytes.len() == 0 {
            bytes_ptr = ptr::null();
        } else {
            bytes_ptr = bytes.as_ptr() as *const ffi::c_void;
        }
        let mut raw_handles: vec::Vec<MojoHandle> = vec::Vec::with_capacity(handles.len());
        for handle in handles.iter_mut() {
            unsafe {
                raw_handles.push(handle.get_native_handle());
                handle.invalidate();
            }
        }
        let raw_handles_ptr;
        if raw_handles.len() == 0 {
            raw_handles_ptr = ptr::null();
        } else {
            raw_handles_ptr = raw_handles.as_ptr();
        }
        return MojoResult::from_code(unsafe {
            ffi::MojoWriteMessage(self.handle.get_native_handle(),
                                  bytes_ptr,
                                  bytes.len() as u32,
                                  raw_handles_ptr,
                                  raw_handles.len() as u32,
                                  flags)
        });
    }
}

impl CastHandle for MessageEndpoint {
    /// Generates a MessageEndpoint from an untyped handle wrapper
    /// See mojo::system::handle for information on untyped vs. typed
    unsafe fn from_untyped(handle: handle::UntypedHandle) -> Self {
        MessageEndpoint { handle: handle }
    }

    /// Consumes this object and produces a plain handle wrapper
    /// See mojo::system::handle for information on untyped vs. typed
    fn as_untyped(self) -> handle::UntypedHandle {
        self.handle
    }
}

impl Handle for MessageEndpoint {
    /// Returns the native handle wrapped by this structure.
    ///
    /// See mojo::system::handle for information on handle wrappers
    fn get_native_handle(&self) -> MojoHandle {
        self.handle.get_native_handle()
    }
}
