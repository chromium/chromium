// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains useful functions and macros for testing.

pub mod mojom_validation;

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::slice;
use std::ptr;

/// This macro sets up tests by adding in Mojo embedder
/// initialization.
macro_rules! tests {
    ( $( $( #[ $attr:meta ] )* fn $i:ident() $b:block)* ) => {
        use std::sync::{Once, ONCE_INIT};
        static START: Once = ONCE_INIT;
        $(
            #[test]
            $(
            #[ $attr ]
            )*
            fn $i() {
                START.call_once(|| unsafe {
                    util::InitializeMojoEmbedder();
                });
                $b
            }
        )*
    }
}

#[link(name = "stdc++")]
extern "C" {}

#[link(name = "c")]
extern "C" {
    fn free(ptr: *mut u8);
}

#[link(name = "rust_embedder")]
extern "C" {
    pub fn InitializeMojoEmbedder();
}

#[link(name = "validation_parser")]
extern "C" {
    #[allow(dead_code)]
    fn ParseValidationTest(input: *const c_char,
                           num_handles: *mut usize,
                           data: *mut *mut u8,
                           data_len: *mut usize)
                           -> *mut c_char;
}

#[allow(dead_code)]
pub fn parse_validation_test(input: &str) -> Result<(Vec<u8>, usize), String> {
    let input_c = CString::new(input.to_string()).unwrap();
    let mut num_handles: usize = 0;
    let mut data: *mut u8 = ptr::null_mut();
    let mut data_len: usize = 0;
    let error = unsafe {
        ParseValidationTest(input_c.as_ptr(),
                            &mut num_handles as *mut usize,
                            &mut data as *mut *mut u8,
                            &mut data_len as *mut usize)
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
