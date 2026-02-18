// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cxx::CxxString;
pub struct RustRepeatingStringCallback(Box<dyn FnMut(&CxxString) + Send>);

impl RustRepeatingStringCallback {
    /// Execute the contained callback. The silly input type is to work around
    /// the fact that cxx won't let us pass opaque rust objects by value so we
    /// have to stick it in a box and pass _that_ around.
    pub fn run(slf: &mut Box<Self>, msg: &CxxString) {
        slf.0(msg);
    }
}

impl<T: FnMut(&CxxString) + Send + 'static> From<T> for RustRepeatingStringCallback {
    fn from(f: T) -> Self {
        Self(Box::new(f))
    }
}

#[cxx::bridge(namespace = "rustmojo_system_api")]
mod ffi {
    unsafe extern "C++" {
        include!("mojo/core/embedder/embedder.h");
    }

    extern "Rust" {
        #[derive(ExternType)]
        type RustRepeatingStringCallback;

        #[Self = "RustRepeatingStringCallback"]
        fn run(slf: &mut Box<RustRepeatingStringCallback>, msg: &CxxString);
    }

    unsafe extern "C++" {
        include!("mojo/public/rust/system/test_util/cxx_shim.h");

        // FOR_RELEASE: This doesn't logically need to be boxed, but cxx
        // requires it. See if we can avoid it, maybe be making
        // RustRepeatingStringCallback a type alias instead of a struct
        fn SetDefaultProcessErrorHandler(handler: Box<RustRepeatingStringCallback>);
    }
}

/// Set the default behavior when mojo receives a report of a bad message.
/// Wrapper around the function of the same name in
/// //mojo/public/cpp/system/ functions.h
pub fn set_default_process_error_handler(mut handler: impl FnMut(&str) + Send + 'static) {
    let cxx_closure = move |msg: &CxxString| handler(&msg.to_string_lossy());
    ffi::SetDefaultProcessErrorHandler(Box::new(cxx_closure.into()));
}
