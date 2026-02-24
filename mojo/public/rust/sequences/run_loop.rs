// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the user-visible interface for using a base::RunLoop
//! object from Rust.

use crate::cxx::ffi::RunLoop as CxxRunLoop;
use crate::cxx::ffi::{CreateRunLoop, QuitRunLoop, RunRunLoop};
use cxx::UniquePtr;
use std::sync::Arc;

pub struct RunLoop {
    run_loop: Arc<UniquePtr<CxxRunLoop>>,
}

impl Default for RunLoop {
    fn default() -> Self {
        Self::new()
    }
}

impl RunLoop {
    // Create a new RunLoop object.
    pub fn new() -> Self {
        RunLoop { run_loop: Arc::new(CreateRunLoop()) }
    }

    // Run the loop until this loop's `quit_closure()` is called. If it has already
    // been called for this loop, return immediately instead.
    //
    // Each loop can only be run once (further runs would just return immediately),
    // so this function takes the `RunLoop` by value to reflect that.
    pub fn run(self) {
        RunRunLoop(&self.run_loop);
    }

    // Return a closure that will quit `self` when executed.
    pub fn get_quit_closure(&self) -> impl Fn() + Send + 'static {
        let self_weak = Arc::downgrade(&self.run_loop);
        move || {
            if let Some(run_loop) = self_weak.upgrade() {
                QuitRunLoop(&run_loop)
            }
        }
    }
}
