// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::ptr;
use std::vec;

use crate::system::ffi;
use crate::system::ffi::types::{MojoHandleSignals, MojoHandleSignalsState};
// This full import is intentional; nearly every type in mojo_types needs to be used.
use crate::system::handle;
use crate::system::mojo_types::*;

/// Get the time ticks now according to the Mojo IPC. As
/// can be seen in the documentation for the Mojo C API,
/// time ticks are meaningless in an absolute sense. Instead,
/// one should compare the results of two of these calls to
/// get a proper notion of time passing.
pub fn get_time_ticks_now() -> MojoTimeTicks {
    unsafe { ffi::MojoGetTimeTicksNow() }
}

/// Waits on many handles (or rather any structures that wrap
/// handles) until the signals declared in 'signals' for each handle
/// are triggered, waiting for a maximum global time of 'deadline'.
/// This function blocks.
pub fn wait_many(
    handles: &[&dyn handle::Handle],
    signals: &[HandleSignals],
    states: &mut [SignalsState],
) -> (isize, MojoResult) {
    assert_eq!(handles.len(), signals.len());
    assert!(states.len() == handles.len() || states.len() == 0);
    let num_inputs = handles.len();
    if num_inputs == 0 {
        let result = MojoResult::from_code(unsafe {
            ffi::MojoWaitMany(ptr::null(), ptr::null(), 0, ptr::null_mut(), ptr::null_mut())
        });
        return (-1, result);
    }
    let raw_states = if states.len() != 0 {
        states.as_mut_ptr() as *mut MojoHandleSignalsState
    } else {
        ptr::null_mut()
    };
    let mut index: usize = usize::MAX;
    let raw_signals = signals.as_ptr() as *const MojoHandleSignals;
    let result = unsafe {
        let mut raw_handles: vec::Vec<MojoHandle> = vec::Vec::with_capacity(num_inputs);
        for handle in handles.iter() {
            raw_handles.push(handle.get_native_handle())
        }
        MojoResult::from_code(ffi::MojoWaitMany(
            raw_handles.as_ptr(),
            raw_signals,
            num_inputs,
            &mut index as *mut usize,
            raw_states,
        ))
    };
    (index as isize, result)
}
