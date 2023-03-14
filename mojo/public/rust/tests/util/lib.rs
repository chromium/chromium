// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains useful functions and macros for testing.

use std::env;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;
use std::slice;
use std::vec::Vec;

/// This macro sets up tests by adding in Mojo embedder initialization.
///
/// Note: this macro is quite delicate because of rustmt's inconsistent handling
/// of macro invocations. Slight changes to macro syntax can make rustfmt ignore
/// the inside of an invocation, which is not what we want.
#[macro_export]
macro_rules! mojo_test {
    {$i: ident, $(#[$attr:meta])* $b:block} => {
        #[test]
        $(
        #[ $attr ]
        )*
        fn $i() {
            $crate::init();
            $b
        }
    }
}

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
            InitializeMojoEmbedder(argc, argv);
        }
    });
}

extern "C" {
    fn free(ptr: *mut u8);
}

extern "C" {
    pub fn InitializeMojoEmbedder(argc: u32, argv: *const *const c_char);
}

extern "C" {
    #[allow(dead_code)]
    fn ParseValidationTest(
        input: *const c_char,
        num_handles: *mut usize,
        data: *mut *mut u8,
        data_len: *mut usize,
    ) -> *mut c_char;
}

#[allow(dead_code)]
pub fn parse_validation_test(input: &str) -> Result<(Vec<u8>, usize), String> {
    let input_c = CString::new(input.to_string()).unwrap();
    let mut num_handles: usize = 0;
    let mut data: *mut u8 = ptr::null_mut();
    let mut data_len: usize = 0;
    let error = unsafe {
        ParseValidationTest(
            input_c.as_ptr(),
            &mut num_handles as *mut usize,
            &mut data as *mut *mut u8,
            &mut data_len as *mut usize,
        )
    };
    if error == ptr::null_mut() {
        if data == ptr::null_mut() || data_len == 0 {
            // We assume we were just given an empty file
            Ok((Vec::new(), 0))
        } else {
            // Make a copy of the buffer
            let buffer;
            unsafe {
                buffer = slice::from_raw_parts(data, data_len).to_vec();
                free(data);
            }
            Ok((buffer, num_handles))
        }
    } else {
        let err_str;
        unsafe {
            // Copy the error string
            err_str = CStr::from_ptr(error)
                .to_str()
                .expect("Could not convert error message to UTF-8!")
                .to_owned();
            free(error as *mut u8);
        }
        Err(err_str)
    }
}
