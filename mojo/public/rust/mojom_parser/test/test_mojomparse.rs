// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Verifies that the MojomParse attribute is correctly derived.
//!
//! FOR_RELEASE: Rewrite the below comment to be clearer.
//!
//! Testing strategy: The existing tests for mojom bindings are end-to-end,
//! relying on mojom files to specify their inputs and outputs. However, we
//! want to unit-test the parser and deparser alone. Since they take ASTs as
//! input, we first validate that the bindings generate the correct ASTs, and
//! then we can use the bindings to generate various parser/deparser tests.
//!
//! The types in this file correspond to those defined in
//! //mojo/public/rust/test_mojom/parser_unittests.mojom

chromium::import! {
    "//mojo/public/rust/mojom_parser:mojom_parser_core";
    "//mojo/public/rust/mojom_parser:parser_unittests_rust";
}

use rust_gtest_interop::prelude::*;

use mojom_parser_core::*;
use parser_unittests_rust::parser_unittests::*;
use std::sync::LazyLock;

/// Represents a type defined in a Mojom file.
///
/// Conceptually, it has three parts, corresponding to the three AST types:
/// 1. The type itself, as written in the Mojom file.
/// 2. The packed representation of the type.
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
    packed_type: MojomWireType,
    // Imagine there's a third `constructor` entry here.
}

fn wrap_struct_fields_type(fields: Vec<(String, MojomType)>) -> MojomType {
    let (field_names, fields) = fields.into_iter().unzip();
    MojomType::Struct { fields, field_names }
}

fn wrap_struct_fields_value(fields: Vec<(String, MojomValue)>) -> MojomValue {
    let (field_names, fields) = fields.into_iter().unzip();
    MojomValue::Struct(field_names, fields)
}

fn wrap_packed_struct_fields(fields: Vec<(String, MojomWireType)>) -> MojomWireType {
    let (packed_field_names, packed_field_types) = fields.into_iter().unzip();
    MojomWireType::Pointer {
        ordinal: 0,
        nested_data_type: PackedStructuredType::Struct { packed_field_names, packed_field_types },
    }
}

impl TestType {
    /// Return the packed version of this type as a struct field with the given
    /// ordinal
    fn as_struct_field(&self, ordinal: Ordinal) -> MojomWireType {
        match self.packed_type.clone() {
            MojomWireType::Leaf { ordinal: _, leaf_type } => {
                MojomWireType::Leaf { ordinal, leaf_type }
            }
            MojomWireType::Bitfield { ordinals: _ } => {
                panic!("Bitfields aren't supported as individuals")
            }
            MojomWireType::Pointer { ordinal: _, nested_data_type } => {
                MojomWireType::Pointer { ordinal, nested_data_type }
            }
            MojomWireType::Union { ordinal: _, variants } => {
                MojomWireType::Union { ordinal, variants }
            }
        }
    }

    /// Return the packed version of this type as a union field
    fn as_union_field(&self) -> MojomWireType {
        match self.packed_type.clone() {
            // Nested unions are represented as pointers
            MojomWireType::Union { variants, .. } => MojomWireType::Pointer {
                ordinal: 0,
                nested_data_type: PackedStructuredType::Union { variants },
            },
            // Everything else is represented as itself.
            _ => self.packed_type.clone(),
        }
    }

    /// Given the rust type T corresponding to this TestType, validate that:
    /// (1) T's associated MojomType and MojomWireType match ours, and
    /// (2) The input value of type T can be converted to and from the input
    ///     MojomValue
    fn validate_mojomparse<T: MojomParse + std::fmt::Debug + Clone + PartialEq>(
        &self,
        rust_val: T,
        mojom_val: MojomValue,
    ) {
        // FOR RELEASE: These assertion macros seem to print a massive (and utterly
        // useless) stack trace. See if we can turn that off.
        expect_eq!(
            T::mojom_type(),
            self.base_type,
            "Type {} had the wrong associated MojomType!",
            self.type_name
        );

        expect_eq!(
            *T::wire_type(),
            self.as_struct_field(0),
            "Type {} failed to pack correctly!",
            self.type_name
        );

        // Test conversion to/from MojomValues
        expect_eq!(mojom_val, rust_val.clone().into());
        expect_eq!(rust_val, mojom_val.clone().try_into().unwrap());
    }
}

// We'll be creating all our test types at global scope, so we can reference
// them from various testing functions.

// Mojom Definition:
// struct Empty {};
static EMPTY_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "Empty",
    base_type: wrap_struct_fields_type(vec![]),
    packed_type: wrap_packed_struct_fields(vec![]),
});

fn empty_mojom() -> MojomValue {
    wrap_struct_fields_value(vec![])
}

// Mojom Definition:
// struct FourInts {
//     int8 a;
//     int16 b;
//     int32 c;
//     int64 d;
// };
static FOUR_INTS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "FourInts",
    base_type: wrap_struct_fields_type(vec![
        ("a".to_string(), MojomType::Int8),
        ("b".to_string(), MojomType::Int16),
        ("c".to_string(), MojomType::Int32),
        ("d".to_string(), MojomType::Int64),
    ]),
    packed_type: wrap_packed_struct_fields(vec![
        ("a".to_string(), MojomWireType::Leaf { ordinal: 0, leaf_type: PackedLeafType::Int8 }),
        ("b".to_string(), MojomWireType::Leaf { ordinal: 1, leaf_type: PackedLeafType::Int16 }),
        ("c".to_string(), MojomWireType::Leaf { ordinal: 2, leaf_type: PackedLeafType::Int32 }),
        ("d".to_string(), MojomWireType::Leaf { ordinal: 3, leaf_type: PackedLeafType::Int64 }),
    ]),
});

fn four_ints_mojom(a: i8, b: i16, c: i32, d: i64) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("a".to_string(), MojomValue::Int8(a)),
        ("b".to_string(), MojomValue::Int16(b)),
        ("c".to_string(), MojomValue::Int32(c)),
        ("d".to_string(), MojomValue::Int64(d)),
    ])
}

// Mojom Definition:
// struct FourIntsReversed {
//   int64 d;
//   int32 c;
//   int16 b;
//   int8 a;
// };
static FOUR_INTS_REVERSED_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "FourIntsReversed",
    base_type: wrap_struct_fields_type(vec![
        ("d".to_string(), MojomType::Int64),
        ("c".to_string(), MojomType::Int32),
        ("b".to_string(), MojomType::Int16),
        ("a".to_string(), MojomType::Int8),
    ]),
    packed_type: wrap_packed_struct_fields(vec![
        ("d".to_string(), MojomWireType::Leaf { ordinal: 0, leaf_type: PackedLeafType::Int64 }),
        ("c".to_string(), MojomWireType::Leaf { ordinal: 1, leaf_type: PackedLeafType::Int32 }),
        ("b".to_string(), MojomWireType::Leaf { ordinal: 2, leaf_type: PackedLeafType::Int16 }),
        ("a".to_string(), MojomWireType::Leaf { ordinal: 3, leaf_type: PackedLeafType::Int8 }),
    ]),
});

fn four_ints_reversed_mojom(a: i8, b: i16, c: i32, d: i64) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("d".to_string(), MojomValue::Int64(d)),
        ("c".to_string(), MojomValue::Int32(c)),
        ("b".to_string(), MojomValue::Int16(b)),
        ("a".to_string(), MojomValue::Int8(a)),
    ])
}

// Mojom Definition:
// struct FourIntsIntermixed {
//   int8 a;
//   int32 b;
//   int16 c;
//   int8 d;
// };
static FOUR_INTS_INTERMIXED_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "FourIntsIntermixed",
    base_type: wrap_struct_fields_type(vec![
        ("a".to_string(), MojomType::Int8),
        ("b".to_string(), MojomType::Int32),
        ("c".to_string(), MojomType::Int16),
        ("d".to_string(), MojomType::Int8),
    ]),
    packed_type: wrap_packed_struct_fields(vec![
        ("a".to_string(), MojomWireType::Leaf { ordinal: 0, leaf_type: PackedLeafType::Int8 }),
        ("d".to_string(), MojomWireType::Leaf { ordinal: 3, leaf_type: PackedLeafType::Int8 }),
        ("c".to_string(), MojomWireType::Leaf { ordinal: 2, leaf_type: PackedLeafType::Int16 }),
        ("b".to_string(), MojomWireType::Leaf { ordinal: 1, leaf_type: PackedLeafType::Int32 }),
    ]),
});

fn four_ints_intermixed_mojom(a: i8, b: i32, c: i16, d: i8) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("a".to_string(), MojomValue::Int8(a)),
        ("b".to_string(), MojomValue::Int32(b)),
        ("c".to_string(), MojomValue::Int16(c)),
        ("d".to_string(), MojomValue::Int8(d)),
    ])
}

// Mojom Definition:
// struct OnceNested {
//   FourInts f1;
//   uint32 a;
//   int8 b;
//   FourIntsReversed f2;
//   FourIntsIntermixed f3;
//   uint16 c;
// };
static ONCE_NESTED_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "OnceNested",
    base_type: wrap_struct_fields_type(vec![
        ("f1".to_string(), FOUR_INTS_TY.base_type.clone()),
        ("a".to_string(), MojomType::UInt32),
        ("b".to_string(), MojomType::Int8),
        ("f2".to_string(), FOUR_INTS_REVERSED_TY.base_type.clone()),
        ("f3".to_string(), FOUR_INTS_INTERMIXED_TY.base_type.clone()),
        ("c".to_string(), MojomType::UInt16),
    ]),
    packed_type: wrap_packed_struct_fields(vec![
        ("f1".to_string(), FOUR_INTS_TY.as_struct_field(0)),
        ("a".to_string(), MojomWireType::Leaf { ordinal: 1, leaf_type: PackedLeafType::UInt32 }),
        ("b".to_string(), MojomWireType::Leaf { ordinal: 2, leaf_type: PackedLeafType::Int8 }),
        ("c".to_string(), MojomWireType::Leaf { ordinal: 5, leaf_type: PackedLeafType::UInt16 }),
        ("f2".to_string(), FOUR_INTS_REVERSED_TY.as_struct_field(3)),
        ("f3".to_string(), FOUR_INTS_INTERMIXED_TY.as_struct_field(4)),
    ]),
});

fn once_nested_mojom(
    f1: MojomValue,
    a: u32,
    b: i8,
    f2: MojomValue,
    f3: MojomValue,
    c: u16,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("f1".to_string(), f1),
        ("a".to_string(), MojomValue::UInt32(a)),
        ("b".to_string(), MojomValue::Int8(b)),
        ("f2".to_string(), f2),
        ("f3".to_string(), f3),
        ("c".to_string(), MojomValue::UInt16(c)),
    ])
}

// Mojom Definition:
// struct TwiceNested {
//   OnceNested o;
//   int16 a;
//   FourInts f;
// };
static TWICE_NESTED_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "TwiceNested",
    base_type: wrap_struct_fields_type(vec![
        ("o".to_string(), ONCE_NESTED_TY.base_type.clone()),
        ("a".to_string(), MojomType::Int16),
        ("f".to_string(), FOUR_INTS_TY.base_type.clone()),
        ("b".to_string(), MojomType::Int32),
        ("c".to_string(), MojomType::Int32),
    ]),
    packed_type: wrap_packed_struct_fields(vec![
        ("o".to_string(), ONCE_NESTED_TY.as_struct_field(0)),
        ("a".to_string(), MojomWireType::Leaf { ordinal: 1, leaf_type: PackedLeafType::Int16 }),
        ("b".to_string(), MojomWireType::Leaf { ordinal: 3, leaf_type: PackedLeafType::Int32 }),
        ("f".to_string(), FOUR_INTS_TY.as_struct_field(2)),
        ("c".to_string(), MojomWireType::Leaf { ordinal: 4, leaf_type: PackedLeafType::Int32 }),
    ]),
});

fn twice_nested_mojom(o: MojomValue, a: i16, f: MojomValue, b: i32, c: i32) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("o".to_string(), o),
        ("a".to_string(), MojomValue::Int16(a)),
        ("f".to_string(), f),
        ("b".to_string(), MojomValue::Int32(b)),
        ("c".to_string(), MojomValue::Int32(c)),
    ])
}

// Mojom Definition:
// struct TenBoolsAndAByte {
//   bool b0; ... bool b4;
//   uint8 n1;
//   bool b5; ... bool b9;
// };
static TEN_BOOLS_AND_A_BYTE_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "TenBoolsAndAByte",
    base_type: wrap_struct_fields_type(vec![
        ("b0".to_string(), MojomType::Bool),
        ("b1".to_string(), MojomType::Bool),
        ("b2".to_string(), MojomType::Bool),
        ("b3".to_string(), MojomType::Bool),
        ("b4".to_string(), MojomType::Bool),
        ("n1".to_string(), MojomType::UInt8),
        ("b5".to_string(), MojomType::Bool),
        ("b6".to_string(), MojomType::Bool),
        ("b7".to_string(), MojomType::Bool),
        ("b8".to_string(), MojomType::Bool),
        ("b9".to_string(), MojomType::Bool),
    ]),
    packed_type: wrap_packed_struct_fields(vec![
        (
            "b0".to_string(),
            MojomWireType::Bitfield {
                ordinals: [Some(0), Some(1), Some(2), Some(3), Some(4), Some(6), Some(7), Some(8)],
            },
        ),
        ("n1".to_string(), MojomWireType::Leaf { ordinal: 5, leaf_type: PackedLeafType::UInt8 }),
        (
            "b8".to_string(),
            MojomWireType::Bitfield {
                ordinals: [Some(9), Some(10), None, None, None, None, None, None],
            },
        ),
    ]),
});

fn ten_bools_and_a_byte_mojom(
    b0: bool,
    b1: bool,
    b2: bool,
    b3: bool,
    b4: bool,
    n1: u8,
    b5: bool,
    b6: bool,
    b7: bool,
    b8: bool,
    b9: bool,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("b0".to_string(), MojomValue::Bool(b0)),
        ("b1".to_string(), MojomValue::Bool(b1)),
        ("b2".to_string(), MojomValue::Bool(b2)),
        ("b3".to_string(), MojomValue::Bool(b3)),
        ("b4".to_string(), MojomValue::Bool(b4)),
        ("n1".to_string(), MojomValue::UInt8(n1)),
        ("b5".to_string(), MojomValue::Bool(b5)),
        ("b6".to_string(), MojomValue::Bool(b6)),
        ("b7".to_string(), MojomValue::Bool(b7)),
        ("b8".to_string(), MojomValue::Bool(b8)),
        ("b9".to_string(), MojomValue::Bool(b9)),
    ])
}

// Mojom Definition:
// struct TenBoolsAndTwoBytes {
//   bool b0; ... bool b4;
//   uint16 n1;
//   bool b5; ... bool b9;
// };
static TEN_BOOLS_AND_TWO_BYTES_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "TenBoolsAndTwoBytes",
    base_type: wrap_struct_fields_type(vec![
        ("b0".to_string(), MojomType::Bool),
        ("b1".to_string(), MojomType::Bool),
        ("b2".to_string(), MojomType::Bool),
        ("b3".to_string(), MojomType::Bool),
        ("b4".to_string(), MojomType::Bool),
        ("n1".to_string(), MojomType::UInt16),
        ("b5".to_string(), MojomType::Bool),
        ("b6".to_string(), MojomType::Bool),
        ("b7".to_string(), MojomType::Bool),
        ("b8".to_string(), MojomType::Bool),
        ("b9".to_string(), MojomType::Bool),
    ]),
    packed_type: wrap_packed_struct_fields(vec![
        (
            "b0".to_string(),
            MojomWireType::Bitfield {
                ordinals: [Some(0), Some(1), Some(2), Some(3), Some(4), Some(6), Some(7), Some(8)],
            },
        ),
        (
            "b8".to_string(),
            MojomWireType::Bitfield {
                ordinals: [Some(9), Some(10), None, None, None, None, None, None],
            },
        ),
        ("n1".to_string(), MojomWireType::Leaf { ordinal: 5, leaf_type: PackedLeafType::UInt16 }),
    ]),
});

fn ten_bools_and_two_bytes_mojom(
    b0: bool,
    b1: bool,
    b2: bool,
    b3: bool,
    b4: bool,
    n1: u16,
    b5: bool,
    b6: bool,
    b7: bool,
    b8: bool,
    b9: bool,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("b0".to_string(), MojomValue::Bool(b0)),
        ("b1".to_string(), MojomValue::Bool(b1)),
        ("b2".to_string(), MojomValue::Bool(b2)),
        ("b3".to_string(), MojomValue::Bool(b3)),
        ("b4".to_string(), MojomValue::Bool(b4)),
        ("n1".to_string(), MojomValue::UInt16(n1)),
        ("b5".to_string(), MojomValue::Bool(b5)),
        ("b6".to_string(), MojomValue::Bool(b6)),
        ("b7".to_string(), MojomValue::Bool(b7)),
        ("b8".to_string(), MojomValue::Bool(b8)),
        ("b9".to_string(), MojomValue::Bool(b9)),
    ])
}

#[gtest(MojomParser, TestMojomParseDerive)]
/// Tests that the MojomParse trait is correctly
/// derived for each of the types in the mojom file.
fn test_mojomparse() {
    FOUR_INTS_TY.validate_mojomparse(
        FourInts { a: 10, b: -20, c: 400, d: -8000 },
        four_ints_mojom(10, -20, 400, -8000),
    );

    EMPTY_TY.validate_mojomparse(Empty {}, empty_mojom());

    FOUR_INTS_REVERSED_TY.validate_mojomparse(
        FourIntsReversed { a: 10, b: -20, c: 400, d: -8000 },
        four_ints_reversed_mojom(10, -20, 400, -8000),
    );

    FOUR_INTS_INTERMIXED_TY.validate_mojomparse(
        FourIntsIntermixed { a: 10, b: 400, c: -20, d: -5 },
        four_ints_intermixed_mojom(10, 400, -20, -5),
    );

    let once_nested_val = OnceNested {
        f1: FourInts { a: 1, b: 2, c: 3, d: 4 },
        a: 13,
        b: 14,
        f2: FourIntsReversed { a: 5, b: 6, c: 7, d: 8 },
        f3: FourIntsIntermixed { a: 9, b: 10, c: 11, d: 12 },
        c: 15,
    };
    let once_nested_mojom_val = once_nested_mojom(
        four_ints_mojom(1, 2, 3, 4),
        13,
        14,
        four_ints_reversed_mojom(5, 6, 7, 8),
        four_ints_intermixed_mojom(9, 10, 11, 12),
        15,
    );
    ONCE_NESTED_TY.validate_mojomparse(once_nested_val.clone(), once_nested_mojom_val.clone());

    TWICE_NESTED_TY.validate_mojomparse(
        TwiceNested {
            o: once_nested_val,
            a: 16,
            f: FourInts { a: 17, b: 18, c: 19, d: 20 },
            b: 21,
            c: 22,
        },
        twice_nested_mojom(once_nested_mojom_val, 16, four_ints_mojom(17, 18, 19, 20), 21, 22),
    );

    TEN_BOOLS_AND_A_BYTE_TY.validate_mojomparse(
        TenBoolsAndAByte {
            b0: true,
            b1: false,
            b2: true,
            b3: false,
            b4: true,
            n1: 200,
            b5: false,
            b6: true,
            b7: false,
            b8: true,
            b9: false,
        },
        ten_bools_and_a_byte_mojom(
            true, false, true, false, true, 200, false, true, false, true, false,
        ),
    );
    TEN_BOOLS_AND_TWO_BYTES_TY.validate_mojomparse(
        TenBoolsAndTwoBytes {
            b0: true,
            b1: false,
            b2: true,
            b3: false,
            b4: true,
            n1: 50000,
            b5: false,
            b6: true,
            b7: false,
            b8: true,
            b9: false,
        },
        ten_bools_and_two_bytes_mojom(
            true, false, true, false, true, 50000, false, true, false, true, false,
        ),
    );
}

#[gtest(MojomParser, TestBadConversion)]
/// Test that we can't convert between incompatible types
fn test_bad_conversion() {
    let empty = empty_mojom();

    let four_ints = four_ints_mojom(10, -20, 400, -8000);

    let four_ints_reversed = four_ints_reversed_mojom(10, -20, 400, -8000);

    let four_ints_intermixed = four_ints_intermixed_mojom(10, 400, -20, -5);

    let once_nested = once_nested_mojom(
        four_ints_mojom(1, 2, 3, 4),
        13,
        14,
        four_ints_reversed_mojom(5, 6, 7, 8),
        four_ints_intermixed_mojom(9, 10, 11, 12),
        15,
    );

    let twice_nested =
        twice_nested_mojom(once_nested.clone(), 16, four_ints_mojom(17, 18, 19, 20), 21, 22);

    let ten_bools_byte = ten_bools_and_a_byte_mojom(
        true, false, true, false, true, 200, false, true, false, true, false,
    );
    let ten_bools_bytes = ten_bools_and_two_bytes_mojom(
        true, false, true, false, true, 50000, false, true, false, true, false,
    );

    // There are far too many bad paths to test them all, so this is just an
    // arbitrary scattershot approach so we have _some_ coverage.
    // TODO(crbug.com/456214728) Replace with matchers from the googletest crate
    // if we switch to it.
    expect_true!(FourInts::try_from(four_ints_reversed).is_err());
    expect_true!(FourInts::try_from(once_nested).is_err());
    expect_true!(FourInts::try_from(empty).is_err());

    expect_true!(TenBoolsAndAByte::try_from(ten_bools_bytes).is_err());
    expect_true!(TenBoolsAndAByte::try_from(four_ints_intermixed).is_err());

    expect_true!(OnceNested::try_from(twice_nested).is_err());
    expect_true!(OnceNested::try_from(ten_bools_byte.clone()).is_err());

    expect_true!(Empty::try_from(ten_bools_byte).is_err());
    expect_true!(Empty::try_from(four_ints).is_err());
}

// Mojom Definition:
// struct SomeEnums {
//   TestEnum e1;
//   uint16 n1;
//   TestEnum2 e2;
// }
const TEST_ENUM_PRED: Predicate<u32> =
    Predicate::new::<TestEnum>(&(TestEnum::is_valid as fn(u32) -> bool));
const TEST_ENUM2_PRED: Predicate<u32> =
    Predicate::new::<TestEnum2>(&(TestEnum2::is_valid as fn(u32) -> bool));

static SOME_ENUMS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "SomeEnums",
    base_type: wrap_struct_fields_type(vec![
        ("e1".to_string(), MojomType::Enum { is_valid: TEST_ENUM_PRED }),
        ("n1".to_string(), MojomType::UInt64),
        ("e2".to_string(), MojomType::Enum { is_valid: TEST_ENUM2_PRED }),
    ]),
    packed_type: wrap_packed_struct_fields(vec![
        (
            "e1".to_string(),
            MojomWireType::Leaf {
                ordinal: 0,
                leaf_type: PackedLeafType::Enum { is_valid: TEST_ENUM_PRED },
            },
        ),
        (
            "e2".to_string(),
            MojomWireType::Leaf {
                ordinal: 2,
                leaf_type: PackedLeafType::Enum { is_valid: TEST_ENUM2_PRED },
            },
        ),
        ("n1".to_string(), MojomWireType::Leaf { ordinal: 1, leaf_type: PackedLeafType::UInt64 }),
    ]),
});

fn some_enums_mojom(e1: u32, n1: u64, e2: u32) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("e1".to_string(), MojomValue::Enum(e1)),
        ("n1".to_string(), MojomValue::UInt64(n1)),
        ("e2".to_string(), MojomValue::Enum(e2)),
    ])
}

#[gtest(MojomParser, TestEnums)]
fn test_enums() {
    SOME_ENUMS_TY.validate_mojomparse(
        SomeEnums { e1: TestEnum::Four, n1: 42, e2: TestEnum2::Eleven },
        some_enums_mojom(4, 42, 11),
    );
    SOME_ENUMS_TY.validate_mojomparse(
        SomeEnums { e1: TestEnum::Zero, n1: 0, e2: TestEnum2::Twelve },
        some_enums_mojom(0, 0, 12),
    );
    SOME_ENUMS_TY.validate_mojomparse(
        SomeEnums { e1: TestEnum::Three, n1: 3, e2: TestEnum2::Four },
        some_enums_mojom(3, 3, 4),
    );
    SOME_ENUMS_TY.validate_mojomparse(
        SomeEnums { e1: TestEnum::Seven, n1: 7, e2: TestEnum2::FourtyTwo },
        some_enums_mojom(7, 7, 42),
    );

    expect_true!(TestEnum::try_from(8).is_err());
    expect_true!(TestEnum::try_from(11).is_err());
    expect_true!(TestEnum2::try_from(0).is_err());
    expect_true!(TestEnum2::try_from(99).is_err());
}

// Mojom Definition:
// union BaseUnion {
//   int8 n1;
//   uint64 u1;
//   TestEnum e1;
//   bool b1;
//   bool b2;
//   Empty em1;
//   FourInts f1;
// }
static BASE_UNION_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "BaseUnion",
    base_type: MojomType::Union {
        variants: [
            (0, MojomType::Int8),
            (1, MojomType::UInt64),
            (2, MojomType::Enum { is_valid: TEST_ENUM_PRED }),
            (3, MojomType::Bool),
            (4, MojomType::Bool),
            (5, EMPTY_TY.base_type.clone()),
            (6, FOUR_INTS_TY.base_type.clone()),
        ]
        .into(),
    },
    packed_type: MojomWireType::Union {
        ordinal: 0,
        variants: [
            (0, MojomWireType::Leaf { ordinal: 0, leaf_type: PackedLeafType::Int8 }),
            (1, MojomWireType::Leaf { ordinal: 0, leaf_type: PackedLeafType::UInt64 }),
            (
                2,
                MojomWireType::Leaf {
                    ordinal: 0,
                    leaf_type: PackedLeafType::Enum { is_valid: TEST_ENUM_PRED },
                },
            ),
            (
                3,
                MojomWireType::Bitfield {
                    ordinals: [Some(0), None, None, None, None, None, None, None],
                },
            ),
            (
                4,
                MojomWireType::Bitfield {
                    ordinals: [Some(0), None, None, None, None, None, None, None],
                },
            ),
            (5, EMPTY_TY.as_union_field()),
            (6, FOUR_INTS_TY.as_union_field()),
        ]
        .into(),
    },
});

fn test_union_mojom_n1(n1: i8) -> MojomValue {
    MojomValue::Union(0, Box::new(MojomValue::Int8(n1)))
}

fn test_union_mojom_u1(u1: u64) -> MojomValue {
    MojomValue::Union(1, Box::new(MojomValue::UInt64(u1)))
}

fn test_union_mojom_e1(e1: u32) -> MojomValue {
    MojomValue::Union(2, Box::new(MojomValue::Enum(e1)))
}

fn test_union_mojom_b1(b1: bool) -> MojomValue {
    MojomValue::Union(3, Box::new(MojomValue::Bool(b1)))
}

fn test_union_mojom_b2(b2: bool) -> MojomValue {
    MojomValue::Union(4, Box::new(MojomValue::Bool(b2)))
}

fn test_union_mojom_em1(em1: MojomValue) -> MojomValue {
    MojomValue::Union(5, Box::new(em1))
}

fn test_union_mojom_f1(f1: MojomValue) -> MojomValue {
    MojomValue::Union(6, Box::new(f1))
}

// Mojom Definition:
// union NestedUnion {
//   int32 n;
//   BaseUnion u;
// }
static NESTED_UNION_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "NestedUnion",
    base_type: MojomType::Union {
        variants: [(0, MojomType::Int32), (1, BASE_UNION_TY.base_type.clone())].into(),
    },
    packed_type: MojomWireType::Union {
        ordinal: 0,
        variants: [
            (0, MojomWireType::Leaf { ordinal: 0, leaf_type: PackedLeafType::Int32 }),
            (1, BASE_UNION_TY.as_union_field()),
        ]
        .into(),
    },
});

fn nested_union_mojom_n(n: i32) -> MojomValue {
    MojomValue::Union(0, Box::new(MojomValue::Int32(n)))
}

fn nested_union_mojom_u(u: MojomValue) -> MojomValue {
    MojomValue::Union(1, Box::new(u))
}

// Mojom Definition:
// struct WithNestedUnion {
//   int64 n1;
//   NestedUnion u;
//   int32 n2;
// }
static WITH_NESTED_UNION_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "WithNestedUnion",
    base_type: wrap_struct_fields_type(vec![
        ("n1".to_string(), MojomType::Int64),
        ("u".to_string(), NESTED_UNION_TY.base_type.clone()),
        ("n2".to_string(), MojomType::Int32),
    ]),
    packed_type: wrap_packed_struct_fields(vec![
        ("n1".to_string(), MojomWireType::Leaf { ordinal: 0, leaf_type: PackedLeafType::Int64 }),
        ("u".to_string(), NESTED_UNION_TY.as_struct_field(1)),
        ("n2".to_string(), MojomWireType::Leaf { ordinal: 2, leaf_type: PackedLeafType::Int32 }),
    ]),
});

fn with_nested_union_mojom(n1: i64, u: MojomValue, n2: i32) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("n1".to_string(), MojomValue::Int64(n1)),
        ("u".to_string(), u),
        ("n2".to_string(), MojomValue::Int32(n2)),
    ])
}

// Mojom Definition:
// union NestederUnion {
//   bool b;
//   int8 n;
//   NestedUnion u;
//   WithNestedUnion w;
// };
static NESTEDER_UNION_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "NestederUnion",
    base_type: MojomType::Union {
        variants: [
            (0, MojomType::Bool),
            (1, MojomType::Int8),
            (2, NESTED_UNION_TY.base_type.clone()),
            (3, WITH_NESTED_UNION_TY.base_type.clone()),
        ]
        .into(),
    },
    packed_type: MojomWireType::Union {
        ordinal: 0,
        variants: [
            (
                0,
                MojomWireType::Bitfield {
                    ordinals: [Some(0), None, None, None, None, None, None, None],
                },
            ),
            (1, MojomWireType::Leaf { ordinal: 0, leaf_type: PackedLeafType::Int8 }),
            (2, NESTED_UNION_TY.as_union_field()),
            (3, WITH_NESTED_UNION_TY.as_union_field()),
        ]
        .into(),
    },
});

fn nesteder_union_mojom_b(b: bool) -> MojomValue {
    MojomValue::Union(0, Box::new(MojomValue::Bool(b)))
}

fn nesteder_union_mojom_n(n: i8) -> MojomValue {
    MojomValue::Union(1, Box::new(MojomValue::Int8(n)))
}

fn nesteder_union_mojom_u(u: MojomValue) -> MojomValue {
    MojomValue::Union(2, Box::new(u))
}

fn nesteder_union_mojom_w(w: MojomValue) -> MojomValue {
    MojomValue::Union(3, Box::new(w))
}

// Mojom Definition:
// struct WithManyUnions {
//   NestedUnion u1;
//   int8 i1;
//   NestederUnion u2;
//   int64 i2;
//   BaseUnion u3;
//   int32 i3;
//   int32 i4;
// };
static WITH_MANY_UNIONS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "WithManyUnions",
    base_type: wrap_struct_fields_type(vec![
        ("u1".to_string(), NESTED_UNION_TY.base_type.clone()),
        ("i1".to_string(), MojomType::Int8),
        ("u2".to_string(), NESTEDER_UNION_TY.base_type.clone()),
        ("i2".to_string(), MojomType::Int64),
        ("u3".to_string(), BASE_UNION_TY.base_type.clone()),
        ("u4".to_string(), NESTEDER_UNION_TY.base_type.clone()),
        ("i3".to_string(), MojomType::Int32),
        ("i4".to_string(), MojomType::Int32),
    ]),
    packed_type: wrap_packed_struct_fields(vec![
        ("u1".to_string(), NESTED_UNION_TY.as_struct_field(0)),
        ("i1".to_string(), MojomWireType::Leaf { ordinal: 1, leaf_type: PackedLeafType::Int8 }),
        ("i3".to_string(), MojomWireType::Leaf { ordinal: 6, leaf_type: PackedLeafType::Int32 }),
        ("u2".to_string(), NESTEDER_UNION_TY.as_struct_field(2)),
        ("i2".to_string(), MojomWireType::Leaf { ordinal: 3, leaf_type: PackedLeafType::Int64 }),
        ("u3".to_string(), BASE_UNION_TY.as_struct_field(4)),
        ("u4".to_string(), NESTEDER_UNION_TY.as_struct_field(5)),
        ("i4".to_string(), MojomWireType::Leaf { ordinal: 7, leaf_type: PackedLeafType::Int32 }),
    ]),
});

fn with_many_unions_mojom(
    u1: MojomValue,
    i1: i8,
    u2: MojomValue,
    i2: i64,
    u3: MojomValue,
    u4: MojomValue,
    i3: i32,
    i4: i32,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("u1".to_string(), u1),
        ("i1".to_string(), MojomValue::Int8(i1)),
        ("u2".to_string(), u2),
        ("i2".to_string(), MojomValue::Int64(i2)),
        ("u3".to_string(), u3),
        ("u4".to_string(), u4),
        ("i3".to_string(), MojomValue::Int32(i3)),
        ("i4".to_string(), MojomValue::Int32(i4)),
    ])
}

#[gtest(MojomParser, TestUnions)]
fn test_unions() {
    BASE_UNION_TY.validate_mojomparse(BaseUnion::n1(10), test_union_mojom_n1(10));
    BASE_UNION_TY.validate_mojomparse(BaseUnion::u1(987654321), test_union_mojom_u1(987654321));
    BASE_UNION_TY.validate_mojomparse(BaseUnion::e1(TestEnum::Three), test_union_mojom_e1(3));
    BASE_UNION_TY.validate_mojomparse(BaseUnion::b1(false), test_union_mojom_b1(false));
    BASE_UNION_TY.validate_mojomparse(BaseUnion::b2(true), test_union_mojom_b2(true));
    BASE_UNION_TY
        .validate_mojomparse(BaseUnion::em1(Empty {}), test_union_mojom_em1(empty_mojom()));
    BASE_UNION_TY.validate_mojomparse(
        BaseUnion::f1(FourInts { a: 5, b: 6, c: 7, d: 8 }),
        test_union_mojom_f1(four_ints_mojom(5, 6, 7, 8)),
    );

    expect_true!(BaseUnion::try_from(MojomValue::Union(99, Box::new(MojomValue::Int8(0)))).is_err());
    expect_true!(
        BaseUnion::try_from(MojomValue::Union(0, Box::new(MojomValue::UInt64(0)))).is_err()
    );

    NESTED_UNION_TY.validate_mojomparse(NestedUnion::n(60), nested_union_mojom_n(60));
    NESTED_UNION_TY.validate_mojomparse(
        NestedUnion::u(BaseUnion::n1(70)),
        nested_union_mojom_u(test_union_mojom_n1(70)),
    );

    WITH_NESTED_UNION_TY.validate_mojomparse(
        WithNestedUnion { n1: 800, u: NestedUnion::n(90), n2: 1000 },
        with_nested_union_mojom(800, nested_union_mojom_n(90), 1000),
    );

    NESTEDER_UNION_TY.validate_mojomparse(NestederUnion::b(false), nesteder_union_mojom_b(false));
    NESTEDER_UNION_TY.validate_mojomparse(NestederUnion::n(-1), nesteder_union_mojom_n(-1));
    NESTEDER_UNION_TY.validate_mojomparse(
        NestederUnion::u(NestedUnion::n(123)),
        nesteder_union_mojom_u(nested_union_mojom_n(123)),
    );
    NESTEDER_UNION_TY.validate_mojomparse(
        NestederUnion::w(WithNestedUnion { n1: 1000, u: NestedUnion::n(-50), n2: 200 }),
        nesteder_union_mojom_w(with_nested_union_mojom(1000, nested_union_mojom_n(-50), 200)),
    );

    WITH_MANY_UNIONS_TY.validate_mojomparse(
        WithManyUnions {
            u1: NestedUnion::n(50),
            i1: 11,
            u2: NestederUnion::b(false),
            i2: 22,
            u3: BaseUnion::n1(55),
            u4: NestederUnion::n(12),
            i3: 33,
            i4: 44,
        },
        with_many_unions_mojom(
            nested_union_mojom_n(50),
            11,
            nesteder_union_mojom_b(false),
            22,
            test_union_mojom_n1(55),
            nesteder_union_mojom_n(12),
            33,
            44,
        ),
    );
}
