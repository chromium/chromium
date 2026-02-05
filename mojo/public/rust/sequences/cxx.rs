// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the FFI bindings which are necessary for using
//! Sequenced Task Runners from Rust code. You shouldn't call these functions
//! yourself; instead, use a sequenced_task_runner::SequencedTaskRunnerHandle.

use crate::callback::RustOnceClosure;
use crate::scoped_refptr::{CxxRefCounted, CxxRefCountedThreadSafe};

#[cxx::bridge(namespace = "rust_sequences")]
pub mod ffi {
    // Bringing in the SequencedTaskRunner type
    unsafe extern "C++" {
        include!("base/task/sequenced_task_runner.h");
        #[namespace = "base"]
        type SequencedTaskRunner;

        // Self is implicitly SequencedTaskRunner because it's the only type
        // in this extern block
        fn AddRef(&self);

        // TODO(crbug.com/472552387): Tweak `cxx` to make this `allow` obsolete.
        #[allow(clippy::missing_safety_doc)]
        /// # Safety
        ///
        /// The caller must assert that it owns 1 ref-count of this
        /// object, and no code will ever dereference this instance of `self`
        /// afterwards unless they know the ref-count is at least 1 (e.g.
        /// because they have a different ref-counted pointer to the
        /// same object).
        unsafe fn Release(&self);
    }

    // Allow C++ to use RustOnceClosure types
    extern "Rust" {
        type RustOnceClosure;

        #[Self = "RustOnceClosure"]
        fn run(boxed: Box<RustOnceClosure>);
    }

    // Bringing in an API for base::RunLoop
    unsafe extern "C++" {
        include!("base/run_loop.h");
        #[namespace = "base"]
        type RunLoop;
    }

    // Bringing in helper functions from the C++ shim
    unsafe extern "C++" {
        include!("mojo/public/rust/sequences/cxx_shim.h");

        // We need a shim here because the normal `GetCurrentDefault` function
        // returns a scoped_refptr, and we can't pass arbitrary generic/templated
        // types across the bridge.
        fn GetCurrentDefaultSequencedTaskRunnerForRust() -> *mut SequencedTaskRunner;

        // TODO(crbug.com/469133195): Figure out how to pass a location as well
        fn PostTaskFromRust(
            runner: Pin<&mut SequencedTaskRunner>,
            task: Box<RustOnceClosure>,
        ) -> bool;

        // We need a shim because cxx won't let us allocate on the stack.
        fn CreateRunLoop() -> UniquePtr<RunLoop>;

        // Call run_loop.Run(). We need a shim because cxx doesn't support
        // functions with default arguments, and for the same reason as
        // `QuitRunLoop` below.
        fn RunRunLoop(run_loop: &UniquePtr<RunLoop>);

        // Quit the given `RunLoop`. We need a shim because the function takes
        // a mutable reference, but since it's thread safe we need to be able to
        // call it with a shared reference.
        fn QuitRunLoop(run_loop: &UniquePtr<RunLoop>);
    }
}

// SAFETY:
// The C++ implementation guarantees that ref-counting is the only mechanism
// managing the lifetime of a `SequencedTaskRunner`.
unsafe impl CxxRefCounted for ffi::SequencedTaskRunner {
    fn add_ref(&self) {
        self.AddRef();
    }

    // SAFETY: The trait imposes the same requirements as `Release`.
    unsafe fn release(&self) {
        unsafe {
            self.Release();
        }
    }
}

// SAFETY: The C++ implementation of this class is designed to be thread-safe.
unsafe impl CxxRefCountedThreadSafe for ffi::SequencedTaskRunner {}
unsafe impl Send for ffi::SequencedTaskRunner {}
unsafe impl Sync for ffi::SequencedTaskRunner {}

// SAFETY: we only expose the thread-safe subset of RunLoop's functionality
unsafe impl Send for ffi::RunLoop {}
unsafe impl Sync for ffi::RunLoop {}
