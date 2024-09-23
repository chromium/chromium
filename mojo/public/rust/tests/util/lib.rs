// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains useful functions for testing.

use std::env;
use std::ffi::CString;
use std::os::raw::c_char;
use std::vec::Vec;

/// Calls Mojo initialization code on first call. Can be called multiple times.
/// Has no effect after the first call.
pub fn init() {
    // The initialization below must only be done once. For ease of safety, we
    // want to allow this function to be called multiple times. Wrap the inner
    // initialization code and only call it the first time.
    use std::sync::Once;
    static START: Once = Once::new();

    START.call_once(|| {
        let mut raw_args: Vec<*const c_char> = Vec::new();
        for a in env::args() {
            let cstr = CString::new(a).unwrap();
            raw_args.push(cstr.into_raw())
        }
        let argc = raw_args.len() as u32;
        let argv = raw_args.leak().as_ptr();

        // `InitializeMojoEmbedder` is safe to call only once. We ensure this
        // by using `std::sync::Once::call_once` above.
        unsafe {
            ffi::InitializeMojoEmbedder(argc, argv);
        }
    });
}

pub fn parse_validation_test_input(input: &str) -> Result<(Vec<u8>, usize), String> {
    cxx::let_cxx_string!(input_c = input);
    cxx::let_cxx_string!(error_message = "");

    let mut data_c = cxx::CxxVector::new();
    let mut num_handles = 0usize;

    let success = unsafe {
        ffi::ParseValidationTestInput(
            input_c.as_ref().get_ref(),
            data_c.as_mut().unwrap().get_unchecked_mut(),
            &mut num_handles as *mut _,
            error_message.as_mut().get_unchecked_mut() as *mut _,
        )
    };

    if !success {
        return Err(error_message.to_string_lossy().into_owned());
    }

    let output = data_c.iter().copied().collect();
    Ok((output, num_handles))
}

#[cxx::bridge]
mod ffi {
    extern "C++" {
        include!("mojo/public/rust/tests/test_support.h");

        unsafe fn InitializeMojoEmbedder(argc: u32, argv: *const *const c_char);

        #[namespace = "mojo::test"]
        unsafe fn ParseValidationTestInput(
            input: &CxxString,
            data: *mut CxxVector<u8>,
            num_handles: *mut usize,
            error_message: *mut CxxString,
        ) -> bool;
    }
}
