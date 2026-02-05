// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the user-visible interface for using a sequenced task
//! runner from Rust code.

use crate::cxx::ffi;
use crate::scoped_refptr::ScopedRefPtr;

/// This type is the Rust representation of a C++
/// `scoped_refptr<base::SequencedTaskRunner>`. It can interoperate with
/// existing `scoped_refptr`s in C++ code, and keeps the task runner alive
/// at least until it goes out of scope.
#[derive(Clone)]
pub struct SequencedTaskRunnerHandle(ScopedRefPtr<ffi::SequencedTaskRunner>);

impl SequencedTaskRunnerHandle {
    /// Get the current default task runner. This function corresponds to
    /// `base::SequencedTaskRunner::GetCurrentDefault` in C++.
    pub fn get_current_default() -> Option<Self> {
        let default_ptr = ffi::GetCurrentDefaultSequencedTaskRunnerForRust();
        // SAFETY: The ffi function above returns a pointer that owns one ref-count
        unsafe { ScopedRefPtr::wrap_ref_counted(default_ptr) }.map(SequencedTaskRunnerHandle)
    }

    /// Post a task to the sequenced task runner. This function corresponds to
    /// `base::SequencedTaskRunner::PostTask`, but without the location
    /// argument (crbug.com/469133195).
    pub fn post_task<F: FnOnce() + Send + 'static>(&self, task: F) -> bool {
        ffi::PostTaskFromRust(self.0.as_pin(), Box::new(task.into()))
    }

    /// Retrieve the contained ScopedRefPtr. This should generally only be used
    /// for ffi purposes.
    pub fn as_scoped_refptr(&self) -> &ScopedRefPtr<ffi::SequencedTaskRunner> {
        &self.0
    }

    /// Run all tasks which have been queued up so far
    pub fn run_all_current_tasks_for_testing(&self) {
        let run_loop = crate::run_loop::RunLoop::new();
        self.post_task(run_loop.get_quit_closure());
        run_loop.run();
    }

    pub fn run_all_current_tasks_on_default_runner_for_testing() {
        Self::get_current_default()
            .expect("Must be called in a context with a default task runner")
            .run_all_current_tasks_for_testing();
    }
}
