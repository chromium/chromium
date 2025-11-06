// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests parsing and deparsing of values.
//!
//! FOR_RELEASE: Fill this out.
//!
//! Note that this only focuses on testing the parsing/deparsing stage. We rely
//! on test_mojomparse.rs to verify that the translations to/from rust types
//! are accurate.
//!
//! The types in this file correspond to those defined in
//! //mojo/public/rust/test_mojom/parser_unittests.mojom

chromium::import! {
    "//mojo/public/rust/mojom_parser:mojom_parser_core";
    "//mojo/public/rust/mojom_parser:parser_unittests_rust";
    "//mojo/public/rust/mojom_parser:validation_parser";
}

use mojom_parser_core::*;
use parser_unittests_rust::parser_unittests::*;
use rust_gtest_interop::prelude::*;

// Verify that `value` serializes to the binary data represented by `data`,
// which is a string in the mojom validation text format, as defined in
// //mojo/public/cpp/bindings/tests/validation_test_input_parser.h
fn validate_parsing<T>(value: T, data: &str) -> anyhow::Result<()>
where
    T: MojomParse + PartialEq + std::fmt::Debug + Clone,
{
    // Helper function so we can use the question mark operator, but also
    // append context to it regardless of where we return.
    let validate_parsing_internal = || -> anyhow::Result<()> {
        let wire_data = validation_parser::parse(data)
            .map_err(anyhow::Error::msg)?
            // We currently don't do anything with handles, so only look at the data field
            .data;
        // FOR_RELEASE: It would be nice to use the `verify_` macros from googletest
        // that return a result, if we get access to them.
        expect_eq!(
            value,
            parse_single_value_for_testing(wire_data.as_ref(), T::wire_type())?.try_into()?
        );
        // FOR_RELEASE: We shouldn't need to clone here
        expect_eq!(
            wire_data.as_ref(),
            deparse_single_value_for_testing(&value.clone().into(), T::wire_type())?
        );
        Ok(())
    };

    // We could also use anyhow's Context trait, but that overwrites the old context
    // since we can't control the printing format gtest uses.
    validate_parsing_internal()
        .map_err(|err| anyhow::anyhow!("{}\nRust value: {:?}\nWire Data: {}", err, value, data))
}

/// Check that we correctly fail to parse mismatching data.
fn validate_parsing_failure<T>(data: &str) -> anyhow::Result<()>
where
    T: MojomParse + PartialEq + std::fmt::Debug + Clone,
{
    let wire_data = validation_parser::parse(data).map_err(anyhow::Error::msg)?.data;
    expect_true!(parse_single_value_for_testing(wire_data.as_ref(), T::wire_type()).is_err());
    Ok(())
}

#[gtest(MojomParser, TestPrimitiveParsing)]
fn test_primitives() -> anyhow::Result<()> {
    validate_parsing(7u8, "[u1]7")?;
    validate_parsing(0u8, "[u1]0")?;
    validate_parsing(255u8, "[u1]0xFF")?;
    validate_parsing(17u16, "[u2]17")?;
    validate_parsing(0u16, "[u2]0")?;
    validate_parsing(65535u16, "[u2]0xFFFF")?;
    validate_parsing(1234567u32, "[u4]1234567")?;
    validate_parsing(0u32, "[u4]0")?;
    validate_parsing(4294967295u32, "[u4]0xFFFFFFFF")?;
    validate_parsing(123456789101112u64, "[u8]123456789101112")?;
    validate_parsing(0u64, "[u8]0")?;
    validate_parsing(18446744073709551615u64, "[u8]0xFFFFFFFFFFFFFFFF")?;

    validate_parsing(-7i8, "[s1]-7")?;
    validate_parsing(0i8, "[s1]0")?;
    validate_parsing(127i8, "[s1]127")?;
    validate_parsing(-128i8, "[s1]-128")?;
    validate_parsing(-17i16, "[s2]-17")?;
    validate_parsing(0i16, "[s2]0")?;
    validate_parsing(32767i16, "[s2]32767")?;
    validate_parsing(-32768i16, "[s2]-32768")?;
    validate_parsing(-1234567i32, "[s4]-1234567")?;
    validate_parsing(0i32, "[s4]0")?;
    validate_parsing(2147483647i32, "[s4]2147483647")?;
    validate_parsing(-2147483648i32, "[s4]-2147483648")?;
    validate_parsing(-123456789101112i64, "[s8]-123456789101112")?;
    validate_parsing(0i64, "[s8]0")?;
    validate_parsing(9223372036854775807i64, "[s8]9223372036854775807")?;
    validate_parsing(-9223372036854775808i64, "[s8]-9223372036854775808")?;

    // Also make sure we correctly fail to parse things.
    // Since all bit patterns are valid, the only way for this to happen is if we
    // don't have enough data.
    validate_parsing_failure::<u16>("[u1]8")?;
    validate_parsing_failure::<i64>("[s4]1006")?;

    Ok(())
}

#[gtest(MojomParser, TestStructParsing)]
fn test_structs() -> anyhow::Result<()> {
    validate_parsing(Empty {}, "[u4]8 [u4]0")?;

    validate_parsing(
        FourInts { a: 10, b: -20, c: 400, d: -8000 },
        "[u4]24 [u4]0 [s1]10 [u1]0 [s2]-20 [s4]400 [s8]-8000",
    )?;

    validate_parsing(
        FourIntsReversed { a: 10, b: -20, c: 400, d: -8000 },
        "[u4]24 [u4]0 [s8]-8000 [s4]400 [s2]-20 [s1]10 [u1]0",
    )?;

    validate_parsing(
        FourIntsIntermixed { a: 10, b: 400, c: -20, d: -5 },
        "[u4]16 [u4]0 [s1]10 [s1]-5 [s2]-20 [s4]400",
    )?;

    let oncenested_val = OnceNested {
        f1: FourInts { a: 1, b: 2, c: 3, d: 4 },
        a: 13,
        b: 14,
        f2: FourIntsReversed { a: 5, b: 6, c: 7, d: 8 },
        f3: FourIntsIntermixed { a: 9, b: 10, c: 11, d: 12 },
        c: 15,
    };

    let oncenested_wire = "
        // OnceNested header
        [dist4]OnceNested_size [u4]0
        // f1
        [dist8]f1_ptr
        // a, b, c
        [u4]13 [s1]14 [u1]0 [s2]15
        // f2
        [dist8]f2_ptr
        // f3
        [dist8]f3_ptr
        [anchr]OnceNested_size

        [anchr]f1_ptr
        [u4]24 [u4]0 [s1]1 [u1]0 [s2]2 [s4]3 [s8]4

        [anchr]f2_ptr
        [u4]24 [u4]0 [s8]8 [s4]7 [s2]6 [s1]5 [u1]0

        [anchr]f3_ptr
        [u4]16 [u4]0 [s1]9 [s1]12 [s2]11 [s4]10
        ";

    validate_parsing(oncenested_val.clone(), oncenested_wire)?;

    let twicenested_val = TwiceNested {
        o: oncenested_val.clone(),
        a: 16,
        f: FourInts { a: 17, b: 18, c: 19, d: 20 },
        b: 21,
        c: 22,
    };

    let twicenested_wire = format!(
        "
        // TwiceNested header
        [dist4]TwiceNested_size [u4]0
        // o
        [dist8]o_ptr
        // a, b
        [s2]16 [s2]0 [s4]21
        // f
        [dist8]f_ptr
        // c
        [s4]22 [s4]0

        [anchr]TwiceNested_size

        [anchr]o_ptr
        {oncenested_wire}

        [anchr]f_ptr
        [u4]24 [u4]0 [s1]17 [u1]0 [s2]18 [s4]19 [s8]20
        "
    );

    validate_parsing(twicenested_val, &twicenested_wire)?;

    // Validate that struct parsing can fail in various ways

    // Wrong header size
    validate_parsing_failure::<Empty>("[u4]4 [u4]0")?;
    validate_parsing_failure::<Empty>("[u4]16 [u4]0")?;
    // Not 8-byte aligned
    validate_parsing_failure::<Empty>("[u4]9 [u4]0 [u1]0")?;

    // f1_ptr not 8-byte-aligned
    validate_parsing_failure::<OnceNested>(
        "
        // OnceNested header
        [dist4]OnceNested_size [u4]0
        // f1
        [dist8]f1_ptr
        // a, b, c
        [u4]13 [s1]14 [u1]0 [s2]15
        // f2
        [dist8]f2_ptr
        // f3
        [dist8]f3_ptr
        [anchr]OnceNested_size

        [u1]0
        [anchr]f1_ptr
        [u4]24 [u4]0 [s1]1 [u1]0 [s2]2 [s4]3 [s8]4

        [anchr]f2_ptr
        [u4]24 [u4]0 [s8]8 [s4]7 [s2]6 [s1]5 [u1]0

        [anchr]f3_ptr
        [u4]16 [u4]0 [s1]9 [s1]12 [s2]11 [s4]10
        ",
    )?;

    // f1_ptr invalidly null
    validate_parsing_failure::<OnceNested>(
        "
        // OnceNested header
        [dist4]OnceNested_size [u4]0
        // f1
        [u8]0 //f1_ptr
        // a, b, c
        [u4]13 [s1]14 [u1]0 [s2]15
        // f2
        [dist8]f2_ptr
        // f3
        [dist8]f3_ptr
        [anchr]OnceNested_size

        [u4]24 [u4]0 [s1]1 [u1]0 [s2]2 [s4]3 [s8]4

        [anchr]f2_ptr
        [u4]24 [u4]0 [s8]8 [s4]7 [s2]6 [s1]5 [u1]0

        [anchr]f3_ptr
        [u4]16 [u4]0 [s1]9 [s1]12 [s2]11 [s4]10
        ",
    )?;

    // f1_ptr in wrong place
    validate_parsing_failure::<OnceNested>(
        "
        // OnceNested header
        [dist4]OnceNested_size [u4]0
        // f1
        [dist8]f1_ptr
        // a, b, c
        [u4]13 [s1]14 [u1]0 [s2]15
        // f2
        [dist8]f2_ptr
        // f3
        [dist8]f3_ptr
        [anchr]OnceNested_size

        [u4]24 [u4]0 [s1]1 [u1]0 [s2]2 [s4]3 [s8]4
        [anchr]f1_ptr

        [anchr]f2_ptr
        [u4]24 [u4]0 [s8]8 [s4]7 [s2]6 [s1]5 [u1]0

        [anchr]f3_ptr
        [u4]16 [u4]0 [s1]9 [s1]12 [s2]11 [s4]10
        ",
    )?;

    Ok(())
}

#[gtest(MojomParser, TestBoolParsing)]
fn test_bools() -> anyhow::Result<()> {
    validate_parsing(
        TenBoolsAndAByte {
            b0: true,
            b1: true,
            b2: false,
            b3: false,
            b4: false,
            n1: 123,
            b5: true,
            b6: false,
            b7: true,
            // Byte boundary
            b8: false,
            b9: true,
        },
        "[u4]16 [u4]0 [b]10100011 [u1]123 [b]00000010 [u1]0 [u4]0",
    )?;

    validate_parsing(
        TenBoolsAndTwoBytes {
            b0: true,
            b1: false,
            b2: false,
            b3: false,
            b4: true,
            n1: 12345,
            b5: true,
            b6: false,
            b7: true,
            // Byte boundary
            b8: true,
            b9: false,
        },
        "[u4]16 [u4]0 [b]10110001 [b]00000001 [u2]12345 [u4]0",
    )?;

    Ok(())
}
