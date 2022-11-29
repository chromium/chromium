// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests encoding and decoding functionality in the bindings package
//!
//! Test failure is defined as the function returning via panicking
//! and the result being caught in the test! macro. If a test function
//! returns without panicking, it is assumed to pass.

use mojo::bindings::mojom::MojomMessageOption;
use mojo::system;

use crate::util;
use crate::util::mojom_validation::*;

/// This macro is a wrapper for the tests! macro as it takes advantage of the
/// shared code between tests.
///
/// Given a test name, it will generate a test function. In this test function
/// we perform the following steps:
///   1. Decode the header of the validation input.
///   2. Decode the payload of the validation input, expecting a validation
///      error.
macro_rules! validation_tests {
    ($($name:ident => $req_type:ident;)*) => {
        tests! {
            $(
            fn $name() {
                let data = include_str!(concat!("../../interfaces/bindings/tests/data/validation/",
                                                stringify!($name),
                                                ".data"));
                let expected = include_str!(concat!("../../interfaces/bindings/tests/data/validation/",
                                                    stringify!($name),
                                                    ".expected")).trim();
                match util::parse_validation_test(data) {
                    Ok((data, num_handles)) => {
                        let mut mock_handles = Vec::with_capacity(num_handles);
                        for _ in 0..num_handles {
                            mock_handles.push(unsafe { system::acquire(0) });
                        }
                        match $req_type::decode_message(data, mock_handles) {
                            Ok(_) => panic!("Should not be valid!"),
                            Err(err) => assert_eq!(err.as_str(), expected),
                        }
                    },
                    Err(msg) => panic!("Error: {}", msg),
                }
            }
            )*
        }
    }
}

validation_tests! {
    conformance_empty => ConformanceTestInterfaceRequestOption;
    conformance_mthd0_incomplete_struct => ConformanceTestInterfaceRequestOption;
    conformance_mthd0_incomplete_struct_header => ConformanceTestInterfaceRequestOption;
    conformance_mthd0_invalid_request_flags => ConformanceTestInterfaceRequestOption;
    conformance_mthd0_invalid_request_flags2 => ConformanceTestInterfaceRequestOption;
    conformance_mthd0_struct_num_bytes_huge => ConformanceTestInterfaceRequestOption;
    conformance_mthd0_struct_num_bytes_less_than_min_requirement => ConformanceTestInterfaceRequestOption;
    conformance_mthd0_struct_num_bytes_less_than_struct_header => ConformanceTestInterfaceRequestOption;
    conformance_mthd10_null_keys => ConformanceTestInterfaceRequestOption;
    conformance_mthd10_null_values => ConformanceTestInterfaceRequestOption;
    conformance_mthd10_one_null_key => ConformanceTestInterfaceRequestOption;
    conformance_mthd10_unequal_array_size => ConformanceTestInterfaceRequestOption;
    conformance_mthd11_num_bytes_version_mismatch_1 => ConformanceTestInterfaceRequestOption;
    conformance_mthd11_num_bytes_version_mismatch_2 => ConformanceTestInterfaceRequestOption;
    conformance_mthd12_invalid_request_flags => ConformanceTestInterfaceRequestOption;
    conformance_mthd1_misaligned_struct => ConformanceTestInterfaceRequestOption;
    conformance_mthd1_struct_pointer_overflow => ConformanceTestInterfaceRequestOption;
    conformance_mthd1_unexpected_null_struct => ConformanceTestInterfaceRequestOption;
    conformance_mthd2_multiple_pointers_to_same_struct => ConformanceTestInterfaceRequestOption;
    conformance_mthd2_overlapped_objects => ConformanceTestInterfaceRequestOption;
    conformance_mthd2_wrong_layout_order => ConformanceTestInterfaceRequestOption;
    conformance_mthd3_array_num_bytes_huge => ConformanceTestInterfaceRequestOption;
    conformance_mthd3_array_num_bytes_less_than_array_header => ConformanceTestInterfaceRequestOption;
    conformance_mthd3_array_num_bytes_less_than_necessary_size => ConformanceTestInterfaceRequestOption;
    conformance_mthd3_array_pointer_overflow => ConformanceTestInterfaceRequestOption;
    conformance_mthd3_incomplete_array => ConformanceTestInterfaceRequestOption;
    conformance_mthd3_incomplete_array_header => ConformanceTestInterfaceRequestOption;
    conformance_mthd3_misaligned_array => ConformanceTestInterfaceRequestOption;
    conformance_mthd3_unexpected_null_array => ConformanceTestInterfaceRequestOption;
    conformance_mthd4_multiple_pointers_to_same_array => ConformanceTestInterfaceRequestOption;
    conformance_mthd4_overlapped_objects => ConformanceTestInterfaceRequestOption;
    conformance_mthd4_wrong_layout_order => ConformanceTestInterfaceRequestOption;
    conformance_mthd5_handle_out_of_range => ConformanceTestInterfaceRequestOption;
    conformance_mthd5_multiple_handles_with_same_value_1 => ConformanceTestInterfaceRequestOption;
    conformance_mthd5_multiple_handles_with_same_value_2 => ConformanceTestInterfaceRequestOption;
    conformance_mthd5_unexpected_invalid_handle => ConformanceTestInterfaceRequestOption;
    conformance_mthd5_wrong_handle_order => ConformanceTestInterfaceRequestOption;
    conformance_mthd6_nested_array_num_bytes_less_than_necessary_size => ConformanceTestInterfaceRequestOption;
    conformance_mthd7_unexpected_null_fixed_array => ConformanceTestInterfaceRequestOption;
    conformance_mthd7_unmatched_array_elements => ConformanceTestInterfaceRequestOption;
    conformance_mthd7_unmatched_array_elements_nested => ConformanceTestInterfaceRequestOption;
    conformance_mthd8_array_num_bytes_overflow => ConformanceTestInterfaceRequestOption;
    conformance_mthd8_unexpected_null_array => ConformanceTestInterfaceRequestOption;
    conformance_mthd8_unexpected_null_string => ConformanceTestInterfaceRequestOption;
    conformance_mthd9_unexpected_null_array => ConformanceTestInterfaceRequestOption;
    boundscheck_msghdr_no_such_method => BoundsCheckTestInterfaceRequestOption;
    conformance_msghdr_incomplete_struct => ConformanceTestInterfaceRequestOption;
    conformance_msghdr_incomplete_struct_header => ConformanceTestInterfaceRequestOption;
    conformance_msghdr_invalid_flag_combo => ConformanceTestInterfaceRequestOption;
    conformance_msghdr_missing_request_id => ConformanceTestInterfaceRequestOption;
    conformance_msghdr_no_such_method => ConformanceTestInterfaceRequestOption;
    conformance_msghdr_num_bytes_huge => ConformanceTestInterfaceRequestOption;
    conformance_msghdr_num_bytes_less_than_min_requirement => ConformanceTestInterfaceRequestOption;
    conformance_msghdr_num_bytes_less_than_struct_header => ConformanceTestInterfaceRequestOption;
    conformance_msghdr_num_bytes_version_mismatch_1 => ConformanceTestInterfaceRequestOption;
    conformance_msghdr_num_bytes_version_mismatch_2 => ConformanceTestInterfaceRequestOption;
    conformance_msghdr_num_bytes_version_mismatch_3 => ConformanceTestInterfaceRequestOption;
    resp_boundscheck_msghdr_no_such_method => BoundsCheckTestInterfaceResponseOption;
    resp_conformance_msghdr_invalid_response_flags1 => ConformanceTestInterfaceResponseOption;
    resp_conformance_msghdr_invalid_response_flags2 => ConformanceTestInterfaceResponseOption;
    resp_conformance_msghdr_no_such_method => ConformanceTestInterfaceResponseOption;
    integration_intf_resp_mthd0_unexpected_array_header => IntegrationTestInterfaceResponseOption;
    integration_intf_rqst_mthd0_unexpected_struct_header => IntegrationTestInterfaceRequestOption;
    integration_msghdr_invalid_flags => IntegrationTestInterfaceRequestOption;

    // Tests with missing data:
    //
    // conformance_mthd14_unexpected_null_array_in_union => ConformanceTestInterfaceRequestOption;
    // conformance_mthd14_unexpected_null_map_in_union => ConformanceTestInterfaceRequestOption;
    // conformance_mthd14_unexpected_null_struct_in_union => ConformanceTestInterfaceRequestOption;
    // conformance_mthd14_unexpected_null_union_in_union => ConformanceTestInterfaceRequestOption;
    // conformance_mthd15_unexpected_null_union_in_array => ConformanceTestInterfaceRequestOption;
}
