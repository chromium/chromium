// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ffi;
use crate::mojo_types::MojoTimeTicks;

/// Get the time ticks now according to the Mojo IPC. As
/// can be seen in the documentation for the Mojo C API,
/// time ticks are meaningless in an absolute sense. Instead,
/// one should compare the results of two of these calls to
/// get a proper notion of time passing.
pub fn get_time_ticks_now() -> MojoTimeTicks {
    unsafe { ffi::MojoGetTimeTicksNow() }
}
