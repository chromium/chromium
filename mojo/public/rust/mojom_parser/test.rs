// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Provides unit tests for the parse and deparser
//!
//! Testing strategy: The existing tests for mojom bindings are end-to-end,
//! relying on mojom files to specify their inputs and outputs. However, we
//! want to unit-test the parser and deparser, so instead we manually specify
//! several mojom types and values directly as ASTs.
//!
//! The three parts of this crate that we test are the packing algorithm, the
//! parser, and the deparser. All the tests operate by calling the appropriate
//! function, and comparing against an expected output.

chromium::import! {
    "//mojo/public/rust/mojom_parser";
}

use rust_gtest_interop::prelude::*;

use anyhow::Result;
use mojom_parser::*;

/// Represents a type defined in a Mojom file.
///
/// Conceptually, it has three parts, corresponding to the three AST types:
/// 1. The type itself, as written in the Mojom file.
/// 2. The packed representation of the type (validated by <TODO>).
/// 3. A constructor for creating values of that type.
///
/// Unfortunately, the arguments to the constructor will vary based on the
/// MojomType it represents, so we can't write down a single type for the third
/// field. Instead, we must manually write a corresponding constructor for
/// every TestType, and track them by name.
struct TestType {
    base_type: MojomType,
    /// Human-readable type name for debugging output
    type_name: &'static str,
    /// Contents of a PackedStructuredType::Struct
    expected_packed_fields: Vec<(String, MojomWireType)>,
    // Imagine there's a third `constructor` entry here.
}

impl TestType {
    /// Return the packed version of this test type, with the given ordinal
    fn get_packed_type(&self, ordinal: Ordinal) -> MojomWireType {
        MojomWireType::Pointer {
            ordinal,
            nested_data_type: PackedStructuredType::Struct {
                packed_field_types: self.expected_packed_fields.clone(),
            },
        }
    }

    /// Ensure that the packed type actually matches the output of the packing
    /// algorithm
    fn validate(&self) {
        // FOR RELEASE: These assertion macros seem to print a massive (and utterly
        // useless) stack trace. See if we can turn that off.
        assert_eq!(
            pack_mojom_type(&self.base_type, 0),
            self.get_packed_type(0),
            "Type {} failed to pack correctly!",
            self.type_name
        );
    }

    fn parse(&self, data: &[u8]) -> Result<MojomValue> {
        let mut data = ParserData::new(data);
        let parsed_fields = parse_struct(&mut data, &self.expected_packed_fields)?;
        Ok(MojomValue::Struct(parsed_fields))
    }
}

// We'll be creating all our test types at global scope, so we can reference
// them from various testing functions.

// FOR_RELEASE: String field names are nice for debugging, but they shouldn't
// be in the final version because they're not necessary and they get partially
// lost during packing. So for now we'll just put in empty strings as dummies.

#[gtest(MojomParserTestSuit, BoolTest)]
fn test_bools() {
    // Used for probing how bools are packed. We should see the first 8 bools in a
    // bitfield, then the uint8, then another bitfield with the remaining two
    // bools.
    let ten_bools_and_a_byte_ty: TestType = TestType {
        type_name: "TenBoolsAndAByte",
        base_type: MojomType::Struct {
            fields: vec![
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::UInt8),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
            ],
        },
        expected_packed_fields: vec![
            (
                "".to_string(),
                MojomWireType::Bitfield {
                    ordinals: [
                        Some(0),
                        Some(1),
                        Some(2),
                        Some(3),
                        Some(4),
                        Some(6),
                        Some(7),
                        Some(8),
                    ],
                },
            ),
            ("".to_string(), MojomWireType::Leaf { ordinal: 5, leaf_type: PackedLeafType::UInt8 }),
            (
                "".to_string(),
                MojomWireType::Bitfield {
                    ordinals: [Some(9), Some(10), None, None, None, None, None, None],
                },
            ),
        ],
    };

    fn ten_bools_and_a_byte(
        e0: bool,
        e1: bool,
        e2: bool,
        e3: bool,
        e4: bool,
        e5: u8,
        e6: bool,
        e7: bool,
        e8: bool,
        e9: bool,
        e10: bool,
    ) -> MojomValue {
        MojomValue::Struct(vec![
            ("".to_string(), MojomValue::Bool(e0)),
            ("".to_string(), MojomValue::Bool(e1)),
            ("".to_string(), MojomValue::Bool(e2)),
            ("".to_string(), MojomValue::Bool(e3)),
            ("".to_string(), MojomValue::Bool(e4)),
            ("".to_string(), MojomValue::UInt8(e5)),
            ("".to_string(), MojomValue::Bool(e6)),
            ("".to_string(), MojomValue::Bool(e7)),
            ("".to_string(), MojomValue::Bool(e8)),
            ("".to_string(), MojomValue::Bool(e9)),
            ("".to_string(), MojomValue::Bool(e10)),
        ])
    }

    // Same as TenBoolsAndAByte, but now we have a 16-bit int so the bitfield
    // for the latter two bools is packed before the uint16.
    let ten_bools_and_two_bytes_ty: TestType = TestType {
        type_name: "TenBoolsAndTwoBytes",
        base_type: MojomType::Struct {
            fields: vec![
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::UInt16),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
                ("".to_string(), MojomType::Bool),
            ],
        },
        expected_packed_fields: vec![
            (
                "".to_string(),
                MojomWireType::Bitfield {
                    ordinals: [
                        Some(0),
                        Some(1),
                        Some(2),
                        Some(3),
                        Some(4),
                        Some(6),
                        Some(7),
                        Some(8),
                    ],
                },
            ),
            (
                "".to_string(),
                MojomWireType::Bitfield {
                    ordinals: [Some(9), Some(10), None, None, None, None, None, None],
                },
            ),
            ("".to_string(), MojomWireType::Leaf { ordinal: 5, leaf_type: PackedLeafType::UInt16 }),
        ],
    };

    fn ten_bools_and_two_bytes(
        e0: bool,
        e1: bool,
        e2: bool,
        e3: bool,
        e4: bool,
        e5: u16,
        e6: bool,
        e7: bool,
        e8: bool,
        e9: bool,
        e10: bool,
    ) -> MojomValue {
        MojomValue::Struct(vec![
            ("".to_string(), MojomValue::Bool(e0)),
            ("".to_string(), MojomValue::Bool(e1)),
            ("".to_string(), MojomValue::Bool(e2)),
            ("".to_string(), MojomValue::Bool(e3)),
            ("".to_string(), MojomValue::Bool(e4)),
            ("".to_string(), MojomValue::UInt16(e5)),
            ("".to_string(), MojomValue::Bool(e6)),
            ("".to_string(), MojomValue::Bool(e7)),
            ("".to_string(), MojomValue::Bool(e8)),
            ("".to_string(), MojomValue::Bool(e9)),
            ("".to_string(), MojomValue::Bool(e10)),
        ])
    }

    ten_bools_and_a_byte_ty.validate();
    ten_bools_and_two_bytes_ty.validate();

    // Test parsing and deparsing

    // 87 cd 02
    let one_byte_val =
        ten_bools_and_a_byte(true, true, true, false, false, 0xcd, false, false, true, false, true);

    let one_byte_data: [u8; 16] = [
        0x10, 0x00, 0x00, 0x00, // Header: Size in bytes (16)
        0x00, 0x00, 0x00, 0x00, // Header: Version number (0)
        0x87, // Bitfield 1
        0xcd, // One Byte
        0x02, // Bitfield 2
        0x00, 0x00, 0x00, 0x00, 0x00, // Padding
    ];

    // ad 03 ef cd
    let two_byte_val = ten_bools_and_two_bytes(
        true, false, true, true, false, 0xcdef, true, false, true, true, true,
    );
    let two_byte_data: [u8; 16] = [
        0x10, 0x00, 0x00, 0x00, // Header: Size in bytes (16)
        0x00, 0x00, 0x00, 0x00, // Header: Version number (0)
        0xad, // Bitfield 1
        0x03, // Bitfield 2
        0xef, 0xcd, // Two Bytes
        0x00, 0x00, 0x00, 0x00, // Padding
    ];

    // FOR RELEASE: there's gotta be a better way to hand things that return a
    // result in a way that gtest will catch.
    assert_eq!(one_byte_val, ten_bools_and_a_byte_ty.parse(&one_byte_data).unwrap());
    assert_eq!(two_byte_val, ten_bools_and_two_bytes_ty.parse(&two_byte_data).unwrap())
}
