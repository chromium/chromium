// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Verifies that our custom header (de)serialization works as expected.
//!
//! Mojom headers are versioned, but the mojom value parser doesn't support
//! versioning yet. Therefore, we have a custom parsing function for headers;
//! this file tests it.
//!
//! Our testing strategy is to create structs that with the same fields as each
//! header version, pass them to the value parser, and compare the results with
//! our custom functions, accounting for the expected differences.
//!
//! Should we ever support versioning, this file can be deleted.

chromium::import! {
    "//mojo/public/rust/mojom_value_parser:mojom_value_parser";
}

use mojom_value_parser::{
    MessageHeader, MessageHeaderV1, MessageHeaderV2, MessageHeaderV3, MojomParse, ParsingResult,
};
use rust_gtest_interop::prelude::*;

// These structs represent exactly the contents of a header of the given
// version. The MojomParse derivation will output almost exactly the right
// serialized format, except the version number will always be 0.

#[derive(Debug, Default, Clone, MojomParse, PartialEq, Eq)]
struct HeaderV1 {
    interface_id: u32,
    name: u32,
    flags: u32,
    trace_nonce: u32,
    request_id: u64,
}

#[derive(Debug, Default, Clone, MojomParse, PartialEq, Eq)]
struct HeaderV2 {
    interface_id: u32,
    name: u32,
    flags: u32,
    trace_nonce: u32,
    request_id: u64,
    payload_ptr: u64,
    interface_ids_ptr: u64,
}

#[derive(Debug, Default, Clone, MojomParse, PartialEq, Eq)]
struct HeaderV3 {
    interface_id: u32,
    name: u32,
    flags: u32,
    trace_nonce: u32,
    request_id: u64,
    payload_ptr: u64,
    interface_ids_ptr: u64,
    creation_timeticks_us: i64,
}

// Each header type gets a its own (de)serialization functions, that use the
// MojomParse derivation but modify the version number in the encoded version.

impl HeaderV1 {
    fn serialize(self) -> Vec<u8> {
        let (mut serialized, _) = mojom_value_parser::serialize(self);
        // Byte 4 contains the version number. It will be 0 since the mojom
        // value parser doesn't handle versioning.
        serialized[4] = 1;
        serialized
    }

    fn deserialize(bytes: &mut [u8]) -> ParsingResult<Self> {
        bytes[4] = 0;
        mojom_value_parser::deserialize_exact(bytes, &mut [])
    }
}

impl HeaderV2 {
    fn serialize(self) -> Vec<u8> {
        let (mut serialized, _) = mojom_value_parser::serialize(self);
        serialized[4] = 2;
        serialized
    }

    fn deserialize(bytes: &mut [u8]) -> ParsingResult<Self> {
        bytes[4] = 0;
        mojom_value_parser::deserialize_exact(bytes, &mut [])
    }
}

impl HeaderV3 {
    fn serialize(self) -> Vec<u8> {
        let (mut serialized, _) = mojom_value_parser::serialize(self);
        serialized[4] = 3;
        serialized
    }

    fn deserialize(bytes: &mut [u8]) -> ParsingResult<Self> {
        bytes[4] = 0;
        mojom_value_parser::deserialize_exact(bytes, &mut [])
    }
}

#[gtest(RustMojomHeaderTest, HeaderV1)]
fn test_v1() {
    let test_header =
        HeaderV1 { interface_id: 1, name: 2, flags: 3, trace_nonce: 4, request_id: 5 };
    let msg_header = MessageHeader::V1(MessageHeaderV1 {
        interface_id: 1,
        name: 2,
        flags: 3,
        trace_nonce: 4,
        request_id: 5,
    });

    let mut ser1 = test_header.clone().serialize();
    let ser2 = msg_header.clone().serialize();
    assert_eq!(ser1, ser2);

    let des1 = HeaderV1::deserialize(&mut ser1).unwrap();
    let (rem, des2) = MessageHeader::deserialize(&ser2).unwrap();

    assert_eq!(test_header, des1);
    assert_eq!(msg_header, des2);
    assert!(rem.is_empty());
}

#[gtest(RustMojomHeaderTest, HeaderV2)]
fn test_v2() {
    let test_header = HeaderV2 {
        interface_id: 10,
        name: 20,
        flags: 30,
        trace_nonce: 40,
        request_id: 50,
        payload_ptr: 60,
        interface_ids_ptr: 70,
    };
    let msg_header = MessageHeader::V2(
        MessageHeaderV1 { interface_id: 10, name: 20, flags: 30, trace_nonce: 40, request_id: 50 },
        MessageHeaderV2 { payload_ptr: 60, interface_ids_ptr: 70 },
    );

    let mut ser1 = test_header.clone().serialize();
    let ser2 = msg_header.clone().serialize();
    assert_eq!(ser1, ser2);

    let des1 = HeaderV2::deserialize(&mut ser1).unwrap();
    let (rem, des2) = MessageHeader::deserialize(&ser2).unwrap();

    assert_eq!(test_header, des1);
    assert_eq!(msg_header, des2);
    assert!(rem.is_empty());
}

#[gtest(RustMojomHeaderTest, HeaderV3)]
fn test_v3() {
    let test_header = HeaderV3 {
        interface_id: 100,
        name: 200,
        flags: 300,
        trace_nonce: 400,
        request_id: 500,
        payload_ptr: 600,
        interface_ids_ptr: 700,
        creation_timeticks_us: 800,
    };
    let msg_header = MessageHeader::V3(
        MessageHeaderV1 {
            interface_id: 100,
            name: 200,
            flags: 300,
            trace_nonce: 400,
            request_id: 500,
        },
        MessageHeaderV2 { payload_ptr: 600, interface_ids_ptr: 700 },
        MessageHeaderV3 { creation_timeticks_us: 800 },
    );

    let mut ser1 = test_header.clone().serialize();
    let ser2 = msg_header.clone().serialize();
    assert_eq!(ser1, ser2);

    let des1 = HeaderV3::deserialize(&mut ser1).unwrap();
    let (rem, des2) = MessageHeader::deserialize(&ser2).unwrap();

    assert_eq!(test_header, des1);
    assert_eq!(msg_header, des2);
    assert!(rem.is_empty());
}
