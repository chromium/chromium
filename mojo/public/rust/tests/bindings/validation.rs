// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Run standard mojo validation tests on Rust bindings.

use rust_gtest_interop::prelude::*;

chromium::import! {
    "//mojo/public/rust/tests:test_util";
}

use test_interfaces::validation_test_interfaces::*;

macro_rules! define_validation_tests {
    {$($test_name:ident => $interface:ident $typ:ident;)*} => {
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

            define_validation_tests!{@call data expectation $interface $typ}
        }
        )*
    };
    {@call $data:ident $expectation:ident $interface:ident Request} => {
        run_validation_test::<$interface>($data, $expectation, false);
    };
    {@call $data:ident $expectation:ident $interface:ident Response} => {
        run_validation_test::<$interface>($data, $expectation, true);
    };
}

// Each test case corresponds to a pair of files in
// //mojo/public/interfaces/bindings/tests/data/validation/
define_validation_tests! {
    conformance_empty => ConformanceTestInterface Request;
    conformance_msghdr_incomplete_struct => ConformanceTestInterface Request;
    conformance_msghdr_incomplete_struct_header => ConformanceTestInterface Request;
    conformance_msghdr_invalid_flag_combo => ConformanceTestInterface Request;
    conformance_msghdr_missing_request_id => ConformanceTestInterface Request;
    conformance_msghdr_no_such_method => ConformanceTestInterface Request;
    conformance_msghdr_num_bytes_huge => ConformanceTestInterface Request;
    conformance_msghdr_num_bytes_less_than_min_requirement => ConformanceTestInterface Request;
    conformance_msghdr_num_bytes_less_than_struct_header => ConformanceTestInterface Request;
    conformance_msghdr_num_bytes_version_mismatch_1 => ConformanceTestInterface Request;
    conformance_msghdr_num_bytes_version_mismatch_2 => ConformanceTestInterface Request;
    conformance_msghdr_num_bytes_version_mismatch_3 => ConformanceTestInterface Request;
    conformance_mthd0_good => ConformanceTestInterface Request;
    conformance_mthd0_incomplete_struct => ConformanceTestInterface Request;
    conformance_mthd0_incomplete_struct_header => ConformanceTestInterface Request;
    conformance_mthd0_invalid_request_flags => ConformanceTestInterface Request;
    conformance_mthd0_invalid_request_flags2 => ConformanceTestInterface Request;
    conformance_mthd0_struct_num_bytes_huge => ConformanceTestInterface Request;
    conformance_mthd0_struct_num_bytes_less_than_min_requirement => ConformanceTestInterface Request;
    conformance_mthd0_struct_num_bytes_less_than_struct_header => ConformanceTestInterface Request;
    conformance_mthd1_good => ConformanceTestInterface Request;
    conformance_mthd1_misaligned_struct => ConformanceTestInterface Request;
    conformance_mthd1_struct_pointer_overflow => ConformanceTestInterface Request;
    conformance_mthd1_unexpected_null_struct => ConformanceTestInterface Request;
    conformance_mthd2_good => ConformanceTestInterface Request;
    conformance_mthd2_multiple_pointers_to_same_struct => ConformanceTestInterface Request;
    conformance_mthd2_overlapped_objects => ConformanceTestInterface Request;
    conformance_mthd2_wrong_layout_order => ConformanceTestInterface Request;
    conformance_mthd11_good_version0 => ConformanceTestInterface Request;
    conformance_mthd11_good_version1 => ConformanceTestInterface Request;
    conformance_mthd11_good_version2 => ConformanceTestInterface Request;
    conformance_mthd11_good_version3 => ConformanceTestInterface Request;
    conformance_mthd11_good_version_newer_than_known_1 => ConformanceTestInterface Request;
    conformance_mthd11_good_version_newer_than_known_2 => ConformanceTestInterface Request;
    conformance_mthd11_num_bytes_version_mismatch_1 => ConformanceTestInterface Request;
    conformance_mthd11_num_bytes_version_mismatch_2 => ConformanceTestInterface Request;
    conformance_mthd12_invalid_request_flags => ConformanceTestInterface Request;
    conformance_mthd14_good_known_enum_values => ConformanceTestInterface Request;
    conformance_mthd14_good_uknown_extensible_enum_value => ConformanceTestInterface Request;
    conformance_mthd14_uknown_non_extensible_enum_value => ConformanceTestInterface Request;
    resp_boundscheck_msghdr_no_such_method => ConformanceTestInterface Response;
    resp_conformance_msghdr_invalid_response_flags1 => ConformanceTestInterface Response;
    resp_conformance_msghdr_invalid_response_flags2 => ConformanceTestInterface Response;
    resp_conformance_msghdr_no_such_method => ConformanceTestInterface Response;
}

fn run_validation_test<I: bindings::mojom::Interface>(
    input_data_str: &str,
    expected_result: &str,
    is_response: bool,
) {
    let expected_result = expected_result.trim_end();
    match run_validation_test_impl::<I>(input_data_str, is_response) {
        Ok(()) => (),
        Err(validation_error) => {
            let kind_str = validation_error.kind().to_str();
            expect_eq!(expected_result, kind_str);
            if expected_result != kind_str {
                println!("{validation_error:?}");
            }
            return;
        }
    };

    // Message validation succeeded. Check if it should've failed.
    expect_eq!("PASS", expected_result);
}

fn run_validation_test_impl<I: bindings::mojom::Interface>(
    input_data_str: &str,
    is_response: bool,
) -> bindings::Result<()> {
    let (data, _num_handles) = test_util::parse_validation_test_input(input_data_str).unwrap();

    let message_view = bindings::MessageView::new(&data)?;

    if is_response {
        message_view.validate_response::<I>()
    } else {
        message_view.validate_request::<I>()
    }
}
