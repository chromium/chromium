// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::ptr;

use crate::system::ffi::{self, raw_ffi};
use crate::system::handle::Handle;
use crate::system::mojo_types;
use crate::system::mojo_types::MojoResult;

#[repr(u32)]
/// Create flags for wait sets
pub enum Create {
    None = 0,
}

#[repr(u32)]
/// Add flags for wait sets
pub enum Add {
    None = 0,
}

/// This struct represents a handle to a wait set in the Mojo system.
///
/// The primary purpose of a wait set is to provide an abstraction for
/// efficiently waiting asynchronously (and cooperatively) on a set of
/// handles which are registered with it.
pub struct WaitSet {
    handle: mojo_types::MojoWaitSetHandle,
}

impl WaitSet {
    /// Creates a new WaitSet object in the Mojo system, and returns a wrapper
    /// for it. If creation fails, returns the result code.
    pub fn new(flags: mojo_types::CreateFlags) -> Result<WaitSet, MojoResult> {
        let mut handle = 0;
        let opts = ffi::MojoCreateWaitSetOptions::new(flags);
        let raw_opts = opts.inner_ptr();
        let r = MojoResult::from_code(unsafe {
            ffi::MojoCreateWaitSet(raw_opts, &mut handle as *mut _)
        });
        if r != MojoResult::Okay { Err(r) } else { Ok(WaitSet { handle: handle }) }
    }

    /// Adds a handle to the underlying wait set.
    ///
    /// The handle that is added may go invalid, at which point the result
    /// returned from wait_on_set for this handle will be `Cancelled'.
    ///
    /// One can pass in a unique cookie value which is used to identify the
    /// handle in the wait result. Currently there are no supported flags,
    /// but the argument is kept for future usage.
    pub fn add(
        &mut self,
        handle: &dyn Handle,
        signals: mojo_types::HandleSignals,
        cookie: u64,
        flags: mojo_types::AddFlags,
    ) -> MojoResult {
        let opts = ffi::MojoWaitSetAddOptions::new(flags);
        let raw_opts = opts.inner_ptr();
        MojoResult::from_code(unsafe {
            ffi::MojoWaitSetAdd(
                self.handle,
                handle.get_native_handle(),
                signals.get_bits(),
                cookie,
                raw_opts,
            )
        })
    }

    /// Removes a handle from the underlying wait set by cookie value.
    pub fn remove(&mut self, cookie: u64) -> MojoResult {
        MojoResult::from_code(unsafe { ffi::MojoWaitSetRemove(self.handle, cookie) })
    }

    /// Waits on this wait set.
    ///
    /// The conditions for the wait to end include:
    /// * A handle has its requested signals satisfied.
    /// * A handle is determined to never be able to have its requested
    ///   signals satisfied.
    /// * The deadline expires.
    /// * This wait set handle becomes invalid (Fatal error in this bindings).
    ///
    /// On a successful wait, we return the maximum number of results that could
    /// possibly be returned (similar to the total number of registered handles).
    /// Additionally, populates the output vector with the results of each handle
    /// that completed waiting.
    ///
    /// On a failed wait, we return the result code.
    pub fn wait_on_set(&self, output: &mut Vec<mojo_types::WaitSetResult>) -> MojoResult {
        assert!((output.capacity() as u64) <= ((1 as u64) << 32));
        let mut num_results = output.capacity() as u32;
        let mut output_ptr = output.as_mut_ptr() as *mut raw_ffi::MojoWaitSetResult;
        if num_results == 0 {
            output_ptr = ptr::null_mut();
        }
        let r = MojoResult::from_code(unsafe {
            ffi::MojoWaitSetWait(self.handle, &mut num_results as *mut u32, output_ptr)
        });
        unsafe {
            output.set_len(num_results as usize);
        }
        r
    }
}
