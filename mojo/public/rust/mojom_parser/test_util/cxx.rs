// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cxx::{let_cxx_string, CxxVector};

#[cxx::bridge(namespace = "validation_parser_bridge")]
mod ffi {
    extern "C++" {
        include!("mojo/public/cpp/bindings/tests/validation_test_input_parser.h");

        /// Calls ParseValidationTestInput, declared the above header.
        /// `input` is the validation file contents to be parsed; the other
        /// arguments are out-parameters and will be cleared/written to.
        /// See the C++ function definition for more details.
        #[namespace = "mojo::test"]
        unsafe fn ParseValidationTestInput(
            input: &CxxString,
            data: *mut CxxVector<u8>,
            num_handles: *mut usize,
            error_message: *mut CxxString,
        ) -> bool;
    }
}

pub struct ValidationTestData {
    pub data: Box<[u8]>,
    pub num_handles: usize,
}

/// Parse the validation text format into binary data; also returns
/// the number of handles in the message. On failure, returns an error message.
pub fn parse(contents: &str) -> Result<ValidationTestData, String> {
    // Binds a variable `input` of type Pin<&mut CxxString>
    let_cxx_string!(input = contents);

    let data = CxxVector::new();
    let mut num_handles: usize = 0;
    let_cxx_string!(error_message = "");

    // SAFETY: We're using CXX types, which are designed to be compatible with
    // C++. The pointers are coming from references, so they are never null.
    let success = unsafe {
        ffi::ParseValidationTestInput(
            &input,
            data.as_mut_ptr(),
            &mut num_handles as *mut _,
            // SAFETY: The C++ code won't move the data here, and we drop it
            // at the end of this function, so we won't violate any pinning
            // invariants.
            std::pin::Pin::into_inner_unchecked(error_message.as_mut()) as *mut _,
        )
    };

    if success {
        let data: Box<[u8]> = Box::from(data.as_slice());
        Ok(ValidationTestData { data, num_handles })
    } else {
        Err(error_message.to_string())
    }
}
