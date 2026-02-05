// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//mojo/public/rust/sequences";
}

#[cxx::bridge(namespace = "rust_sequences_test")]
pub mod ffi {
    unsafe extern "C++" {
        include!("mojo/public/rust/sequences/test/test_util.h");
        pub type TestRefCounted;

        pub fn CreateTestRefCounted(b: &mut bool) -> *mut TestRefCounted;
        pub fn HasOneRef(&self) -> bool;
        pub fn HasAtLeastOneRef(&self) -> bool;

        fn AddRef(&self);

        // TODO(crbug.com/472552387): Tweak `cxx` to make this `allow` obsolete.
        #[allow(clippy::missing_safety_doc)]
        /// # Safety
        /// Same requirements as in sequences::scoped_refptr::CxxRefCounted.
        unsafe fn Release(&self);
    }

    unsafe extern "C++" {
        include!("base/test/task_environment.h");

        #[namespace = "base::test"]
        pub type SingleThreadTaskEnvironment;

        #[namespace = "base"]
        type SequencedTaskRunner = sequences::cxx::ffi::SequencedTaskRunner;

        pub fn CreateTaskEnvironment() -> UniquePtr<SingleThreadTaskEnvironment>;
    }
}

// SAFETY:
// The C++ implementation guarantees that ref-counting is the only mechanism
// managing the lifetime of a `SequencedTaskRunner`.
unsafe impl sequences::CxxRefCounted for ffi::TestRefCounted {
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
