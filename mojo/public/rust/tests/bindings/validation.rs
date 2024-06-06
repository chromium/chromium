// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Run standard mojo validation tests on Rust bindings.

use rust_gtest_interop::prelude::*;

chromium::import! {
    "//mojo/public/rust/tests:test_util";
}

macro_rules! define_validation_tests {
    {$($test_name:ident ;)*} => {
        $(
        #[gtest(ValidationTest, $test_name)]
        fn test() {
            let data = include_str!(
                concat!(
                    "../../../interfaces/bindings/tests/data/validation/",
                    stringify!($test_name),
                    ".data",
                )
            );

            let expectation = include_str!(
                concat!(
                    "../../../interfaces/bindings/tests/data/validation/",
                    stringify!($test_name),
                    ".expected",
                )
            );

            run_validation_test(data, expectation);
        }
        )*
    }
}

// Each test case corresponds to a pair of files in
// //mojo/public/interfaces/bindings/tests/data/validation/
define_validation_tests! {
    conformance_empty;
    conformance_msghdr_incomplete_struct;
    conformance_msghdr_incomplete_struct_header;
    conformance_msghdr_invalid_flag_combo;
    conformance_msghdr_missing_request_id;
    conformance_msghdr_num_bytes_huge;
    conformance_msghdr_num_bytes_less_than_min_requirement;
    conformance_msghdr_num_bytes_less_than_struct_header;
    conformance_msghdr_num_bytes_version_mismatch_1;
    conformance_msghdr_num_bytes_version_mismatch_2;
    conformance_msghdr_num_bytes_version_mismatch_3;
}

fn run_validation_test(input_data_str: &str, expected_result: &str) {
    let (data, _num_handles) = test_util::parse_validation_test_input(input_data_str).unwrap();

    let _message_view = match bindings::MessageView::new(&data) {
        Ok(m) => m,
        Err(validation_error) => {
            expect_eq!(expected_result.trim_end(), validation_error.to_str());
            return;
        }
    };

    // Message validation succeeded. Check if it should've failed.
    expect_eq!("PASS", expected_result.trim_end());
}
