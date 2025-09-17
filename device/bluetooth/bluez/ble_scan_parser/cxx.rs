// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge(namespace=ble_scan_parser_bridge)]
pub mod ffi {
    // Note: this is only exposed for testing purposes.
    #[derive(Clone, Copy)]
    enum UuidFormat {
        // The UUID is the third and fourth bytes of a UUID with this pattern:
        // 0000____-0000-1000-8000-00805F9B34FB
        With16Bits,
        // The UUID is the first four bytes of a UUID with this pattern:
        // ________-0000-1000-8000-00805F9B34FB
        With32Bits,
        // The UUID is a standard UUID
        With128Bits,
    }

    unsafe extern "C++" {
        include!("device/bluetooth/bluez/ble_scan_parser/wrapper_functions.h");

        pub type ScanRecord;

        fn set_advertising_flags(record: Pin<&mut ScanRecord>, flags: i8);
        fn set_tx_power(record: Pin<&mut ScanRecord>, power: i8);
        fn set_advertisement_name(record: Pin<&mut ScanRecord>, name: &[u8]);
        fn add_service_uuid(record: Pin<&mut ScanRecord>, uuid: &[u8; 16]);
        fn add_service_data(record: Pin<&mut ScanRecord>, uuid: &[u8; 16], data: &[u8]);
        fn add_manufacturer_data(record: Pin<&mut ScanRecord>, company_code: u16, data: &[u8]);

        pub type UuidListBuilderForTest;
        fn add_uuid(self: Pin<&mut UuidListBuilderForTest>, uuid: &[u8; 16]);
    }

    extern "Rust" {
        fn parse(advertising_data: &[u8], record: Pin<&mut ScanRecord>) -> bool;
        fn parse_service_uuids_for_test(
            bytes: &[u8],
            format: UuidFormat,
            uuid_list_builder: Pin<&mut UuidListBuilderForTest>,
        ) -> bool;
        fn parse_uuid_for_test(bytes: &[u8], format: UuidFormat, out_uuid: &mut [u8; 16]) -> bool;
    }
}

use crate::parse;
use crate::parse_service_uuids_for_test;
use crate::parse_uuid_for_test;
