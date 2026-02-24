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
    "//mojo/public/rust/mojom_value_parser:mojom_value_parser_core";
    "//mojo/public/rust/mojom_value_parser:parser_unittests_rust";
    "//mojo/public/rust/mojom_value_parser:validation_parser";
}

use mojom_value_parser_core::*;
use ordered_float::OrderedFloat;
use parser_unittests_rust::parser_unittests::*;
use rust_gtest_interop::prelude::*;

use crate::helpers::*;

// Verify that `value` serializes to the binary data represented by `data`,
// which is a string in the mojom validation text format, as defined in
// //mojo/public/cpp/bindings/tests/validation_test_input_parser.h
fn validate_parsing<T>(value: T, data: &str) -> anyhow::Result<()>
where
    T: MojomParse + PartialEq + std::fmt::Debug,
{
    // We have to compute this eagerly since `value ` will get consumed by the test
    let err_str = format!("\nRust value: {value:?}\nWire Data: {data}");

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
            &value,
            &parse_single_value_for_testing(wire_data.as_ref(), &mut [], T::wire_type())?
                .try_into()?
        );
        expect_eq!(
            wire_data.as_ref(),
            *deparse_single_value_for_testing(value.into(), T::wire_type())?
        );
        Ok(())
    };

    // We could also use anyhow's Context trait, but that overwrites the old context
    // since we can't control the printing format gtest uses.
    validate_parsing_internal().map_err(|err| anyhow::anyhow!("{err}{err_str}"))
}

// Similar to `validate_parsing`, but for types with handles. It takes the
// number of handles the type should contain, and creates that many dummy
// handles for the deparsing process. It also does a slightly more relaxed check
// after parsing (comparing the parsed `MojomValue` instead of the parsed `T`).
fn validate_parsing_with_handles<T>(value: T, data: &str, num_handles: usize) -> anyhow::Result<()>
where
    T: MojomParse + PartialEq + std::fmt::Debug,
{
    // We have to compute this eagerly since `value ` will get consumed by the test
    let err_str = format!("\nRust value: {value:?}\nWire Data: {data}");

    // Helper function so we can use the question mark operator, but also
    // append context to it regardless of where we return.
    let validate_parsing_internal = || -> anyhow::Result<()> {
        let wire_data = validation_parser::parse(data)
            .map_err(anyhow::Error::msg)?
            // We currently don't do anything with handles, so only look at the data field
            .data;

        let mojom_value: MojomValue = value.into();
        let mut handles = (0..num_handles).map(|_| Some(dummy_handle())).collect::<Vec<_>>();

        // FOR_RELEASE: It would be nice to use the `verify_` macros from googletest
        // that return a result, if we get access to them.
        expect_true!(equivalent_value(
            &mojom_value,
            &parse_single_value_for_testing(wire_data.as_ref(), &mut handles, T::wire_type())?
        ));
        // All the handles in `handles` should have been consumed during parsing
        expect_true!(handles.into_iter().all(|opt| opt.is_none()));
        expect_eq!(
            wire_data.as_ref(),
            *deparse_single_value_for_testing(mojom_value, T::wire_type())?
        );
        Ok(())
    };

    // We could also use anyhow's Context trait, but that overwrites the old context
    // since we can't control the printing format gtest uses.
    validate_parsing_internal().map_err(|err| anyhow::anyhow!("{err}{err_str}"))
}

/// Check that we correctly fail to parse mismatching data.
fn validate_parsing_failure<T>(data: &str) -> anyhow::Result<()>
where
    T: MojomParse + PartialEq + std::fmt::Debug,
{
    validate_parsing_failure_with_handles::<T>(data, 0)
}

/// Check that we correctly fail to parse mismatching data...with handles!
fn validate_parsing_failure_with_handles<T>(data: &str, num_handles: usize) -> anyhow::Result<()>
where
    T: MojomParse + PartialEq + std::fmt::Debug,
{
    let wire_data = validation_parser::parse(data).map_err(anyhow::Error::msg)?.data;
    let mut handles = (0..num_handles).map(|_| Some(dummy_handle())).collect::<Vec<_>>();
    expect_true!(
        parse_single_value_for_testing(wire_data.as_ref(), &mut handles, T::wire_type()).is_err()
    );
    Ok(())
}

#[gtest(RustTestMojomParsing, TestPrimitiveParsing)]
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

    validate_parsing(3.14f32, "[f]3.14")?;
    validate_parsing(0.0f32, "[f]0.0")?;
    validate_parsing(-1.0f32, "[f]-1.0")?;

    validate_parsing(2.71828f64, "[d]2.71828")?;
    validate_parsing(0.0f64, "[d]0.0")?;
    validate_parsing(-123.456f64, "[d]-123.456")?;

    // Also make sure we correctly fail to parse things.
    // Since all bit patterns are valid, the only way for this to happen is if we
    // don't have enough data.
    validate_parsing_failure::<u16>("[u1]8")?;
    validate_parsing_failure::<i64>("[s4]1006")?;
    validate_parsing_failure::<f64>("[s4]1006")?;

    Ok(())
}

#[gtest(RustTestMojomParsing, TestStructParsing)]
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

#[gtest(RustTestMojomParsing, TestBoolParsing)]
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

#[gtest(RustTestMojomParsing, TestEnumParsing)]
fn test_enums() -> anyhow::Result<()> {
    validate_parsing(
        SomeEnums { e1: TestEnum::Seven, n1: 98765, e2: TestEnum2::FourtyTwo },
        "[u4]24 [u4]0 [u4]7 [u4]42 [u8]98765",
    )?;

    validate_parsing_failure::<SomeEnums>("[u4]24 [u4]0 [u4]5 [u4]42 [u8]98765")?;
    validate_parsing_failure::<SomeEnums>("[u4]24 [u4]0 [u4]7 [u4]99 [u8]98765")?;

    Ok(())
}

#[gtest(RustTestMojomParsing, TestUnionParsing)]
fn test_unions() -> anyhow::Result<()> {
    // BaseUnion
    validate_parsing(BaseUnion::n1(10), "[u4]16 [u4]0 [u8]10")?;
    validate_parsing(BaseUnion::u1(987654321), "[u4]16 [u4]1 [u8]987654321")?;
    validate_parsing(BaseUnion::e1(TestEnum::Three), "[u4]16 [u4]2 [u8]3")?;
    validate_parsing(BaseUnion::b1(false), "[u4]16 [u4]3 [u8]0")?;
    validate_parsing(BaseUnion::b2(true), "[u4]16 [u4]4 [u8]1")?;
    validate_parsing(
        BaseUnion::em1(Empty {}),
        "[u4]16 [u4]5 [dist8]em1_ptr [anchr]em1_ptr [u4]8 [u4]0",
    )?;
    validate_parsing(
        BaseUnion::f1(FourInts { a: 5, b: 6, c: 7, d: 8 }),
        concat!(
            "[u4]16 [u4]6 [dist8]f1_ptr ",                              // BaseUnion
            "[anchr]f1_ptr [u4]24 [u4]0 [s1]5 [u1]0 [s2]6 [s4]7 [s8]8", // FourInts
        ),
    )?;
    validate_parsing(BaseUnion::fl(3.14f32), "[u4]16 [u4]7 [f]3.14 [u4]0")?;

    validate_parsing_failure::<BaseUnion>("[u4]16 [u4]99 [u8]0")?;
    validate_parsing_failure::<BaseUnion>("[u4]16 [u4]6 [u8]0")?;

    // NestedUnion
    validate_parsing(NestedUnion::n(99), "[u4]16 [u4]0 [u8]99")?;
    validate_parsing(
        NestedUnion::u(BaseUnion::e1(TestEnum::Three)),
        "[u4]16 [u4]1 [dist8]u_ptr [anchr]u_ptr [u4]16 [u4]2 [u8]3",
    )?;
    validate_parsing(
        NestedUnion::u(BaseUnion::f1(FourInts { a: 1, b: 2, c: 3, d: 4 })),
        concat!(
            "[u4]16 [u4]1 [dist8]u_ptr ",               // NestedUnion
            "[anchr]u_ptr [u4]16 [u4]6 [dist8]f1_ptr ", // BaseUnion
            "[anchr]f1_ptr [u4]24 [u4]0 [s1]1 [u1]0 [s2]2 [s4]3 [s8]4", // FourInts
        ),
    )?;

    // Should be encoded as a pointer
    validate_parsing_failure::<NestedUnion>("[u4]16 [u4]1 [u4]16 [u4]2 [u8]3")?;

    // WithNestedUnion
    validate_parsing(
        WithNestedUnion { n1: 17, u: NestedUnion::n(18), n2: 19 },
        concat!(
            "[u4]40 [u4]0 [s8]17 ", // WithNestedUnion Header + n1
            "[u4]16 [u4]0 [u8]18 ", // NestedUnion
            "[u4]19 [u4]0",         // n2
        ),
    )?;
    validate_parsing(
        WithNestedUnion { n1: 99, u: NestedUnion::u(BaseUnion::b1(true)), n2: 100 },
        concat!(
            "[u4]40 [u4]0 [s8]99 ",            // WithNestedUnion Header + n1
            "[u4]16 [u4]1 [dist8]u_ptr ",      // NestedUnion
            "[u4]100 [u4]0 ",                  // n2
            "[anchr]u_ptr [u4]16 [u4]3 [u8]1", // BaseUnion
        ),
    )?;

    // NestederUnion
    validate_parsing(NestederUnion::b(false), "[u4]16 [u4]0 [u8]0")?;
    validate_parsing(NestederUnion::n(-1), "[u4]16 [u4]1 [s1]-1 [u1]0 [u2]0 [u4]0")?;
    validate_parsing(
        NestederUnion::u(NestedUnion::n(123)),
        "[u4]16 [u4]2 [dist8]u_ptr [anchr]u_ptr [u4]16 [u4]0 [u8]123",
    )?;
    validate_parsing(
        NestederUnion::u(NestedUnion::u(BaseUnion::b2(true))),
        concat!(
            "[u4]16 [u4]2 [dist8]u_ptr ",               // NestederUnion
            "[anchr]u_ptr [u4]16 [u4]1 [dist8]u_ptr2 ", // NestedUnion
            "[anchr]u_ptr2 [u4]16 [u4]4 [u8]1",         // BaseUnion
        ),
    )?;
    validate_parsing(
        NestederUnion::w(WithNestedUnion { n1: 1000, u: NestedUnion::n(-50), n2: 200 }),
        concat!(
            "[u4]16 [u4]3 [dist8]w_ptr ",          // NestederUnion
            "[anchr]w_ptr [u4]40 [u4]0 [s8]1000 ", // WithNestedUnion Header + n1
            "[u4]16 [u4]0 [s4]-50 [u4]0 ",         // NestedUnion
            "[u4]200 [u4]0",                       // n2
        ),
    )?;
    validate_parsing(
        NestederUnion::w(WithNestedUnion {
            n1: 999,
            u: NestedUnion::u(BaseUnion::f1(FourInts { a: 101, b: -102, c: 103, d: -104 })),
            n2: -211,
        }),
        concat!(
            "[u4]16 [u4]3 [dist8]w_ptr ",               // NestederUnion
            "[anchr]w_ptr [u4]40 [u4]0 [s8]999 ",       // WithNestedUnion Header + n1
            "[u4]16 [u4]1 [dist8]u_ptr ",               // NestedUnion
            "[s4]-211 [u4]0 ",                          // n2
            "[anchr]u_ptr [u4]16 [u4]6 [dist8]f1_ptr ", // BaseUnion
            "[anchr]f1_ptr [u4]24 [u4]0 [s1]101 [u1]0 [s2]-102 [s4]103 [s8]-104"  // FourInts
        ),
    )?;

    // WithManyUnions
    validate_parsing(
        WithManyUnions {
            u1: NestedUnion::n(50),
            i1: 11,
            u2: NestederUnion::b(false),
            d1: 3.14159,
            u3: BaseUnion::n1(55),
            u4: NestederUnion::n(12),
            i2: 33,
            i3: 44,
        },
        concat!(
            "[u4]96 [u4]0 ",              // WithManyUnions Header
            "[u4]16 [u4]0 [u8]50 ",       // u1
            "[s1]11 [u1]0 [u2]0 [s4]33 ", // i1, padding, i2
            "[u4]16 [u4]0 [u8]0 ",        // u2
            "[d]3.14159 ",                // d1
            "[u4]16 [u4]0 [u8]55 ",       // u3
            "[u4]16 [u4]1 [u8]12 ",       // u4
            "[s4]44 [u4]0",               // i3, padding
        ),
    )?;

    // ALL the nesting!
    validate_parsing(
        WithManyUnions {
            u1: NestedUnion::u(BaseUnion::b1(true)),
            i1: -1,
            u2: NestederUnion::u(NestedUnion::u(BaseUnion::e1(TestEnum::Seven))),
            d1: 2.71828,
            u3: BaseUnion::b2(false),
            u4: NestederUnion::w(WithNestedUnion {
                n1: 2000,
                u: NestedUnion::u(BaseUnion::f1(FourInts { a: 12, b: 13, c: 14, d: 15 })),
                n2: 3000,
            }),
            i2: -3,
            i3: -4,
        },
        concat!(
            "[u4]96 [u4]0 ",                                    // WithManyUnions Header
            "[u4]16 [u4]1 [dist8]u1_ptr ",                      // NestedUnion::u
            "[s1]-1 [u1]0 [u2]0 [s4]-3 ",                       // i1, padding, i2
            "[u4]16 [u4]2 [dist8]u2_ptr ",                      // NestederUnion::u
            "[d]2.71828 ",                                      // d1
            "[u4]16 [u4]4 [u8]0 ",                              // BaseUnion::b2(false)
            "[u4]16 [u4]3 [dist8]u4_ptr ",                      // NestederUnion::w
            "[s4]-4 [u4]0 ",                                    // i3, padding
            "[anchr]u1_ptr [u4]16 [u4]3 [u8]1 ",                // BaseUnion::b1(true) - u1.u
            "[anchr]u2_ptr [u4]16 [u4]1 [dist8]u2_u_ptr ",      // NestedUnion::u - u2.u
            "[anchr]u2_u_ptr [u4]16 [u4]2 [u8]7 ",              // BaseUnion::e1(...) - u2.u.e1
            "[anchr]u4_ptr [u4]40 [u4]0 [s8]2000 ",             // WithNestedUnion Header + n1
            "[u4]16 [u4]1 [dist8]u4_w_u_ptr ",                  // NestedUnion::u - u4.w.u
            "[s4]3000 [u4]0 ",                                  // WithNestedUnion.n2
            "[anchr]u4_w_u_ptr [u4]16 [u4]6 [dist8]u4_f1_ptr ", // BaseUnion::f1 - u4.w.u.u
            "[anchr]u4_f1_ptr [u4]24 [u4]0 [s1]12 [u1]0 [s2]13 [s4]14 [s8]15"  // FourInts
        ),
    )?;

    Ok(())
}

#[gtest(RustTestMojomParsing, TestArrayParsing)]
fn test_array_parsing() -> anyhow::Result<()> {
    // array<int16>
    validate_parsing::<Vec<i16>>(
        vec![1, -2, 3, -4, 5],
        "[u4]18 [u4]5 [s2]1 [s2]-2 [s2]3 [s2]-4 [s2]5 [u2]0 [u4]0",
    )?;

    validate_parsing::<Vec<i16>>(vec![], "[u4]8 [u4]0")?;

    // array<uint64, 3>
    validate_parsing::<[u64; 3]>([5, 6, 7], "[u4]32 [u4]3 [u8]5 [u8]6 [u8]7")?;
    // Wrong number of elements
    validate_parsing_failure::<[u64; 3]>("[u4]32 [u4]4 [u8]5 [u8]6 [u8]7 [u8]8")?;

    // array<bool>
    validate_parsing::<Vec<bool>>(
        vec![true, true, false, true, false, false, true, false, true],
        "[u4]10 [u4]9 [b]01001011 [b]00000001 [u2]0 [u4]0",
    )?;

    // array<bool, 20>
    validate_parsing::<[bool; 20]>(
        [
            false, true, true, false, true, true, false, true, true, false, true, true, false,
            true, true, false, true, true, false, true,
        ],
        "[u4]11 [u4]20 [b]10110110 [b]01101101 [b]00001011 [u1]0 [u4]0",
    )?;

    // array<float>
    validate_parsing::<Vec<f32>>(
        vec![1.0, -2.5, 3.14],
        "[u4]20 [u4]3 [f]1.0 [f]-2.5 [f]3.14 [u4]0",
    )?;

    // array<TestEnum>
    validate_parsing::<Vec<TestEnum>>(
        vec![TestEnum::Zero, TestEnum::Four, TestEnum::Seven],
        "[u4]20 [u4]3 [u4]0 [u4]4 [u4]7 [u4]0",
    )?;

    // Bad enum value
    validate_parsing_failure::<Vec<TestEnum>>("[u4]24 [u4]3 [u4]0 [u4]99 [u4]4")?;

    // array<BaseUnion>
    validate_parsing::<Vec<BaseUnion>>(
        vec![BaseUnion::n1(10), BaseUnion::u1(20), BaseUnion::e1(TestEnum::Three)],
        concat!(
            "[u4]56 [u4]3 ", // Array header
            "[u4]16 [u4]0 [u8]10 ",
            "[u4]16 [u4]1 [u8]20 ",
            "[u4]16 [u4]2 [u8]3",
        ),
    )?;

    // Bad union value in array
    validate_parsing_failure::<Vec<BaseUnion>>(concat!(
        "[u4]48 [u4]3 ", // Array header
        "[u4]16 [u4]0 [u8]10 ",
        "[u4]16 [u4]99 [u8]20 ", // Invalid discriminant
        "[u4]16 [u4]2 [u8]3",
    ))?;

    // array<FourInts>
    validate_parsing::<Vec<FourInts>>(
        vec![FourInts { a: 1, b: 2, c: 3, d: 4 }, FourInts { a: 5, b: 6, c: 7, d: 8 }],
        concat!(
            "[u4]24 [u4]2 ", // Array header
            "[dist8]fourints_0_ptr ",
            "[dist8]fourints_1_ptr ",
            "[anchr]fourints_0_ptr ",
            "[u4]24 [u4]0 [s1]1 [u1]0 [s2]2 [s4]3 [s8]4 ", // FourInts 0
            "[anchr]fourints_1_ptr ",
            "[u4]24 [u4]0 [s1]5 [u1]0 [s2]6 [s4]7 [s8]8", // FourInts 1
        ),
    )?;

    // array<array<uint8>>
    validate_parsing::<Vec<Vec<u8>>>(
        vec![vec![1, 2], vec![3, 4, 5]],
        concat!(
            "[u4]24 [u4]2 ", // Array header
            "[dist8]nested_0_ptr ",
            "[dist8]nested_1_ptr ",
            "[anchr]nested_0_ptr ",
            "[u4]10 [u4]2 [u1]1 [u1]2 [u2]0 [u4]0 ", // Inner array 0
            "[anchr]nested_1_ptr ",
            "[u4]11 [u4]3 [u1]3 [u1]4 [u1]5 [u1]0 [u4]0", // Inner array 1
        ),
    )?;

    // array<array<uint8, 2>, 3>
    validate_parsing::<[[u8; 2]; 3]>(
        [[6, 7], [8, 9], [10, 11]],
        concat!(
            "[u4]32 [u4]3 ", // Outer array header
            "[dist8]nested_sized_0_ptr ",
            "[dist8]nested_sized_1_ptr ",
            "[dist8]nested_sized_2_ptr ",
            "[anchr]nested_sized_0_ptr ",
            "[u4]10 [u4]2 [u1]6 [u1]7 [u2]0 [u4]0 ", // Inner array 0
            "[anchr]nested_sized_1_ptr ",
            "[u4]10 [u4]2 [u1]8 [u1]9 [u2]0 [u4]0 ", // Inner array 1
            "[anchr]nested_sized_2_ptr ",
            "[u4]10 [u4]2 [u1]10 [u1]11 [u2]0 [u4]0", // Inner array 2
        ),
    )?;

    // array<NestedUnion>
    validate_parsing::<Vec<NestedUnion>>(
        vec![NestedUnion::n(30), NestedUnion::u(BaseUnion::n1(40)), NestedUnion::n(50)],
        concat!(
            "[u4]56 [u4]3 ", // Array header
            "[u4]16 [u4]0 [u8]30 ",
            "[u4]16 [u4]1 [dist8]nested_union_1_ptr ",
            "[u4]16 [u4]0 [u8]50 ",
            "[anchr]nested_union_1_ptr ",
            "[u4]16 [u4]0 [u8]40",
        ),
    )?;

    Ok(())
}

// Note that tests involving maps are especially tricky, because semantically
// the order of (key, value) pairs doesn't matter, but since we do exact
// comparisons it matters when we specify the wire data here.
// Our implementation uses a BTreeMap to guarantee that we always serialize in
// a sorted order.
#[gtest(RustTestMojomParsing, TestMapParsing)]
fn test_map_parsing() -> anyhow::Result<()> {
    use std::collections::HashMap;

    validate_parsing::<HashMap<i8, i8>>(
        [(1, 10), (3, 40), (8, 99)].into(),
        concat!(
            // Map header
            "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            // keys array
            "[anchr]keys_ptr ",
            "[u4]11 [u4]3 ", // Keys header: size, num_elements
            "[s1]1 [s1]3 [s1]8 [u1]0 [u4]0 ",
            // values array
            "[anchr]values_ptr ",
            "[u4]11 [u4]3 ", // Values header: size, num_elements
            "[s1]10 [s1]40 [s1]99 [u1]0 [u4]0 ",
        ),
    )?;

    // Empty maps are fine
    validate_parsing::<HashMap<i8, i8>>(
        [].into(),
        concat!(
            // Map struct
            "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            // keys array
            "[anchr]keys_ptr [u4]8 [u4]0 ",
            // values array
            "[anchr]values_ptr [u4]8 [u4]0 ",
        ),
    )?;

    // Mismatched sizes are not fine
    validate_parsing_failure::<HashMap<i8, i8>>(concat!(
        // Map struct
        "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
        // keys array
        "[anchr]keys_ptr [u4]9 [u4]1 ",
        "[s1]1 [s1]0 [s2]0 [s4]0 ",
        // values array
        "[anchr]values_ptr [u4]8 [u4]0 ",
    ))?;
    validate_parsing_failure::<HashMap<i8, i8>>(concat!(
        // Map struct
        "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
        // keys array
        "[anchr]keys_ptr [u4]9 [u4]1 ",
        "[s1]1 [s1]0 [s2]0 [s4]0 ",
        // values array
        "[anchr]values_ptr [u4]10 [u4]2 ",
        "[s1]2 [s1]3 [s2]0 [s4]0 ",
    ))?;

    // Nor are duplicate keys
    validate_parsing_failure::<HashMap<i8, i8>>(concat!(
        // Map struct
        "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
        // keys array
        "[anchr]keys_ptr [u4]10 [u4]2 ",
        "[s1]1 [s1]1 [s2]0 [s4]0 ",
        // values array
        "[anchr]values_ptr [u4]10 [u4]2 ",
        "[s1]2 [s1]3 [s2]0 [s4]0 ",
    ))?;
    validate_parsing_failure::<HashMap<OrderedFloat<f64>, i8>>(concat!(
        // Map struct
        "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
        // keys array
        "[anchr]keys_ptr [u4]24 [u4]2 ",
        "[d]1.233e-4 [d]1.233e-4 ",
        // values array
        "[anchr]values_ptr [u4]10 [u4]2 ",
        "[s1]2 [s1]3 [s2]0 [s4]0 ",
    ))?;

    validate_parsing::<HashMap<bool, u16>>(
        [(false, 1020), (true, 3040)].into(),
        concat!(
            // Map header
            "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            // keys array
            "[anchr]keys_ptr ",
            "[u4]9 [u4]2 [b]00000010 [u1]0 [u2]0 [u4]0 ",
            // values array
            "[anchr]values_ptr ",
            "[u4]12 [u4]2 [u2]1020 [u2]3040 [u4]0",
        ),
    )?;

    validate_parsing::<HashMap<OrderedFloat<f32>, i32>>(
        [(1.1f32.into(), 10), (2.2f32.into(), 20)].into(),
        concat!(
            // Map header
            "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            // keys array
            "[anchr]keys_ptr ",
            "[u4]16 [u4]2 [f]1.1 [f]2.2 ",
            // values array
            "[anchr]values_ptr ",
            "[u4]16 [u4]2 [s4]10 [s4]20 ",
        ),
    )?;

    validate_parsing::<HashMap<TestEnum, i32>>(
        [(TestEnum::Seven, -2), (TestEnum::Four, -3), (TestEnum::Zero, -1), (TestEnum::Three, -3)]
            .into(),
        concat!(
            // Map header
            "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            // keys array
            "[anchr]keys_ptr ",
            // Note that keys are sorted in ascending order
            "[u4]24 [u4]4 [u4]0 [u4]3 [u4]4 [u4]7 ",
            // values array
            "[anchr]values_ptr ",
            // And the values are sorted to match
            "[u4]24 [u4]4 [s4]-1 [s4]-3 [s4]-3 [s4]-2 ",
        ),
    )?;

    validate_parsing::<HashMap<i8, FourInts>>(
        [(1, FourInts { a: 1, b: 2, c: 3, d: 4 }), (5, FourInts { a: 5, b: 6, c: 7, d: 8 })].into(),
        concat!(
            // Map header
            "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            // keys array
            "[anchr]keys_ptr [u4]10 [u4]2 [s1]1 [s1]5 [u2]0 [u4]0 ",
            // values array
            "[anchr]values_ptr ",
            "[u4]24 [u4]2 [dist8]v0 [dist8]v1 ",
            "[anchr]v0 [u4]24 [u4]0 [s1]1 [u1]0 [s2]2 [s4]3 [s8]4 ",
            "[anchr]v1 [u4]24 [u4]0 [s1]5 [u1]0 [s2]6 [s4]7 [s8]8 ",
        ),
    )?;

    validate_parsing::<HashMap<i8, NestedUnion>>(
        [
            (-1, NestedUnion::n(10)),
            (-2, NestedUnion::u(BaseUnion::e1(TestEnum::Seven))),
            (-3, NestedUnion::n(23)),
        ]
        .into(),
        concat!(
            // Map header
            "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            // keys array
            "[anchr]keys_ptr [u4]11 [u4]3 [s1]-3 [s1]-2 [s1]-1 [s1]0 [u4]0 ",
            // values array
            "[anchr]values_ptr ",
            "[u4]56 [u4]3 ",
            "[u4]16 [u4]0 [u8]23 ",
            "[u4]16 [u4]1 [dist8]v1 ",
            "[u4]16 [u4]0 [u8]10 ",
            "[anchr]v1 [u4]16 [u4]2 [u8]7",
        ),
    )?;

    validate_parsing::<HashMap<i8, HashMap<i16, u32>>>(
        [(1, [(10, 100), (20, 200)].into()), (2, [(30, 300), (40, 400), (50, 500)].into())].into(),
        concat!(
            // Toplevel map header
            "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            // Toplevel keys array
            "[anchr]keys_ptr [u4]10 [u4]2 [s1]1 [s1]2 [u2]0 [u4]0 ",
            // Toplevel values array
            "[anchr]values_ptr ",
            "[u4]24 [u4]2 [dist8]v0 [dist8]v1 ",
            // First nested map
            "[anchr]v0 [u4]24 [u4]0 [dist8]v0_keys [dist8]v0_values ",
            "[anchr]v0_keys [u4]12 [u4]2 [s2]10 [s2]20 [u4]0 ",
            "[anchr]v0_values [u4]16 [u4]2 [u4]100 [u4]200 ",
            // Second nested map
            "[anchr]v1 [u4]24 [u4]0 [dist8]v1_keys [dist8]v1_values ",
            "[anchr]v1_keys [u4]14 [u4]3 [s2]30 [s2]40 [s2]50 [u2]0 ",
            "[anchr]v1_values [u4]20 [u4]3 [u4]300 [u4]400 [u4]500 [u4]0 ",
        ),
    )?;

    Ok(())
}

// Create the expected wire format representation of string.
fn str_wire_format(str: &str) -> String {
    let size = 8 + str.len();
    let num_chars = str.len();
    let padding = (8 - (str.len() % 8)) % 8;
    let body: String =
        str.as_bytes().iter().fold("".to_string(), |acc, b| format!("{acc} [u1]{b}"));
    let padding: String = std::iter::repeat_n("[u1]0 ", padding).collect();
    format!("[u4]{size} [u4]{num_chars} {body} {padding}")
}

#[gtest(RustTestMojomParsing, TestStringParsing)]
fn test_string_parsing() -> anyhow::Result<()> {
    use std::collections::HashMap;

    validate_parsing::<String>("hello".to_string(), &str_wire_format("hello"))?;

    validate_parsing::<Vec<String>>(
        vec!["life".to_string(), "universe".to_string(), "everything".to_string()],
        &format!(
            "{} {} {} {} {} {} {} {}",
            "[u4]32 [u4]3 ",
            "[dist8]life_ptr [dist8]universe_ptr [dist8]everything_ptr",
            "[anchr]life_ptr",
            &str_wire_format("life"),
            "[anchr]universe_ptr",
            &str_wire_format("universe"),
            "[anchr]everything_ptr",
            &str_wire_format("everything"),
        ),
    )?;

    validate_parsing::<HashMap<u8, String>>(
        [(10, "ten".to_string()), (20, "twenty".to_string())].into(),
        &format!(
            "{} {} {} {} {} {} {} {} {}",
            "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            "[anchr]keys_ptr ",
            "[u4]10 [u4]2 [u1]10 [u1]20 [u2]0 [u4]0 ",
            "[anchr]values_ptr ",
            "[u4]24 [u4]2 [dist8]ten_ptr [dist8]twenty_ptr ",
            "[anchr]ten_ptr ",
            &str_wire_format("ten"),
            "[anchr]twenty_ptr ",
            &str_wire_format("twenty"),
        ),
    )?;

    validate_parsing::<HashMap<String, i16>>(
        [("three".to_string(), 3), ("four".to_string(), 4)].into(),
        &format!(
            "{} {} {} {} {} {} {} {} {}",
            "[u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            "[anchr]keys_ptr ",
            "[u4]24 [u4]2 [dist8]four_ptr [dist8]three_ptr ",
            "[anchr]four_ptr ",
            &str_wire_format("four"),
            "[anchr]three_ptr ",
            &str_wire_format("three"),
            "[anchr]values_ptr ",
            "[u4]12 [u4]2 [s2]4 [s2]3 [u4]0 ",
        ),
    )?;

    // Non-UTF8 string (192 isn't a valid UTF-8 character)
    validate_parsing_failure::<String>("[u4]10 [u4]2 [u1]72 [u1]192 [u2]0 [u4]0")?;

    // Extremely UTF-8 string, courtesy of the rust docs
    validate_parsing("💖".to_string(), "[u4]12 [u4]4 [u1]240 [u1]159 [u1]146 [u1]150 [u4]0")?;

    Ok(())
}

#[gtest(RustTestMojomParsing, TestComplexUnionParsing)]
fn test_complex_union_parsing() -> anyhow::Result<()> {
    // HoldsComplexTypes: string
    validate_parsing::<HoldsComplexTypes>(
        HoldsComplexTypes::str("union_string".to_string()),
        &format!(
            "[u4]16 [u4]0 [dist8]union_str_ptr [anchr]union_str_ptr {}",
            &str_wire_format("union_string")
        ),
    )?;

    validate_parsing::<ComplexUnionHolder>(
        ComplexUnionHolder { u: HoldsComplexTypes::str("union_string".to_string()) },
        &format!(
            "[u4]24 [u4]0 [u4]16 [u4]0 [dist8]union_str_ptr [anchr]union_str_ptr {}",
            &str_wire_format("union_string")
        ),
    )?;

    // HoldsComplexTypes: array<int16>
    validate_parsing::<HoldsComplexTypes>(
        HoldsComplexTypes::arr(vec![-10, -20, -30]),
        concat!(
            "[u4]16 [u4]1 [dist8]union_arr_ptr ",
            "[anchr]union_arr_ptr [u4]14 [u4]3 [s2]-10 [s2]-20 [s2]-30 [u2]0"
        ),
    )?;

    // HoldsComplexTypes: map<uint8, uint8>
    validate_parsing::<HoldsComplexTypes>(
        HoldsComplexTypes::m([(100, 1), (101, 2)].into()),
        concat!(
            "[u4]16 [u4]2 [dist8]union_map_ptr ",
            "[anchr]union_map_ptr [u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            "[anchr]keys_ptr [u4]10 [u4]2 [u1]100 [u1]101 [u2]0 [u4]0 ",
            "[anchr]values_ptr [u4]10 [u4]2 [u1]1 [u1]2 [u2]0 [u4]0 ",
        ),
    )?;
    Ok(())
}

#[gtest(RustTestMojomParsing, TestNullableParsing)]
fn test_nullable_parsing() -> anyhow::Result<()> {
    validate_parsing::<NullableBasics>(
        NullableBasics {
            b: None,
            n1: None,
            n2: None,
            empty: None,
            e: None,
            fourints: None,
            f1: None,
            f2: None,
        },
        "[u4]48 [u4]0 [b]00000000 [u1]0 [u2]0 [u4]0 [u8]0 [u8]0 [f]0 [u4]0 [d]0",
    )?;

    validate_parsing::<NullableBasics>(
        NullableBasics {
            b: Some(true),
            n1: None,
            n2: Some(12),
            empty: None,
            e: None,
            fourints: Some(FourInts { a: 1, b: 2, c: 3, d: 4 }),
            f1: None,
            f2: Some(2.71828),
        },
        concat!(
            "[u4]48 [u4]0 [b]01001011 [u1]12 [u2]0 [u4]0 [u8]0 [dist8]fourints_ptr [f]0 [u4]0 [d]2.71828 ",
            "[anchr]fourints_ptr [u4]24 [u4]0 [s1]1 [u1]0 [s2]2 [s4]3 [s8]4",
        ),
    )?;

    validate_parsing::<NullableBasics>(
        NullableBasics {
            b: None,
            n1: Some(33),
            n2: None,
            empty: Some(Empty {}),
            e: Some(TestEnum::Four),
            fourints: None,
            f1: Some(3.14),
            f2: None,
        },
        concat!(
            "[u4]48 [u4]0 [b]00110100 [u1]0 [u2]33 [u4]4 [dist8]empty_ptr [u8]0 [f]3.14 [u4]0 [d]0 ",
            "[anchr]empty_ptr [u4]8 [u4]0",
        ),
    )?;

    validate_parsing::<NullableBasics>(
        NullableBasics {
            b: Some(false),
            n1: Some(44),
            n2: Some(22),
            empty: Some(Empty {}),
            e: Some(TestEnum::Zero),
            fourints: Some(FourInts { a: 1, b: 2, c: 3, d: 4 }),
            f1: Some(1.23),
            f2: Some(4.56),
        },
        concat!(
            "[u4]48 [u4]0 [b]01111101 [u1]22 [u2]44 [u4]0 [dist8]empty_ptr [dist8]fourints_ptr [f]1.23 [u4]0 [d]4.56 ",
            "[anchr]empty_ptr [u4]8 [u4]0 ",
            "[anchr]fourints_ptr [u4]24 [u4]0 [s1]1 [u1]0 [s2]2 [s4]3 [s8]4",
        ),
    )?;

    validate_parsing::<ArraysOfNullables>(
        ArraysOfNullables {
            bools: vec![
                Some(true),
                None,
                Some(false),
                None,
                None,
                Some(true),
                Some(true),
                Some(false),
                Some(true),
                None,
                None,
                Some(false),
            ],
            empties: vec![None, Some(Empty {}), None, Some(Empty {})],
            enums: vec![Some(TestEnum::Seven), None, Some(TestEnum::Zero), Some(TestEnum::Seven)],
            unions: vec![Some(BaseUnion::n1(5)), None, Some(BaseUnion::b1(true))],
        },
        concat!(
            "[u4]40 [u4]0 [dist8]bools_ptr [dist8]empties_ptr [dist8]enums_ptr [dist8]unions_ptr ",
            "[anchr]bools_ptr [u4]12 [u4]12 [b]11100101 [b]00001001 [b]01100001 [b]00000001 [u4]0 ",
            "[anchr]empties_ptr [u4]40 [u4]4 [u8]0 [dist8]empty_ptr_1 [u8]0 [dist8]empty_ptr_2 ",
            "[anchr]empty_ptr_1 [u4]8 [u4]0 ",
            "[anchr]empty_ptr_2 [u4]8 [u4]0 ",
            "[anchr]enums_ptr [u4]28 [u4]4 [b]00001101 [u1]0 [u2]0 [u4]7 [u4]0 [u4]0 [u4]7 [u4]0 ",
            "[anchr]unions_ptr [u4]56 [u4]3 ",
            "[u4]16 [u4]0 [u8]5 ",
            "[u8]0 [u8]0 ",
            "[u4]16 [u4]3 [u8]1"
        ),
    )?;

    validate_parsing::<NullableArrays>(
        NullableArrays { null_arr: None, double_null_arr: None },
        "[u4]24 [u4]0 [u8]0 [u8]0",
    )?;

    validate_parsing::<NullableArrays>(
        NullableArrays {
            null_arr: Some(vec![true, false, true]),
            double_null_arr: Some(vec![Some(true), None, Some(false), Some(true)]),
        },
        concat!(
            "[u4]24 [u4]0 [dist8]null_arr_ptr [dist8]double_null_arr_ptr ",
            "[anchr]null_arr_ptr [u4]9 [u4]3 [b]00000101 [u1]0 [u2]0 [u4]0 ",
            "[anchr]double_null_arr_ptr [u4]10 [u4]4 [b]00001101 [b]00001001 [u2]0 [u4]0"
        ),
    )?;

    validate_parsing::<UnionWithNullables>(UnionWithNullables::e(None), "[u4]16 [u4]0 [u8]0")?;
    validate_parsing::<UnionWithNullables>(UnionWithNullables::str(None), "[u4]16 [u4]1 [u8]0")?;
    validate_parsing::<UnionWithNullables>(UnionWithNullables::u(None), "[u4]16 [u4]2 [u8]0")?;

    validate_parsing::<UnionWithNullables>(
        UnionWithNullables::e(Some(Empty {})),
        "[u4]16 [u4]0 [dist8]empty_ptr [anchr]empty_ptr [u4]8 [u4]0",
    )?;
    validate_parsing::<UnionWithNullables>(
        UnionWithNullables::str(Some("union_string".to_string())),
        &format!(
            "[u4]16 [u4]1 [dist8]union_str_ptr [anchr]union_str_ptr {}",
            &str_wire_format("union_string")
        ),
    )?;
    validate_parsing::<UnionWithNullables>(
        UnionWithNullables::u(Some(BaseUnion::n1(123))),
        "[u4]16 [u4]2 [dist8]u_ptr [anchr]u_ptr [u4]16 [u4]0 [u8]123",
    )?;

    validate_parsing::<NullableOthers>(
        NullableOthers { u: None, m: None, str: None },
        "[u4]40 [u4]0 [u8]0 [u8]0 [u8]0 [u8]0",
    )?;
    validate_parsing::<NullableOthers>(
        NullableOthers {
            u: Some(UnionWithNullables::u(None)),
            m: None,
            str: Some("holla".to_string()),
        },
        &format!(
            "{} {} {}",
            "[u4]40 [u4]0 [u4]16 [u4]2 [u8]0 [u8]0 [dist8]str_ptr ",
            "[anchr]str_ptr ",
            &str_wire_format("holla")
        ),
    )?;
    validate_parsing::<NullableOthers>(
        NullableOthers {
            u: Some(UnionWithNullables::u(Some(BaseUnion::f1(FourInts {
                a: 1,
                b: 2,
                c: 3,
                d: 4,
            })))),
            m: None,
            str: None,
        },
        concat!(
            "[u4]40 [u4]0 [u4]16 [u4]2 [dist8]u_inner_ptr [u8]0 [u8]0 ",
            "[anchr]u_inner_ptr [u4]16 [u4]6 [dist8]f1_ptr ",
            "[anchr]f1_ptr [u4]24 [u4]0 [s1]1 [u1]0 [s2]2 [s4]3 [s8]4"
        ),
    )?;
    validate_parsing::<NullableOthers>(
        NullableOthers {
            u: Some(UnionWithNullables::u(Some(BaseUnion::n1(42)))),
            m: Some([(1, 2), (3, 4)].into()),
            str: Some("hello".to_string()),
        },
        &format!(
            "{} {} {} {} {} {} {}",
            "[u4]40 [u4]0 [u4]16 [u4]2 [dist8]u_inner_ptr [dist8]m_ptr [dist8]str_ptr ",
            "[anchr]u_inner_ptr [u4]16 [u4]0 [u8]42 ",
            "[anchr]m_ptr [u4]24 [u4]0 [dist8]keys_ptr [dist8]values_ptr ",
            "[anchr]keys_ptr [u4]10 [u4]2 [u1]1 [u1]3 [u2]0 [u4]0 ",
            "[anchr]values_ptr [u4]10 [u4]2 [u1]2 [u1]4 [u2]0 [u4]0 ",
            "[anchr]str_ptr ",
            &str_wire_format("hello")
        ),
    )?;

    Ok(())
}

#[gtest(RustTestMojomParsing, TestHandleParsing)]
fn test_handle_parsing() -> anyhow::Result<()> {
    // Note: [s4]-1 is 0xffffffff, the indicator for a `None` handle
    validate_parsing_with_handles(
        Handles { h1: dummy_handle(), h2: None, h3: dummy_handle().into(), h4: None },
        "[u4]24 [u4]0 [u4]0 [s4]-1 [u4]1 [s4]-1",
        2,
    )?;
    // Can't parse if we don't give enough handles
    validate_parsing_failure_with_handles::<Handles>("[u4]24 [u4]0 [u4]0 [s4]-1 [u4]1 [s4]-1", 1)?;

    validate_parsing_with_handles(
        Handles {
            h1: dummy_handle(),
            h2: Some(dummy_handle()),
            h3: dummy_handle().into(),
            h4: None,
        },
        "[u4]24 [u4]0 [u4]0 [u4]1 [u4]2 [s4]-1",
        3,
    )?;

    validate_parsing_with_handles(
        Handles {
            h1: dummy_handle(),
            h2: None,
            h3: dummy_handle().into(),
            h4: Some(dummy_handle().into()),
        },
        "[u4]24 [u4]0 [u4]0 [s4]-1 [u4]1 [u4]2",
        3,
    )?;

    validate_parsing_with_handles(
        Handles {
            h1: dummy_handle(),
            h2: Some(dummy_handle()),
            h3: dummy_handle().into(),
            h4: Some(dummy_handle().into()),
        },
        "[u4]24 [u4]0 [u4]0 [u4]1 [u4]2 [u4]3",
        4,
    )?;

    validate_parsing_with_handles(WithHandles::h1(None), "[u4]16 [u4]0 [s4]-1 [u4]0", 0)?;
    validate_parsing_with_handles(
        WithHandles::h1(Some(dummy_handle())),
        "[u4]16 [u4]0 [u4]0 [u4]0",
        1,
    )?;
    validate_parsing_with_handles(
        WithHandles::h2(dummy_handle().into()),
        "[u4]16 [u4]1 [u4]0 [u4]0",
        1,
    )?;
    validate_parsing(WithHandles::n(42), "[u4]16 [u4]2 [u4]42 [u4]0")?;

    validate_parsing_with_handles(
        NestedHandles {
            h1: dummy_handle(),
            arr: vec![Some(dummy_handle()), None, Some(dummy_handle()), None],
            h2: dummy_handle(),
            wh: WithHandles::h2(dummy_handle().into()),
            h3: dummy_handle(),
        },
        concat!(
            "[u4]48 [u4]0 [u4]0 [u4]1 [dist8]arr_ptr ",
            "[u4]16 [u4]1 [u4]2 [u4]0 ", // wh
            "[u4]3 [u4]0 ",
            "[anchr]arr_ptr [u4]24 [u4]4 [u4]4 [s4]-1 [u4]5 [s4]-1" // arr
        ),
        6,
    )?;

    Ok(())
}
