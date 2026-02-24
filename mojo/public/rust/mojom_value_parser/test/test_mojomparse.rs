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
    "//mojo/public/rust/mojom_value_parser:mojom_value_parser_core";
    "//mojo/public/rust/mojom_value_parser:parser_unittests_rust";
}

use rust_gtest_interop::prelude::*;

use mojom_value_parser_core::*;
use ordered_float::OrderedFloat;
use parser_unittests_rust::parser_unittests::*;
use std::collections::HashMap;
use std::sync::LazyLock;

use crate::helpers::*;

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

fn wrap_packed_struct_fields(
    fields: Vec<(String, StructuredBodyElementOwned)>,
    num_elements_in_value: usize,
) -> MojomWireType {
    let (packed_field_names, packed_field_types) = fields.into_iter().unzip();
    MojomWireType::Pointer {
        nested_data_type: PackedStructuredType::Struct {
            packed_field_names,
            packed_field_types,
            num_elements_in_value,
        },
        is_nullable: false,
    }
}

// Helper macros since otherwise the lines get too long
// and it's harder to read the test cases
macro_rules! bare_leaf {
    ($leaf_ty:expr) => {
        MojomWireType::Leaf { leaf_type: $leaf_ty, is_nullable: false }
    };
    ($leaf_ty:expr, $nullable:expr) => {
        MojomWireType::Leaf { leaf_type: $leaf_ty, is_nullable: $nullable }
    };
}

macro_rules! struct_leaf {
    ($ord:expr, $leaf_ty:expr) => {
        StructuredBodyElement::SingleValue(
            $ord,
            MojomWireType::Leaf { leaf_type: $leaf_ty, is_nullable: false },
        )
    };
    ($ord:expr, $leaf_ty:expr, $nullable:expr) => {
        StructuredBodyElement::SingleValue(
            $ord,
            MojomWireType::Leaf { leaf_type: $leaf_ty, is_nullable: $nullable },
        )
    };
}

impl TestType {
    /// Return the packed version of this type as a struct field with the given
    /// ordinal
    fn as_struct_field(&self, ordinal: Ordinal) -> StructuredBodyElementOwned {
        StructuredBodyElement::SingleValue(ordinal, self.packed_type.clone())
    }

    /// Return the packed version of this type as a union field
    fn as_union_field(&self) -> MojomWireType {
        match self.packed_type.clone() {
            // Nested unions are represented as pointers
            MojomWireType::Union { variants, is_nullable } => MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Union { variants },
                is_nullable,
            },
            // Everything else is represented as itself.
            _ => self.packed_type.clone(),
        }
    }

    // These are separate functions mostly to avoid cluttering all the existing
    // calls to as_struct_field with an extra, unused parameter
    fn as_nullable_struct_field(&self, ordinal: Ordinal) -> StructuredBodyElementOwned {
        StructuredBodyElement::SingleValue(ordinal, self.packed_type.clone().make_nullable())
    }

    fn as_nullable_union_field(&self) -> MojomWireType {
        self.as_union_field().make_nullable()
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
            self.packed_type,
            "Type {} failed to pack correctly!",
            self.type_name
        );

        // Test conversion to/from MojomValues
        expect_eq!(&mojom_val, &rust_val.clone().into());
        expect_eq!(rust_val, mojom_val.try_into().unwrap());
    }

    /// Similar to `validate_mojomparse`, but for types which contain handles.
    /// Since no two different handle values will ever be equal, it uses a
    /// special comparison operator that ignores handles.
    fn validate_mojomparse_handles<T: MojomParse + std::fmt::Debug + PartialEq>(
        &self,
        rust_val: T,
        get_mojom_val: impl Fn() -> MojomValue,
    ) {
        expect_eq!(
            T::mojom_type(),
            self.base_type,
            "Type {} had the wrong associated MojomType!",
            self.type_name
        );

        expect_eq!(
            *T::wire_type(),
            self.packed_type,
            "Type {} failed to pack correctly!",
            self.type_name
        );

        expect_true!(equivalent_value(&get_mojom_val(), &rust_val.into()));
        // Unfortunately, we don't have `equivalent_value` for rust types.
        // We could write a bunch of them if we _really_ wanted to, but for now
        // we satisfy ourselves by checking round-trip conversions.
        expect_true!(equivalent_value(
            &get_mojom_val(),
            // The function we really want to test here is T::try_from
            &T::try_from(get_mojom_val()).unwrap().into()
        ));
    }
}

// We'll be creating all our test types at global scope, so we can reference
// them from various testing functions.

// Mojom Definition:
// struct Empty {};
static EMPTY_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "Empty",
    base_type: wrap_struct_fields_type(vec![]),
    packed_type: wrap_packed_struct_fields(vec![], 0),
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
    packed_type: wrap_packed_struct_fields(
        vec![
            ("a".to_string(), struct_leaf!(0, PackedLeafType::Int8)),
            ("b".to_string(), struct_leaf!(1, PackedLeafType::Int16)),
            ("c".to_string(), struct_leaf!(2, PackedLeafType::Int32)),
            ("d".to_string(), struct_leaf!(3, PackedLeafType::Int64)),
        ],
        4,
    ),
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
    packed_type: wrap_packed_struct_fields(
        vec![
            ("d".to_string(), struct_leaf!(0, PackedLeafType::Int64)),
            ("c".to_string(), struct_leaf!(1, PackedLeafType::Int32)),
            ("b".to_string(), struct_leaf!(2, PackedLeafType::Int16)),
            ("a".to_string(), struct_leaf!(3, PackedLeafType::Int8)),
        ],
        4,
    ),
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
    packed_type: wrap_packed_struct_fields(
        vec![
            ("a".to_string(), struct_leaf!(0, PackedLeafType::Int8)),
            ("d".to_string(), struct_leaf!(3, PackedLeafType::Int8)),
            ("c".to_string(), struct_leaf!(2, PackedLeafType::Int16)),
            ("b".to_string(), struct_leaf!(1, PackedLeafType::Int32)),
        ],
        4,
    ),
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
    packed_type: wrap_packed_struct_fields(
        vec![
            ("f1".to_string(), FOUR_INTS_TY.as_struct_field(0)),
            ("a".to_string(), struct_leaf!(1, PackedLeafType::UInt32)),
            ("b".to_string(), struct_leaf!(2, PackedLeafType::Int8)),
            ("c".to_string(), struct_leaf!(5, PackedLeafType::UInt16)),
            ("f2".to_string(), FOUR_INTS_REVERSED_TY.as_struct_field(3)),
            ("f3".to_string(), FOUR_INTS_INTERMIXED_TY.as_struct_field(4)),
        ],
        6,
    ),
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
    packed_type: wrap_packed_struct_fields(
        vec![
            ("o".to_string(), ONCE_NESTED_TY.as_struct_field(0)),
            ("a".to_string(), struct_leaf!(1, PackedLeafType::Int16)),
            ("b".to_string(), struct_leaf!(3, PackedLeafType::Int32)),
            ("f".to_string(), FOUR_INTS_TY.as_struct_field(2)),
            ("c".to_string(), struct_leaf!(4, PackedLeafType::Int32)),
        ],
        5,
    ),
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
    packed_type: wrap_packed_struct_fields(
        vec![
            (
                "b0".to_string(),
                StructuredBodyElement::Bitfield([
                    Some((0, false)),
                    Some((1, false)),
                    Some((2, false)),
                    Some((3, false)),
                    Some((4, false)),
                    Some((6, false)),
                    Some((7, false)),
                    Some((8, false)),
                ]),
            ),
            ("n1".to_string(), struct_leaf!(5, PackedLeafType::UInt8)),
            (
                "b8".to_string(),
                StructuredBodyElement::Bitfield([
                    Some((9, false)),
                    Some((10, false)),
                    None,
                    None,
                    None,
                    None,
                    None,
                    None,
                ]),
            ),
        ],
        11,
    ),
});

#[allow(clippy::too_many_arguments)]
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
    packed_type: wrap_packed_struct_fields(
        vec![
            (
                "b0".to_string(),
                StructuredBodyElement::Bitfield([
                    Some((0, false)),
                    Some((1, false)),
                    Some((2, false)),
                    Some((3, false)),
                    Some((4, false)),
                    Some((6, false)),
                    Some((7, false)),
                    Some((8, false)),
                ]),
            ),
            (
                "b8".to_string(),
                StructuredBodyElement::Bitfield([
                    Some((9, false)),
                    Some((10, false)),
                    None,
                    None,
                    None,
                    None,
                    None,
                    None,
                ]),
            ),
            ("n1".to_string(), struct_leaf!(5, PackedLeafType::UInt16)),
        ],
        11,
    ),
});

#[allow(clippy::too_many_arguments)]
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

#[gtest(RustTestMojomParsingAttr, TestMojomParseDerive)]
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
    let once_nested_mojom_val = || {
        once_nested_mojom(
            four_ints_mojom(1, 2, 3, 4),
            13,
            14,
            four_ints_reversed_mojom(5, 6, 7, 8),
            four_ints_intermixed_mojom(9, 10, 11, 12),
            15,
        )
    };
    ONCE_NESTED_TY.validate_mojomparse(once_nested_val.clone(), once_nested_mojom_val());

    TWICE_NESTED_TY.validate_mojomparse(
        TwiceNested {
            o: once_nested_val,
            a: 16,
            f: FourInts { a: 17, b: 18, c: 19, d: 20 },
            b: 21,
            c: 22,
        },
        twice_nested_mojom(once_nested_mojom_val(), 16, four_ints_mojom(17, 18, 19, 20), 21, 22),
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

#[gtest(RustTestMojomParsingAttr, TestBadConversion)]
/// Test that we can't convert between incompatible types
fn test_bad_conversion() {
    let empty = empty_mojom();

    let four_ints = four_ints_mojom(10, -20, 400, -8000);

    let four_ints_reversed = four_ints_reversed_mojom(10, -20, 400, -8000);

    let four_ints_intermixed = four_ints_intermixed_mojom(10, 400, -20, -5);

    let once_nested = || {
        once_nested_mojom(
            four_ints_mojom(1, 2, 3, 4),
            13,
            14,
            four_ints_reversed_mojom(5, 6, 7, 8),
            four_ints_intermixed_mojom(9, 10, 11, 12),
            15,
        )
    };

    let twice_nested =
        twice_nested_mojom(once_nested(), 16, four_ints_mojom(17, 18, 19, 20), 21, 22);

    let ten_bools_byte = || {
        ten_bools_and_a_byte_mojom(
            true, false, true, false, true, 200, false, true, false, true, false,
        )
    };
    let ten_bools_bytes = ten_bools_and_two_bytes_mojom(
        true, false, true, false, true, 50000, false, true, false, true, false,
    );

    // There are far too many bad paths to test them all, so this is just an
    // arbitrary scattershot approach so we have _some_ coverage.
    // TODO(crbug.com/456214728) Replace with matchers from the googletest crate
    // if we switch to it.
    expect_true!(FourInts::try_from(four_ints_reversed).is_err());
    expect_true!(FourInts::try_from(once_nested()).is_err());
    expect_true!(FourInts::try_from(empty).is_err());

    expect_true!(TenBoolsAndAByte::try_from(ten_bools_bytes).is_err());
    expect_true!(TenBoolsAndAByte::try_from(four_ints_intermixed).is_err());

    expect_true!(OnceNested::try_from(twice_nested).is_err());
    expect_true!(OnceNested::try_from(ten_bools_byte()).is_err());

    expect_true!(Empty::try_from(ten_bools_byte()).is_err());
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
    packed_type: wrap_packed_struct_fields(
        vec![
            ("e1".to_string(), struct_leaf!(0, PackedLeafType::Enum { is_valid: TEST_ENUM_PRED })),
            ("e2".to_string(), struct_leaf!(2, PackedLeafType::Enum { is_valid: TEST_ENUM2_PRED })),
            ("n1".to_string(), struct_leaf!(1, PackedLeafType::UInt64)),
        ],
        3,
    ),
});

fn some_enums_mojom(e1: u32, n1: u64, e2: u32) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("e1".to_string(), MojomValue::Enum(e1)),
        ("n1".to_string(), MojomValue::UInt64(n1)),
        ("e2".to_string(), MojomValue::Enum(e2)),
    ])
}

#[gtest(RustTestMojomParsingAttr, TestEnums)]
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
            (7, MojomType::Float32),
        ]
        .into(),
    },
    packed_type: MojomWireType::Union {
        variants: [
            (0, bare_leaf!(PackedLeafType::Int8)),
            (1, bare_leaf!(PackedLeafType::UInt64)),
            (2, bare_leaf!(PackedLeafType::Enum { is_valid: TEST_ENUM_PRED })),
            (3, bare_leaf!(PackedLeafType::Bool)),
            (4, bare_leaf!(PackedLeafType::Bool)),
            (5, EMPTY_TY.as_union_field()),
            (6, FOUR_INTS_TY.as_union_field()),
            (7, bare_leaf!(PackedLeafType::Float32)),
        ]
        .into(),
        is_nullable: false,
    },
});

fn base_union_mojom_n1(n1: i8) -> MojomValue {
    MojomValue::Union(0, Box::new(MojomValue::Int8(n1)))
}

fn base_union_mojom_u1(u1: u64) -> MojomValue {
    MojomValue::Union(1, Box::new(MojomValue::UInt64(u1)))
}

fn base_union_mojom_e1(e1: u32) -> MojomValue {
    MojomValue::Union(2, Box::new(MojomValue::Enum(e1)))
}

fn base_union_mojom_b1(b1: bool) -> MojomValue {
    MojomValue::Union(3, Box::new(MojomValue::Bool(b1)))
}

fn base_union_mojom_b2(b2: bool) -> MojomValue {
    MojomValue::Union(4, Box::new(MojomValue::Bool(b2)))
}

fn base_union_mojom_em1(em1: MojomValue) -> MojomValue {
    MojomValue::Union(5, Box::new(em1))
}

fn base_union_mojom_f1(f1: MojomValue) -> MojomValue {
    MojomValue::Union(6, Box::new(f1))
}

fn base_union_mojom_fl(fl: f32) -> MojomValue {
    MojomValue::Union(7, Box::new(MojomValue::Float32(fl.into())))
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
        variants: [(0, bare_leaf!(PackedLeafType::Int32)), (1, BASE_UNION_TY.as_union_field())]
            .into(),
        is_nullable: false,
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
    packed_type: wrap_packed_struct_fields(
        vec![
            ("n1".to_string(), struct_leaf!(0, PackedLeafType::Int64)),
            ("u".to_string(), NESTED_UNION_TY.as_struct_field(1)),
            ("n2".to_string(), struct_leaf!(2, PackedLeafType::Int32)),
        ],
        3,
    ),
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
        variants: [
            (0, bare_leaf!(PackedLeafType::Bool)),
            (1, bare_leaf!(PackedLeafType::Int8)),
            (2, NESTED_UNION_TY.as_union_field()),
            (3, WITH_NESTED_UNION_TY.as_union_field()),
        ]
        .into(),
        is_nullable: false,
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
//   double d1;
//   BaseUnion u3;
//   NestederUnion u4;
//   int32 i2;
//   int32 i3;
// };
static WITH_MANY_UNIONS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "WithManyUnions",
    base_type: wrap_struct_fields_type(vec![
        ("u1".to_string(), NESTED_UNION_TY.base_type.clone()),
        ("i1".to_string(), MojomType::Int8),
        ("u2".to_string(), NESTEDER_UNION_TY.base_type.clone()),
        ("d1".to_string(), MojomType::Float64),
        ("u3".to_string(), BASE_UNION_TY.base_type.clone()),
        ("u4".to_string(), NESTEDER_UNION_TY.base_type.clone()),
        ("i2".to_string(), MojomType::Int32),
        ("i3".to_string(), MojomType::Int32),
    ]),
    packed_type: wrap_packed_struct_fields(
        vec![
            ("u1".to_string(), NESTED_UNION_TY.as_struct_field(0)),
            ("i1".to_string(), struct_leaf!(1, PackedLeafType::Int8)),
            ("i2".to_string(), struct_leaf!(6, PackedLeafType::Int32)),
            ("u2".to_string(), NESTEDER_UNION_TY.as_struct_field(2)),
            ("d1".to_string(), struct_leaf!(3, PackedLeafType::Float64)),
            ("u3".to_string(), BASE_UNION_TY.as_struct_field(4)),
            ("u4".to_string(), NESTEDER_UNION_TY.as_struct_field(5)),
            ("i3".to_string(), struct_leaf!(7, PackedLeafType::Int32)),
        ],
        8,
    ),
});

#[allow(clippy::too_many_arguments)]
fn with_many_unions_mojom(
    u1: MojomValue,
    i1: i8,
    u2: MojomValue,
    d1: f64,
    u3: MojomValue,
    u4: MojomValue,
    i2: i32,
    i3: i32,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("u1".to_string(), u1),
        ("i1".to_string(), MojomValue::Int8(i1)),
        ("u2".to_string(), u2),
        ("d1".to_string(), MojomValue::Float64(d1.into())),
        ("u3".to_string(), u3),
        ("u4".to_string(), u4),
        ("i2".to_string(), MojomValue::Int32(i2)),
        ("i3".to_string(), MojomValue::Int32(i3)),
    ])
}

#[gtest(RustTestMojomParsingAttr, TestUnions)]
fn test_unions() {
    BASE_UNION_TY.validate_mojomparse(BaseUnion::n1(10), base_union_mojom_n1(10));
    BASE_UNION_TY.validate_mojomparse(BaseUnion::u1(987654321), base_union_mojom_u1(987654321));
    BASE_UNION_TY.validate_mojomparse(BaseUnion::e1(TestEnum::Three), base_union_mojom_e1(3));
    BASE_UNION_TY.validate_mojomparse(BaseUnion::b1(false), base_union_mojom_b1(false));
    BASE_UNION_TY.validate_mojomparse(BaseUnion::b2(true), base_union_mojom_b2(true));
    BASE_UNION_TY
        .validate_mojomparse(BaseUnion::em1(Empty {}), base_union_mojom_em1(empty_mojom()));
    BASE_UNION_TY.validate_mojomparse(
        BaseUnion::f1(FourInts { a: 5, b: 6, c: 7, d: 8 }),
        base_union_mojom_f1(four_ints_mojom(5, 6, 7, 8)),
    );
    BASE_UNION_TY.validate_mojomparse(BaseUnion::fl(3.14), base_union_mojom_fl(3.14));

    expect_true!(BaseUnion::try_from(MojomValue::Union(99, Box::new(MojomValue::Int8(0)))).is_err());
    expect_true!(
        BaseUnion::try_from(MojomValue::Union(0, Box::new(MojomValue::UInt64(0)))).is_err()
    );

    NESTED_UNION_TY.validate_mojomparse(NestedUnion::n(60), nested_union_mojom_n(60));
    NESTED_UNION_TY.validate_mojomparse(
        NestedUnion::u(BaseUnion::n1(70)),
        nested_union_mojom_u(base_union_mojom_n1(70)),
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
            d1: 3.14159,
            u3: BaseUnion::n1(55),
            u4: NestederUnion::n(12),
            i2: 33,
            i3: 44,
        },
        with_many_unions_mojom(
            nested_union_mojom_n(50),
            11,
            nesteder_union_mojom_b(false),
            3.14159,
            base_union_mojom_n1(55),
            nesteder_union_mojom_n(12),
            33,
            44,
        ),
    )
}

macro_rules! array {
    ($element_type:expr, $num_elements:expr) => {
        MojomType::Array { element_type: Box::new($element_type), num_elements: $num_elements }
    };
}

macro_rules! packed_array {
    ($element_type:expr, $num_elements:expr) => {
        MojomWireType::Pointer {
            nested_data_type: PackedStructuredType::Array {
                element_type: std::sync::Arc::new($element_type),
                array_type: if let Some(n) = $num_elements {
                    PackedArrayType::SizedArray(n)
                } else {
                    PackedArrayType::UnsizedArray
                },
            },
            is_nullable: false,
        }
    };
}

// Mojom Definition:
// array<int16>
static ARRAY_INT16_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<int16>",
    base_type: array!(MojomType::Int16, None),
    packed_type: packed_array!(bare_leaf!(PackedLeafType::Int16), None),
});

fn array_int16_mojom(elts: Vec<i16>) -> MojomValue {
    MojomValue::Array(elts.into_iter().map(MojomValue::Int16).collect())
}

// Mojom Definition:
// array<uint64, 3>
static ARRAY_UINT64_SIZED_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<uint64, 3>",
    base_type: array!(MojomType::UInt64, Some(3)),
    packed_type: packed_array!(bare_leaf!(PackedLeafType::UInt64), Some(3)),
});

fn array_uint64_sized_mojom(elts: [u64; 3]) -> MojomValue {
    MojomValue::Array(elts.into_iter().map(MojomValue::UInt64).collect())
}

// Mojom Definition:
// array<bool>
static ARRAY_BOOL_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<bool>",
    base_type: array!(MojomType::Bool, None),
    packed_type: packed_array!(bare_leaf!(PackedLeafType::Bool), None),
});

fn array_bool_mojom(elts: Vec<bool>) -> MojomValue {
    MojomValue::Array(elts.into_iter().map(MojomValue::Bool).collect())
}

// Mojom Definition:
// array<bool, 13>
static ARRAY_BOOL_SIZED_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<bool, 13>",
    base_type: array!(MojomType::Bool, Some(13)),
    packed_type: packed_array!(bare_leaf!(PackedLeafType::Bool), Some(13)),
});

fn array_bool_sized_mojom(elts: [bool; 13]) -> MojomValue {
    MojomValue::Array(elts.into_iter().map(MojomValue::Bool).collect())
}

// Mojom Definition:
// array<TestEnum>
static ARRAY_ENUM_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<TestEnum>",
    base_type: array!(MojomType::Enum { is_valid: TEST_ENUM_PRED }, None),
    packed_type: packed_array!(bare_leaf!(PackedLeafType::Enum { is_valid: TEST_ENUM_PRED }), None),
});

fn array_enum_mojom(elts: Vec<TestEnum>) -> MojomValue {
    MojomValue::Array(elts.into_iter().map(|e| MojomValue::Enum(e.into())).collect())
}

// Mojom Definition:
// array<BaseUnion>
static ARRAY_UNION_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<BaseUnion>",
    base_type: array!(BASE_UNION_TY.base_type.clone(), None),
    packed_type: packed_array!(BASE_UNION_TY.packed_type.clone(), None),
});

fn array_union_mojom(elts: Vec<BaseUnion>) -> MojomValue {
    MojomValue::Array(elts.into_iter().map(MojomValue::from).collect())
}

// Mojom Definition:
// array<NestedUnion>
static ARRAY_UNION_NESTED_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<NestedUnion>",
    base_type: array!(NESTED_UNION_TY.base_type.clone(), None),
    packed_type: packed_array!(NESTED_UNION_TY.packed_type.clone(), None),
});

fn array_union_nested_mojom(elts: Vec<NestedUnion>) -> MojomValue {
    MojomValue::Array(elts.into_iter().map(MojomValue::from).collect())
}

// Mojom Definition:
// array<FourInts>
static ARRAY_FOURINTS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<FourInts>",
    base_type: array!(FOUR_INTS_TY.base_type.clone(), None),
    packed_type: packed_array!(FOUR_INTS_TY.packed_type.clone(), None),
});

fn array_fourints_mojom(elts: Vec<FourInts>) -> MojomValue {
    MojomValue::Array(elts.into_iter().map(MojomValue::from).collect())
}

// Mojom Definition:
// array<array<uint8>>
static ARRAY_NESTED_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<array<uint8>>",
    base_type: array!(array!(MojomType::UInt8, None), None),
    packed_type: packed_array!(packed_array!(bare_leaf!(PackedLeafType::UInt8), None), None),
});

fn array_nested_mojom(elts: Vec<Vec<u8>>) -> MojomValue {
    MojomValue::Array(
        elts.into_iter()
            .map(|inner_vec| {
                MojomValue::Array(inner_vec.into_iter().map(MojomValue::UInt8).collect())
            })
            .collect(),
    )
}

// Mojom Definition:
// array<array<uint8, 2>, 3>
static ARRAY_NESTED_SIZED_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<array<uint8, 2>, 3>",
    base_type: array!(array!(MojomType::UInt8, Some(2)), Some(3)),
    packed_type: packed_array!(packed_array!(bare_leaf!(PackedLeafType::UInt8), Some(2)), Some(3)),
});

fn array_nested_sized_mojom(elts: [[u8; 2]; 3]) -> MojomValue {
    MojomValue::Array(
        elts.into_iter()
            .map(|inner_arr| {
                MojomValue::Array(inner_arr.into_iter().map(MojomValue::UInt8).collect())
            })
            .collect(),
    )
}

// Mojom Definition:
// array<float>
static ARRAY_FLOAT_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<float>",
    base_type: array!(MojomType::Float32, None),
    packed_type: packed_array!(bare_leaf!(PackedLeafType::Float32), None),
});

fn array_float_mojom(elts: Vec<f32>) -> MojomValue {
    MojomValue::Array(elts.into_iter().map(|e| MojomValue::Float32(e.into())).collect())
}

// Mojom Definition:
// struct Arrays {
//   array<int16> ints;
//   array<uint64, 3> ints_sized;
//   array<bool> bools;
//   array<bool, 13> bool_sized;
//   array<float> floats;
//   array<TestEnum> enums;
//   array<BaseUnion> unions;
//   array<NestedUnion> unions_nested;
//   array<FourInts> fourints;
//   array<array<uint8>> nested;
//   array<array<uint8, 2>, 3> nested_sized;
// };
static ARRAYS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "Arrays",
    base_type: wrap_struct_fields_type(vec![
        ("ints".to_string(), ARRAY_INT16_TY.base_type.clone()),
        ("ints_sized".to_string(), ARRAY_UINT64_SIZED_TY.base_type.clone()),
        ("bools".to_string(), ARRAY_BOOL_TY.base_type.clone()),
        ("bool_sized".to_string(), ARRAY_BOOL_SIZED_TY.base_type.clone()),
        ("floats".to_string(), ARRAY_FLOAT_TY.base_type.clone()),
        ("enums".to_string(), ARRAY_ENUM_TY.base_type.clone()),
        ("unions".to_string(), ARRAY_UNION_TY.base_type.clone()),
        ("unions_nested".to_string(), ARRAY_UNION_NESTED_TY.base_type.clone()),
        ("fourints".to_string(), ARRAY_FOURINTS_TY.base_type.clone()),
        ("nested".to_string(), ARRAY_NESTED_TY.base_type.clone()),
        ("nested_sized".to_string(), ARRAY_NESTED_SIZED_TY.base_type.clone()),
    ]),
    packed_type: wrap_packed_struct_fields(
        vec![
            (
                "ints".to_string(),
                StructuredBodyElement::SingleValue(0, ARRAY_INT16_TY.packed_type.clone()),
            ),
            (
                "ints_sized".to_string(),
                StructuredBodyElement::SingleValue(1, ARRAY_UINT64_SIZED_TY.packed_type.clone()),
            ),
            (
                "bools".to_string(),
                StructuredBodyElement::SingleValue(2, ARRAY_BOOL_TY.packed_type.clone()),
            ),
            (
                "bool_sized".to_string(),
                StructuredBodyElement::SingleValue(3, ARRAY_BOOL_SIZED_TY.packed_type.clone()),
            ),
            (
                "floats".to_string(),
                StructuredBodyElement::SingleValue(4, ARRAY_FLOAT_TY.packed_type.clone()),
            ),
            (
                "enums".to_string(),
                StructuredBodyElement::SingleValue(5, ARRAY_ENUM_TY.packed_type.clone()),
            ),
            (
                "unions".to_string(),
                StructuredBodyElement::SingleValue(6, ARRAY_UNION_TY.packed_type.clone()),
            ),
            (
                "unions_nested".to_string(),
                StructuredBodyElement::SingleValue(7, ARRAY_UNION_NESTED_TY.packed_type.clone()),
            ),
            (
                "fourints".to_string(),
                StructuredBodyElement::SingleValue(8, ARRAY_FOURINTS_TY.packed_type.clone()),
            ),
            (
                "nested".to_string(),
                StructuredBodyElement::SingleValue(9, ARRAY_NESTED_TY.packed_type.clone()),
            ),
            (
                "nested_sized".to_string(),
                StructuredBodyElement::SingleValue(10, ARRAY_NESTED_SIZED_TY.packed_type.clone()),
            ),
        ],
        11,
    ),
});

#[allow(clippy::too_many_arguments)]
fn arrays_mojom(
    ints: Vec<i16>,
    ints_sized: [u64; 3],
    bools: Vec<bool>,
    bool_sized: [bool; 13],
    floats: Vec<f32>,
    enums: Vec<TestEnum>,
    unions: Vec<BaseUnion>,
    unions_nested: Vec<NestedUnion>,
    fourints: Vec<FourInts>,
    nested: Vec<Vec<u8>>,
    nested_sized: [[u8; 2]; 3],
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("ints".to_string(), array_int16_mojom(ints)),
        ("ints_sized".to_string(), array_uint64_sized_mojom(ints_sized)),
        ("bools".to_string(), array_bool_mojom(bools)),
        ("bool_sized".to_string(), array_bool_sized_mojom(bool_sized)),
        ("floats".to_string(), array_float_mojom(floats)),
        ("enums".to_string(), array_enum_mojom(enums)),
        ("unions".to_string(), array_union_mojom(unions)),
        ("unions_nested".to_string(), array_union_nested_mojom(unions_nested)),
        ("fourints".to_string(), array_fourints_mojom(fourints)),
        ("nested".to_string(), array_nested_mojom(nested)),
        ("nested_sized".to_string(), array_nested_sized_mojom(nested_sized)),
    ])
}

#[gtest(RustTestMojomParsingAttr, TestArrays)]
fn test_arrays() {
    ARRAY_INT16_TY
        .validate_mojomparse::<Vec<i16>>(vec![1, -2, 3, -4], array_int16_mojom(vec![1, -2, 3, -4]));
    ARRAY_UINT64_SIZED_TY
        .validate_mojomparse::<[u64; 3]>([5, 6, 7], array_uint64_sized_mojom([5, 6, 7]));
    ARRAY_BOOL_TY.validate_mojomparse::<Vec<bool>>(
        vec![true, false, true, false, true, false, true, false, true],
        array_bool_mojom(vec![true, false, true, false, true, false, true, false, true]),
    );
    ARRAY_BOOL_SIZED_TY.validate_mojomparse::<[bool; 13]>(
        [true, true, false, true, false, false, true, true, false, true, false, false, true],
        array_bool_sized_mojom([
            true, true, false, true, false, false, true, true, false, true, false, false, true,
        ]),
    );
    ARRAY_FLOAT_TY.validate_mojomparse::<Vec<f32>>(
        vec![1.0, -2.0, 3.14],
        array_float_mojom(vec![1.0, -2.0, 3.14]),
    );
    ARRAY_ENUM_TY.validate_mojomparse::<Vec<TestEnum>>(
        vec![TestEnum::Zero, TestEnum::Seven, TestEnum::Four],
        array_enum_mojom(vec![TestEnum::Zero, TestEnum::Seven, TestEnum::Four]),
    );
    ARRAY_UNION_TY.validate_mojomparse::<Vec<BaseUnion>>(
        vec![BaseUnion::n1(10), BaseUnion::u1(20), BaseUnion::e1(TestEnum::Three)],
        array_union_mojom(vec![
            BaseUnion::n1(10),
            BaseUnion::u1(20),
            BaseUnion::e1(TestEnum::Three),
        ]),
    );
    ARRAY_UNION_NESTED_TY.validate_mojomparse::<Vec<NestedUnion>>(
        vec![NestedUnion::n(30), NestedUnion::u(BaseUnion::n1(40))],
        array_union_nested_mojom(vec![NestedUnion::n(30), NestedUnion::u(BaseUnion::n1(40))]),
    );
    ARRAY_FOURINTS_TY.validate_mojomparse::<Vec<FourInts>>(
        vec![FourInts { a: 1, b: 2, c: 3, d: 4 }, FourInts { a: 5, b: 6, c: 7, d: 8 }],
        array_fourints_mojom(vec![
            FourInts { a: 1, b: 2, c: 3, d: 4 },
            FourInts { a: 5, b: 6, c: 7, d: 8 },
        ]),
    );
    ARRAY_NESTED_TY.validate_mojomparse::<Vec<Vec<u8>>>(
        vec![vec![1, 2], vec![3, 4, 5]],
        array_nested_mojom(vec![vec![1, 2], vec![3, 4, 5]]),
    );
    ARRAY_NESTED_SIZED_TY.validate_mojomparse::<[[u8; 2]; 3]>(
        [[6, 7], [8, 9], [10, 11]],
        array_nested_sized_mojom([[6, 7], [8, 9], [10, 11]]),
    );

    let array_val = array_int16_mojom(vec![]);
    expect_true!(FourInts::try_from(array_val).is_err());

    expect_true!(<Vec<i16>>::try_from(empty_mojom()).is_err());
    expect_true!(<[u64; 3]>::try_from(empty_mojom()).is_err());

    ARRAYS_TY.validate_mojomparse(
        Arrays {
            ints: vec![101, -201, 301, -401],
            ints_sized: [501, 601, 701],
            bools: vec![false, true, false, true, false, true, false, true, false],
            bool_sized: [
                false, false, true, false, true, true, false, false, true, false, true, true, false,
            ],
            floats: vec![1.1, 2.2, 3.3],
            enums: vec![TestEnum::Four, TestEnum::Zero, TestEnum::Seven],
            unions: vec![BaseUnion::n1(12), BaseUnion::u1(22), BaseUnion::e1(TestEnum::Four)],
            unions_nested: vec![NestedUnion::n(32), NestedUnion::u(BaseUnion::n1(42))],
            fourints: vec![
                FourInts { a: 12, b: 22, c: 32, d: 42 },
                FourInts { a: 52, b: 62, c: 72, d: 82 },
            ],
            nested: vec![vec![11, 22], vec![33, 44, 55]],
            nested_sized: [[16, 17], [18, 19], [20, 21]],
        },
        arrays_mojom(
            vec![101, -201, 301, -401],
            [501, 601, 701],
            vec![false, true, false, true, false, true, false, true, false],
            [false, false, true, false, true, true, false, false, true, false, true, true, false],
            vec![1.1, 2.2, 3.3],
            vec![TestEnum::Four, TestEnum::Zero, TestEnum::Seven],
            vec![BaseUnion::n1(12), BaseUnion::u1(22), BaseUnion::e1(TestEnum::Four)],
            vec![NestedUnion::n(32), NestedUnion::u(BaseUnion::n1(42))],
            vec![FourInts { a: 12, b: 22, c: 32, d: 42 }, FourInts { a: 52, b: 62, c: 72, d: 82 }],
            vec![vec![11, 22], vec![33, 44, 55]],
            [[16, 17], [18, 19], [20, 21]],
        ),
    );
}

macro_rules! map {
    ($key_type:expr, $value_type:expr) => {
        MojomType::Map { key_type: Box::new($key_type), value_type: Box::new($value_type) }
    };
}

macro_rules! packed_map {
    ($key_type:expr, $value_type:expr) => {
        MojomWireType::Pointer {
            nested_data_type: PackedStructuredType::Map {
                key_type: std::sync::Arc::new($key_type),
                value_type: std::sync::Arc::new($value_type),
            },
            is_nullable: false,
        }
    };
}

// Mojom Definition:
// map<uint8, uint8>
static MAP_U8_U8_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "map<uint8, uint8>",
    base_type: map!(MojomType::UInt8, MojomType::UInt8),
    packed_type: packed_map!(bare_leaf!(PackedLeafType::UInt8), bare_leaf!(PackedLeafType::UInt8)),
});

fn map_u8_u8_mojom(elts: HashMap<u8, u8>) -> MojomValue {
    MojomValue::Map(
        elts.into_iter().map(|(k, v)| (MojomValue::UInt8(k), MojomValue::UInt8(v))).collect(),
    )
}

// Mojom Definition:
// map<bool, uint16>
static MAP_BOOL_U16_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "map<bool, uint16>",
    base_type: map!(MojomType::Bool, MojomType::UInt16),
    packed_type: packed_map!(bare_leaf!(PackedLeafType::Bool), bare_leaf!(PackedLeafType::UInt16)),
});

fn map_bool_u16_mojom(elts: HashMap<bool, u16>) -> MojomValue {
    MojomValue::Map(
        elts.into_iter().map(|(k, v)| (MojomValue::Bool(k), MojomValue::UInt16(v))).collect(),
    )
}

// Mojom Definition:
// map<TestEnum, int32>
static MAP_ENUM_I32_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "map<TestEnum, int32>",
    base_type: map!(MojomType::Enum { is_valid: TEST_ENUM_PRED }, MojomType::Int32),
    packed_type: packed_map!(
        bare_leaf!(PackedLeafType::Enum { is_valid: TEST_ENUM_PRED }),
        bare_leaf!(PackedLeafType::Int32)
    ),
});

fn map_enum_i32_mojom(elts: HashMap<TestEnum, i32>) -> MojomValue {
    MojomValue::Map(
        elts.into_iter().map(|(k, v)| (MojomValue::Enum(k.into()), MojomValue::Int32(v))).collect(),
    )
}

// Mojom Definition:
// map<int8, FourInts>
static MAP_I8_FOURINTS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "map<int8, FourInts>",
    base_type: map!(MojomType::Int8, FOUR_INTS_TY.base_type.clone()),
    packed_type: packed_map!(bare_leaf!(PackedLeafType::Int8), FOUR_INTS_TY.packed_type.clone()),
});

fn map_i8_fourints_mojom(elts: HashMap<i8, FourInts>) -> MojomValue {
    MojomValue::Map(
        elts.into_iter().map(|(k, v)| (MojomValue::Int8(k), MojomValue::from(v))).collect(),
    )
}

// Mojom Definition:
// map<int8, NestedUnion>
static MAP_I8_NESTEDUNION_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "map<int8, NestedUnion>",
    base_type: map!(MojomType::Int8, NESTED_UNION_TY.base_type.clone()),
    packed_type: packed_map!(bare_leaf!(PackedLeafType::Int8), NESTED_UNION_TY.packed_type.clone()),
});

fn map_i8_nestedunion_mojom(elts: HashMap<i8, NestedUnion>) -> MojomValue {
    MojomValue::Map(
        elts.into_iter().map(|(k, v)| (MojomValue::Int8(k), MojomValue::from(v))).collect(),
    )
}

// Mojom Definition:
// map<int8, map<int16, uint32>>
static MAP_I8_MAP_I16_U32_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "map<int8, map<int16, uint32>>",
    base_type: map!(MojomType::Int8, map!(MojomType::Int16, MojomType::UInt32)),
    packed_type: packed_map!(
        bare_leaf!(PackedLeafType::Int8),
        packed_map!(bare_leaf!(PackedLeafType::Int16), bare_leaf!(PackedLeafType::UInt32))
    ),
});

fn map_i8_map_i16_u32_mojom(elts: HashMap<i8, HashMap<i16, u32>>) -> MojomValue {
    MojomValue::Map(
        elts.into_iter().map(|(k, v)| (MojomValue::Int8(k), MojomValue::from(v))).collect(),
    )
}

// Mojom Definition:
// map<float, int32>
static MAP_FLOAT_I32_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "map<float, int32>",
    base_type: map!(MojomType::Float32, MojomType::Int32),
    packed_type: packed_map!(
        bare_leaf!(PackedLeafType::Float32),
        bare_leaf!(PackedLeafType::Int32)
    ),
});

fn map_float_i32_mojom(elts: HashMap<OrderedFloat<f32>, i32>) -> MojomValue {
    MojomValue::Map(
        elts.into_iter().map(|(k, v)| (MojomValue::Float32(k), MojomValue::Int32(v))).collect(),
    )
}

// Mojom Definition:
// struct Maps {
//   map<uint8, uint8> eights;
//   map<bool, uint16> bools;
//   map<TestEnum, int32> enums;
//   map<int8, FourInts> to_struct;
//   map<int8, NestedUnion> to_union;
//   map<int8, map<int16, uint32>> to_map;
//   map<float, int32> float_map;
// }
static MAPS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "Maps",
    base_type: wrap_struct_fields_type(vec![
        ("eights".to_string(), MAP_U8_U8_TY.base_type.clone()),
        ("bools".to_string(), MAP_BOOL_U16_TY.base_type.clone()),
        ("enums".to_string(), MAP_ENUM_I32_TY.base_type.clone()),
        ("to_struct".to_string(), MAP_I8_FOURINTS_TY.base_type.clone()),
        ("to_union".to_string(), MAP_I8_NESTEDUNION_TY.base_type.clone()),
        ("to_map".to_string(), MAP_I8_MAP_I16_U32_TY.base_type.clone()),
        ("float_map".to_string(), MAP_FLOAT_I32_TY.base_type.clone()),
    ]),
    packed_type: wrap_packed_struct_fields(
        vec![
            ("eights".to_string(), MAP_U8_U8_TY.as_struct_field(0)),
            ("bools".to_string(), MAP_BOOL_U16_TY.as_struct_field(1)),
            ("enums".to_string(), MAP_ENUM_I32_TY.as_struct_field(2)),
            ("to_struct".to_string(), MAP_I8_FOURINTS_TY.as_struct_field(3)),
            ("to_union".to_string(), MAP_I8_NESTEDUNION_TY.as_struct_field(4)),
            ("to_map".to_string(), MAP_I8_MAP_I16_U32_TY.as_struct_field(5)),
            ("float_map".to_string(), MAP_FLOAT_I32_TY.as_struct_field(6)),
        ],
        7,
    ),
});

fn maps_mojom(
    eights: HashMap<u8, u8>,
    bools: HashMap<bool, u16>,
    enums: HashMap<TestEnum, i32>,
    to_struct: HashMap<i8, FourInts>,
    to_union: HashMap<i8, NestedUnion>,
    to_map: HashMap<i8, HashMap<i16, u32>>,
    float_map: HashMap<OrderedFloat<f32>, i32>,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("eights".to_string(), map_u8_u8_mojom(eights)),
        ("bools".to_string(), map_bool_u16_mojom(bools)),
        ("enums".to_string(), map_enum_i32_mojom(enums)),
        ("to_struct".to_string(), map_i8_fourints_mojom(to_struct)),
        ("to_union".to_string(), map_i8_nestedunion_mojom(to_union)),
        ("to_map".to_string(), map_i8_map_i16_u32_mojom(to_map)),
        ("float_map".to_string(), map_float_i32_mojom(float_map)),
    ])
}

#[gtest(RustTestMojomParsingAttr, TestMaps)]
fn test_maps() {
    let eights_data = [(1, 2), (3, 4)];
    MAP_U8_U8_TY.validate_mojomparse::<HashMap<u8, u8>>(
        eights_data.into(),
        map_u8_u8_mojom(eights_data.into()),
    );

    let bools_data = [(true, 10), (false, 20)];
    MAP_BOOL_U16_TY.validate_mojomparse::<HashMap<bool, u16>>(
        bools_data.into(),
        map_bool_u16_mojom(bools_data.into()),
    );

    let enums_data = [(TestEnum::Zero, -1), (TestEnum::Seven, -2)];
    MAP_ENUM_I32_TY.validate_mojomparse::<HashMap<TestEnum, i32>>(
        enums_data.clone().into(),
        map_enum_i32_mojom(enums_data.clone().into()),
    );

    let to_struct_data = [(5, FourInts { a: 1, b: 2, c: 3, d: 4 })];
    MAP_I8_FOURINTS_TY.validate_mojomparse::<HashMap<i8, FourInts>>(
        to_struct_data.clone().into(),
        map_i8_fourints_mojom(to_struct_data.clone().into()),
    );

    let to_union_data = [(-8, NestedUnion::n(50)), (-9, NestedUnion::u(BaseUnion::n1(10)))];
    MAP_I8_NESTEDUNION_TY.validate_mojomparse::<HashMap<i8, NestedUnion>>(
        to_union_data.clone().into(),
        map_i8_nestedunion_mojom(to_union_data.clone().into()),
    );

    let to_map_data =
        [(1, [(10, 100), (20, 200)].into()), (2, [(30, 300), (40, 400), (50, 500)].into())];
    MAP_I8_MAP_I16_U32_TY.validate_mojomparse::<HashMap<i8, HashMap<i16, u32>>>(
        to_map_data.clone().into(),
        map_i8_map_i16_u32_mojom(to_map_data.clone().into()),
    );

    let float_map_data = [(1.1.into(), 10), (2.2.into(), 20)];
    MAP_FLOAT_I32_TY.validate_mojomparse::<HashMap<OrderedFloat<f32>, i32>>(
        float_map_data.into(),
        map_float_i32_mojom(float_map_data.into()),
    );

    MAPS_TY.validate_mojomparse(
        Maps {
            eights: eights_data.into(),
            bools: bools_data.into(),
            enums: enums_data.clone().into(),
            to_struct: to_struct_data.clone().into(),
            to_union: to_union_data.clone().into(),
            to_map: to_map_data.clone().into(),
            float_map: float_map_data.into(),
        },
        maps_mojom(
            eights_data.into(),
            bools_data.into(),
            enums_data.into(),
            to_struct_data.into(),
            to_union_data.into(),
            to_map_data.into(),
            float_map_data.into(),
        ),
    );
}

// Mojom definition: string
static STRING_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "string",
    base_type: MojomType::String,
    packed_type: MojomWireType::Pointer {
        nested_data_type: PackedStructuredType::Array {
            element_type: std::sync::Arc::new(MojomWireType::Leaf {
                leaf_type: PackedLeafType::UInt8,
                is_nullable: false,
            }),
            array_type: PackedArrayType::String,
        },
        is_nullable: false,
    },
});

// Mojom Definition: array<string>
static ARRAY_STRING_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<string>",
    base_type: array!(MojomType::String, None),
    packed_type: packed_array!(STRING_TY.packed_type.clone(), None),
});

// Mojom Definition: map<uint8, string>
static MAP_U8_STRING_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "map<uint8, string>",
    base_type: map!(MojomType::UInt8, MojomType::String),
    packed_type: packed_map!(bare_leaf!(PackedLeafType::UInt8), STRING_TY.packed_type.clone()),
});

// Mojom Definition: map<string, int16>
static MAP_STRING_I16_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "map<string, int16>",
    base_type: map!(MojomType::String, MojomType::Int16),
    packed_type: packed_map!(STRING_TY.packed_type.clone(), bare_leaf!(PackedLeafType::Int16)),
});

// Mojom Definition:
// struct Strings {
//   string str;
//   array<string> arr;
//   map<uint8, string> to_str;
//   map<string, int16> from_str;
//   HoldsComplexTypes u;
// }
static STRINGS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "Strings",
    base_type: wrap_struct_fields_type(vec![
        ("str".to_string(), STRING_TY.base_type.clone()),
        ("arr".to_string(), ARRAY_STRING_TY.base_type.clone()),
        ("to_str".to_string(), MAP_U8_STRING_TY.base_type.clone()),
        ("from_str".to_string(), MAP_STRING_I16_TY.base_type.clone()),
    ]),
    packed_type: wrap_packed_struct_fields(
        vec![
            ("str".to_string(), STRING_TY.as_struct_field(0)),
            ("arr".to_string(), ARRAY_STRING_TY.as_struct_field(1)),
            ("to_str".to_string(), MAP_U8_STRING_TY.as_struct_field(2)),
            ("from_str".to_string(), MAP_STRING_I16_TY.as_struct_field(3)),
        ],
        4,
    ),
});

fn mojomvalue_from_str(str: &str) -> MojomValue {
    MojomValue::String(str.to_string())
}

fn strings_mojom(
    str: &str,
    arr: Vec<&str>,
    to_str: HashMap<u8, &str>,
    from_str: HashMap<&str, i16>,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("str".to_string(), mojomvalue_from_str(str)),
        ("arr".to_string(), MojomValue::Array(arr.into_iter().map(mojomvalue_from_str).collect())),
        (
            "to_str".to_string(),
            MojomValue::Map(
                to_str
                    .into_iter()
                    .map(|(k, v)| (MojomValue::UInt8(k), mojomvalue_from_str(v)))
                    .collect(),
            ),
        ),
        (
            "from_str".to_string(),
            MojomValue::Map(
                from_str
                    .into_iter()
                    .map(|(k, v)| (mojomvalue_from_str(k), MojomValue::Int16(v)))
                    .collect(),
            ),
        ),
    ])
}

#[gtest(RustTestMojomParsingAttr, TestStrings)]
fn test_strings() {
    STRINGS_TY.validate_mojomparse(
        Strings {
            str: "test".to_string(),
            arr: vec!["a".to_string(), "b".to_string()],
            to_str: [(1, "one".to_string()), (2, "two".to_string())].into(),
            from_str: [("three".to_string(), 3), ("four".to_string(), 4)].into(),
        },
        strings_mojom(
            "test",
            vec!["a", "b"],
            [(1, "one"), (2, "two")].into(),
            [("three", 3), ("four", 4)].into(),
        ),
    );

    STRINGS_TY.validate_mojomparse(
        Strings {
            str: "".to_string(),
            arr: vec![],
            to_str: HashMap::new(),
            from_str: HashMap::new(),
        },
        strings_mojom("", vec![], HashMap::new(), HashMap::new()),
    );
}

// Mojom Definition:
// union HoldsComplexTypes {
//   string str;
//   array<int8> arr;
//   map<int8, int8> m;
// }
static HOLDS_COMPLEX_TYPES_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "HoldsComplexTypes",
    base_type: MojomType::Union {
        variants: [
            (0, STRING_TY.base_type.clone()),
            (1, ARRAY_INT16_TY.base_type.clone()),
            (2, MAP_U8_U8_TY.base_type.clone()),
        ]
        .into(),
    },
    packed_type: MojomWireType::Union {
        variants: [
            (0, STRING_TY.as_union_field()),
            (1, ARRAY_INT16_TY.as_union_field()),
            (2, MAP_U8_U8_TY.as_union_field()),
        ]
        .into(),
        is_nullable: false,
    },
});

fn holds_complex_types_mojom_str(str: &str) -> MojomValue {
    MojomValue::Union(0, Box::new(MojomValue::String(str.to_string())))
}

fn holds_complex_types_mojom_arr(arr: Vec<i16>) -> MojomValue {
    MojomValue::Union(
        1,
        Box::new(MojomValue::Array(arr.into_iter().map(MojomValue::Int16).collect())),
    )
}

fn holds_complex_types_mojom_m(map: HashMap<u8, u8>) -> MojomValue {
    MojomValue::Union(
        2,
        Box::new(MojomValue::Map(
            map.into_iter().map(|(k, v)| (MojomValue::UInt8(k), MojomValue::UInt8(v))).collect(),
        )),
    )
}

static COMPLEX_UNION_HOLDER_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "ComplexUnionHolder",
    base_type: wrap_struct_fields_type(vec![(
        "u".to_string(),
        HOLDS_COMPLEX_TYPES_TY.base_type.clone(),
    )]),
    packed_type: wrap_packed_struct_fields(
        vec![("u".to_string(), HOLDS_COMPLEX_TYPES_TY.as_struct_field(0))],
        1,
    ),
});

#[gtest(RustTestMojomParsingAttr, TestComplexUnion)]
fn test_complex_union() {
    // Test both the union and the union inside a struct
    HOLDS_COMPLEX_TYPES_TY.validate_mojomparse(
        HoldsComplexTypes::str("hello".to_string()),
        holds_complex_types_mojom_str("hello"),
    );

    COMPLEX_UNION_HOLDER_TY.validate_mojomparse(
        ComplexUnionHolder { u: HoldsComplexTypes::str("eek".to_string()) },
        wrap_struct_fields_value(vec![("u".to_string(), holds_complex_types_mojom_str("eek"))]),
    );

    HOLDS_COMPLEX_TYPES_TY.validate_mojomparse(
        HoldsComplexTypes::arr(vec![1, 2, 3]),
        holds_complex_types_mojom_arr(vec![1, 2, 3]),
    );

    COMPLEX_UNION_HOLDER_TY.validate_mojomparse(
        ComplexUnionHolder { u: HoldsComplexTypes::arr(vec![99, 222, 301, 282]) },
        wrap_struct_fields_value(vec![(
            "u".to_string(),
            holds_complex_types_mojom_arr(vec![99, 222, 301, 282]),
        )]),
    );

    HOLDS_COMPLEX_TYPES_TY.validate_mojomparse(
        HoldsComplexTypes::m([(1, 10), (2, 20)].into()),
        holds_complex_types_mojom_m([(1, 10), (2, 20)].into()),
    );

    COMPLEX_UNION_HOLDER_TY.validate_mojomparse(
        ComplexUnionHolder { u: HoldsComplexTypes::m([(19, 120), (29, 210)].into()) },
        wrap_struct_fields_value(vec![(
            "u".to_string(),
            holds_complex_types_mojom_m([(19, 120), (29, 210)].into()),
        )]),
    );
}

/// Wrap the type in `MojomType::Nullable` and a new box
macro_rules! nullable_ty {
    ($inner_ty:expr) => {
        MojomType::Nullable { inner_type: Box::new($inner_ty) }
    };
}

/// Wrap the (optional) value in `MojomValue::Nullable` and a new box
macro_rules! nullable_val {
    ($inner_val:expr) => {
        MojomValue::Nullable($inner_val.map(Box::new))
    };
}

// Mojom Definition:
// struct NullableBasics {
//   bool? b;
//   uint16? u16;
//   int8? i8;
//   Empty? empty;
//   TestEnum? e;
//   FourInts? fourints;
//   float? f1;
//   double? f2;
// }
static NULLABLE_BASICS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "NullableBasics",
    base_type: wrap_struct_fields_type(vec![
        ("b".to_string(), nullable_ty!(MojomType::Bool)),
        ("n1".to_string(), nullable_ty!(MojomType::UInt16)),
        ("n2".to_string(), nullable_ty!(MojomType::Int8)),
        ("empty".to_string(), nullable_ty!(EMPTY_TY.base_type.clone())),
        ("e".to_string(), nullable_ty!(MojomType::Enum { is_valid: TEST_ENUM_PRED })),
        ("fourints".to_string(), nullable_ty!(FOUR_INTS_TY.base_type.clone())),
        ("f1".to_string(), nullable_ty!(MojomType::Float32)),
        ("f2".to_string(), nullable_ty!(MojomType::Float64)),
    ]),
    packed_type: wrap_packed_struct_fields(
        vec![
            (
                "b_tag".to_string(),
                StructuredBodyElement::Bitfield([
                    Some((0, true)),
                    Some((0, false)),
                    Some((1, true)),
                    Some((2, true)),
                    Some((4, true)),
                    Some((6, true)),
                    Some((7, true)),
                    None,
                ]),
            ),
            ("n2_val".to_string(), struct_leaf!(2, PackedLeafType::Int8, true)),
            ("n1_val".to_string(), struct_leaf!(1, PackedLeafType::UInt16, true)),
            (
                "e_val".to_string(),
                struct_leaf!(4, PackedLeafType::Enum { is_valid: TEST_ENUM_PRED }, true),
            ),
            ("empty".to_string(), EMPTY_TY.as_nullable_struct_field(3)),
            ("fourints".to_string(), FOUR_INTS_TY.as_nullable_struct_field(5)),
            ("f1_val".to_string(), struct_leaf!(6, PackedLeafType::Float32, true)),
            ("f2_val".to_string(), struct_leaf!(7, PackedLeafType::Float64, true)),
        ],
        8,
    ),
});

#[allow(clippy::too_many_arguments)]
fn nullable_basics_mojom(
    b: Option<bool>,
    n1: Option<u16>,
    n2: Option<i8>,
    empty: Option<MojomValue>,
    e: Option<u32>,
    fourints: Option<MojomValue>,
    f1: Option<f32>,
    f2: Option<f64>,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("b".to_string(), nullable_val!(b.map(MojomValue::Bool))),
        ("n1".to_string(), nullable_val!(n1.map(MojomValue::UInt16))),
        ("n2".to_string(), nullable_val!(n2.map(MojomValue::Int8))),
        ("empty".to_string(), nullable_val!(empty)),
        ("e".to_string(), nullable_val!(e.map(MojomValue::Enum))),
        ("fourints".to_string(), nullable_val!(fourints)),
        ("f1".to_string(), nullable_val!(f1.map(|f| MojomValue::Float32(f.into())))),
        ("f2".to_string(), nullable_val!(f2.map(|f| MojomValue::Float64(f.into())))),
    ])
}

// Mojom Definition:
// array<bool?>
static ARRAY_NULL_BOOL_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<bool?>",
    base_type: array!(nullable_ty!(MojomType::Bool), None),
    packed_type: packed_array!(bare_leaf!(PackedLeafType::Bool, true), None),
});

fn array_null_bool_mojom(elts: Vec<Option<bool>>) -> MojomValue {
    MojomValue::Array(
        elts.into_iter().map(|elt| nullable_val!(elt.map(MojomValue::Bool))).collect(),
    )
}

// Mojom Definition:
// array<Empty?>
static ARRAY_NULL_EMPTY_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<Empty?>",
    base_type: array!(nullable_ty!(EMPTY_TY.base_type.clone()), None),
    packed_type: packed_array!(EMPTY_TY.packed_type.clone().make_nullable(), None),
});

fn array_null_empty_mojom(elts: Vec<Option<Empty>>) -> MojomValue {
    MojomValue::Array(
        elts.into_iter().map(|elt| nullable_val!(elt.map(MojomValue::from))).collect(),
    )
}

// Mojom Definition:
// array<TestEnum?>
static ARRAY_NULL_ENUM_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<TestEnum?>",
    base_type: array!(nullable_ty!(MojomType::Enum { is_valid: TEST_ENUM_PRED }), None),
    packed_type: packed_array!(
        bare_leaf!(PackedLeafType::Enum { is_valid: TEST_ENUM_PRED }, true),
        None
    ),
});

fn array_null_enum_mojom(elts: Vec<Option<TestEnum>>) -> MojomValue {
    MojomValue::Array(
        elts.into_iter()
            .map(|elt| nullable_val!(elt.map(|e| MojomValue::Enum(e.into()))))
            .collect(),
    )
}

// Mojom Definition:
// array<BaseUnion?>
static ARRAY_NULL_UNION_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<BaseUnion?>",
    base_type: array!(nullable_ty!(BASE_UNION_TY.base_type.clone()), None),
    packed_type: packed_array!(BASE_UNION_TY.packed_type.clone().make_nullable(), None),
});

fn array_null_union_mojom(elts: Vec<Option<BaseUnion>>) -> MojomValue {
    MojomValue::Array(
        elts.into_iter().map(|elt| nullable_val!(elt.map(MojomValue::from))).collect(),
    )
}

// Mojom Definition:
// struct ArraysOfNullables {
//   array<bool?> bools;
//   array<Empty?> empties;
//   array<TestEnum?> enums;
//   array<BaseUnion?> unions;
// }
static ARRAYS_OF_NULLABLES_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "ArraysOfNullables",
    base_type: wrap_struct_fields_type(vec![
        ("bools".to_string(), ARRAY_NULL_BOOL_TY.base_type.clone()),
        ("empties".to_string(), ARRAY_NULL_EMPTY_TY.base_type.clone()),
        ("enums".to_string(), ARRAY_NULL_ENUM_TY.base_type.clone()),
        ("unions".to_string(), ARRAY_NULL_UNION_TY.base_type.clone()),
    ]),
    packed_type: wrap_packed_struct_fields(
        vec![
            ("bools".to_string(), ARRAY_NULL_BOOL_TY.as_struct_field(0)),
            ("empties".to_string(), ARRAY_NULL_EMPTY_TY.as_struct_field(1)),
            ("enums".to_string(), ARRAY_NULL_ENUM_TY.as_struct_field(2)),
            ("unions".to_string(), ARRAY_NULL_UNION_TY.as_struct_field(3)),
        ],
        4,
    ),
});

fn arrays_of_nullables_mojom(
    bools: Vec<Option<bool>>,
    empties: Vec<Option<Empty>>,
    enums: Vec<Option<TestEnum>>,
    unions: Vec<Option<BaseUnion>>,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("bools".to_string(), array_null_bool_mojom(bools)),
        ("empties".to_string(), array_null_empty_mojom(empties)),
        ("enums".to_string(), array_null_enum_mojom(enums)),
        ("unions".to_string(), array_null_union_mojom(unions)),
    ])
}

// Mojom Definition:
// array<bool>?
static NULL_ARRAY_BOOL_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<bool>?",
    base_type: nullable_ty!(array!(MojomType::Bool, None)),
    packed_type: packed_array!(bare_leaf!(PackedLeafType::Bool, false), None).make_nullable(),
});

fn null_array_bool_mojom(elts: Option<Vec<bool>>) -> MojomValue {
    nullable_val!(elts.map(|e| MojomValue::Array(e.into_iter().map(MojomValue::Bool).collect())))
}

// Mojom Definition:
// array<bool?>?
static NULL_ARRAY_NULL_BOOL_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<bool?>?",
    base_type: nullable_ty!(array!(nullable_ty!(MojomType::Bool), None)),
    packed_type: packed_array!(bare_leaf!(PackedLeafType::Bool, true), None).make_nullable(),
});

fn null_array_null_bool_mojom(elts: Option<Vec<Option<bool>>>) -> MojomValue {
    nullable_val!(elts.map(array_null_bool_mojom))
}

// Mojom Definition:
// struct NullableArrays {
//   array<bool>? null_arr;
//   array<bool?>? double_null_arr;
// }
static NULLABLE_ARRAYS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "NullableArrays",
    base_type: wrap_struct_fields_type(vec![
        ("null_arr".to_string(), NULL_ARRAY_BOOL_TY.base_type.clone()),
        ("double_null_arr".to_string(), NULL_ARRAY_NULL_BOOL_TY.base_type.clone()),
    ]),
    packed_type: wrap_packed_struct_fields(
        vec![
            ("null_arr".to_string(), NULL_ARRAY_BOOL_TY.as_struct_field(0)),
            ("double_null_arr".to_string(), NULL_ARRAY_NULL_BOOL_TY.as_struct_field(1)),
        ],
        2,
    ),
});

fn nullable_arrays_mojom(
    null_arr: Option<Vec<bool>>,
    double_null_arr: Option<Vec<Option<bool>>>,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("null_arr".to_string(), null_array_bool_mojom(null_arr)),
        ("double_null_arr".to_string(), null_array_null_bool_mojom(double_null_arr)),
    ])
}

// Mojom Definition:
// union UnionWithNullables {
//   Empty? e;
//   string? str;
//   BaseUnion? u;
// }
static UNION_WITH_NULLABLES_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "UnionWithNullables",
    base_type: MojomType::Union {
        variants: [
            (0, nullable_ty!(EMPTY_TY.base_type.clone())),
            (1, nullable_ty!(MojomType::String)),
            (2, nullable_ty!(BASE_UNION_TY.base_type.clone())),
        ]
        .into(),
    },
    packed_type: MojomWireType::Union {
        variants: [
            (0, EMPTY_TY.as_nullable_union_field()),
            (1, STRING_TY.as_nullable_union_field()),
            (2, BASE_UNION_TY.as_nullable_union_field()),
        ]
        .into(),
        is_nullable: false,
    },
});

fn union_with_nullables_mojom_e(e: Option<Empty>) -> MojomValue {
    MojomValue::Union(0, Box::new(nullable_val!(e.map(|_| empty_mojom()))))
}

fn union_with_nullables_mojom_str(str: Option<&str>) -> MojomValue {
    MojomValue::Union(1, Box::new(nullable_val!(str.map(|s| MojomValue::String(s.to_string())))))
}

fn union_with_nullables_mojom_u(u: Option<MojomValue>) -> MojomValue {
    MojomValue::Union(2, Box::new(nullable_val!(u)))
}

// Mojom Definition:
// struct NullableOthers {
//   UnionWithNullables? u;
//   map<uint8, uint8>? m;
//   string? str;
// }
static NULLABLE_OTHERS_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "NullableOthers",
    base_type: wrap_struct_fields_type(vec![
        ("u".to_string(), nullable_ty!(UNION_WITH_NULLABLES_TY.base_type.clone())),
        ("m".to_string(), nullable_ty!(MAP_U8_U8_TY.base_type.clone())),
        ("str".to_string(), nullable_ty!(STRING_TY.base_type.clone())),
    ]),
    packed_type: wrap_packed_struct_fields(
        vec![
            ("u".to_string(), UNION_WITH_NULLABLES_TY.as_nullable_struct_field(0)),
            ("m".to_string(), MAP_U8_U8_TY.as_nullable_struct_field(1)),
            ("str".to_string(), STRING_TY.as_nullable_struct_field(2)),
        ],
        3,
    ),
});

fn nullable_others_mojom(
    u: Option<MojomValue>,
    m: Option<HashMap<u8, u8>>,
    str: Option<&str>,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("u".to_string(), nullable_val!(u)),
        ("m".to_string(), nullable_val!(m.map(map_u8_u8_mojom))),
        ("str".to_string(), nullable_val!(str.map(|s| MojomValue::String(s.to_string())))),
    ])
}

#[gtest(RustTestMojomParsingAttr, TestNullables)]
fn test_nullables() {
    NULLABLE_BASICS_TY.validate_mojomparse(
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
        nullable_basics_mojom(None, None, None, None, None, None, None, None),
    );
    NULLABLE_BASICS_TY.validate_mojomparse(
        NullableBasics {
            b: Some(true),
            n1: Some(33),
            n2: Some(12),
            empty: Some(Empty {}),
            e: Some(TestEnum::Four),
            fourints: Some(FourInts { a: 1, b: 2, c: 3, d: 4 }),
            f1: Some(3.14),
            f2: Some(2.71828),
        },
        nullable_basics_mojom(
            Some(true),
            Some(33),
            Some(12),
            Some(empty_mojom()),
            Some(4),
            Some(four_ints_mojom(1, 2, 3, 4)),
            Some(3.14),
            Some(2.71828),
        ),
    );

    ARRAYS_OF_NULLABLES_TY.validate_mojomparse(
        ArraysOfNullables {
            bools: vec![Some(true), None, Some(false)],
            empties: vec![None, Some(Empty {}), None, None, None],
            enums: vec![Some(TestEnum::Seven), None, Some(TestEnum::Zero), Some(TestEnum::Seven)],
            unions: vec![Some(BaseUnion::n1(5)), None, Some(BaseUnion::b1(true))],
        },
        arrays_of_nullables_mojom(
            vec![Some(true), None, Some(false)],
            vec![None, Some(Empty {}), None, None, None],
            vec![Some(TestEnum::Seven), None, Some(TestEnum::Zero), Some(TestEnum::Seven)],
            vec![Some(BaseUnion::n1(5)), None, Some(BaseUnion::b1(true))],
        ),
    );

    NULLABLE_ARRAYS_TY.validate_mojomparse(
        NullableArrays {
            null_arr: Some(vec![true, false, true]),
            double_null_arr: Some(vec![Some(true), None, Some(false)]),
        },
        nullable_arrays_mojom(
            Some(vec![true, false, true]),
            Some(vec![Some(true), None, Some(false)]),
        ),
    );
    NULLABLE_ARRAYS_TY.validate_mojomparse(
        NullableArrays { null_arr: None, double_null_arr: None },
        nullable_arrays_mojom(None, None),
    );

    UNION_WITH_NULLABLES_TY
        .validate_mojomparse(UnionWithNullables::e(None), union_with_nullables_mojom_e(None));
    UNION_WITH_NULLABLES_TY
        .validate_mojomparse(UnionWithNullables::str(None), union_with_nullables_mojom_str(None));
    UNION_WITH_NULLABLES_TY
        .validate_mojomparse(UnionWithNullables::u(None), union_with_nullables_mojom_u(None));

    UNION_WITH_NULLABLES_TY.validate_mojomparse(
        UnionWithNullables::e(Some(Empty {})),
        union_with_nullables_mojom_e(Some(Empty {})),
    );
    UNION_WITH_NULLABLES_TY.validate_mojomparse(
        UnionWithNullables::str(Some("hello".to_string())),
        union_with_nullables_mojom_str(Some("hello")),
    );
    UNION_WITH_NULLABLES_TY.validate_mojomparse(
        UnionWithNullables::u(Some(BaseUnion::n1(123))),
        union_with_nullables_mojom_u(Some(base_union_mojom_n1(123))),
    );
    UNION_WITH_NULLABLES_TY.validate_mojomparse(
        UnionWithNullables::u(Some(BaseUnion::b1(true))),
        union_with_nullables_mojom_u(Some(base_union_mojom_b1(true))),
    );
    UNION_WITH_NULLABLES_TY.validate_mojomparse(
        UnionWithNullables::u(Some(BaseUnion::f1(FourInts { a: 1, b: 2, c: 3, d: 4 }))),
        union_with_nullables_mojom_u(Some(base_union_mojom_f1(four_ints_mojom(1, 2, 3, 4)))),
    );

    NULLABLE_OTHERS_TY.validate_mojomparse(
        NullableOthers { u: None, m: None, str: None },
        nullable_others_mojom(None, None, None),
    );
    NULLABLE_OTHERS_TY.validate_mojomparse(
        NullableOthers {
            u: Some(UnionWithNullables::u(Some(BaseUnion::n1(42)))),
            m: Some([(1, 2), (3, 4)].into()),
            str: Some("hello".to_string()),
        },
        nullable_others_mojom(
            Some(union_with_nullables_mojom_u(Some(base_union_mojom_n1(42)))),
            Some([(1, 2), (3, 4)].into()),
            Some("hello"),
        ),
    );
}

// Mojom Definition:
// struct Handles {
//   handle h1;
//   handle? h2;
//   handle<message_pipe> h3;
//   handle<message_pipe>? h4;
// }
static HANDLES_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "Handles",
    base_type: wrap_struct_fields_type(vec![
        ("h1".to_string(), MojomType::Handle),
        ("h2".to_string(), nullable_ty!(MojomType::Handle)),
        ("h3".to_string(), MojomType::Handle),
        ("h4".to_string(), nullable_ty!(MojomType::Handle)),
    ]),
    packed_type: wrap_packed_struct_fields(
        vec![
            ("h1".to_string(), struct_leaf!(0, PackedLeafType::Handle, false)),
            ("h2".to_string(), struct_leaf!(1, PackedLeafType::Handle, true)),
            ("h3".to_string(), struct_leaf!(2, PackedLeafType::Handle, false)),
            ("h4".to_string(), struct_leaf!(3, PackedLeafType::Handle, true)),
        ],
        4,
    ),
});

fn handles_mojom(
    h1: UntypedHandle,
    h2: Option<UntypedHandle>,
    h3: UntypedHandle,
    h4: Option<UntypedHandle>,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("h1".to_string(), MojomValue::Handle(h1)),
        ("h2".to_string(), nullable_val!(h2.map(MojomValue::Handle))),
        ("h3".to_string(), MojomValue::Handle(h3)),
        ("h4".to_string(), nullable_val!(h4.map(MojomValue::Handle))),
    ])
}

// Mojom Definition:
// union WithHandles {
//   handle? h1;
//   handle<message_pipe> h2;
//   uint8 n;
// }
static WITH_HANDLES_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "WithHandles",
    base_type: MojomType::Union {
        variants: [
            (0, nullable_ty!(MojomType::Handle)),
            (1, MojomType::Handle),
            (2, MojomType::UInt8),
        ]
        .into(),
    },
    packed_type: MojomWireType::Union {
        variants: [
            (0, bare_leaf!(PackedLeafType::Handle, true)),
            (1, bare_leaf!(PackedLeafType::Handle)),
            (2, bare_leaf!(PackedLeafType::UInt8)),
        ]
        .into(),
        is_nullable: false,
    },
});

fn base_union_mojom_h1(h1: Option<UntypedHandle>) -> MojomValue {
    MojomValue::Union(0, Box::new(nullable_val!(h1.map(MojomValue::Handle))))
}

fn base_union_mojom_h2(h2: UntypedHandle) -> MojomValue {
    MojomValue::Union(1, Box::new(MojomValue::Handle(h2)))
}

fn base_union_mojom_n(e1: u8) -> MojomValue {
    MojomValue::Union(2, Box::new(MojomValue::UInt8(e1)))
}

// Mojom Definition:
// array<handle?>
static ARRAY_NULL_HANDLE_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "array<handle?>",
    base_type: array!(nullable_ty!(MojomType::Handle), None),
    packed_type: packed_array!(bare_leaf!(PackedLeafType::Handle, true), None),
});

fn array_null_handle_mojom(elts: Vec<Option<UntypedHandle>>) -> MojomValue {
    MojomValue::Array(
        elts.into_iter().map(|elt| nullable_val!(elt.map(MojomValue::Handle))).collect(),
    )
}

// Mojom Definition:
// struct NestedHandles {
//     handle h1;
//     array<handle?> arr;
//     handle h2;
//     WithHandles wh;
//     handle h3;
// };
static NESTED_HANDLES_TY: LazyLock<TestType> = LazyLock::new(|| TestType {
    type_name: "NestedHandles",
    base_type: wrap_struct_fields_type(vec![
        ("h1".to_string(), MojomType::Handle),
        ("arr".to_string(), ARRAY_NULL_HANDLE_TY.base_type.clone()),
        ("h2".to_string(), MojomType::Handle),
        ("wh".to_string(), WITH_HANDLES_TY.base_type.clone()),
        ("h3".to_string(), MojomType::Handle),
    ]),
    packed_type: wrap_packed_struct_fields(
        vec![
            ("h1".to_string(), struct_leaf!(0, PackedLeafType::Handle)),
            ("h2".to_string(), struct_leaf!(2, PackedLeafType::Handle)),
            ("arr".to_string(), ARRAY_NULL_HANDLE_TY.as_struct_field(1)),
            ("wh".to_string(), WITH_HANDLES_TY.as_struct_field(3)),
            ("h3".to_string(), struct_leaf!(4, PackedLeafType::Handle)),
        ],
        5,
    ),
});

fn nested_handles_mojom(
    h1: UntypedHandle,
    arr: Vec<Option<UntypedHandle>>,
    h2: UntypedHandle,
    wh: MojomValue,
    h3: UntypedHandle,
) -> MojomValue {
    wrap_struct_fields_value(vec![
        ("h1".to_string(), MojomValue::Handle(h1)),
        ("arr".to_string(), array_null_handle_mojom(arr)),
        ("h2".to_string(), MojomValue::Handle(h2)),
        ("wh".to_string(), wh),
        ("h3".to_string(), MojomValue::Handle(h3)),
    ])
}

#[gtest(RustTestMojomParsingAttr, TestHandles)]
fn test_handles() {
    HANDLES_TY.validate_mojomparse_handles(
        Handles { h1: dummy_handle(), h2: None, h3: dummy_handle().into(), h4: None },
        || handles_mojom(dummy_handle(), None, dummy_handle(), None),
    );

    HANDLES_TY.validate_mojomparse_handles(
        Handles {
            h1: dummy_handle(),
            h2: Some(dummy_handle()),
            h3: dummy_handle().into(),
            h4: Some(dummy_handle().into()),
        },
        || {
            handles_mojom(
                dummy_handle(),
                Some(dummy_handle()),
                dummy_handle(),
                Some(dummy_handle()),
            )
        },
    );

    WITH_HANDLES_TY
        .validate_mojomparse_handles(WithHandles::h1(None), || base_union_mojom_h1(None));
    WITH_HANDLES_TY.validate_mojomparse_handles(WithHandles::h1(Some(dummy_handle())), || {
        base_union_mojom_h1(Some(dummy_handle()))
    });
    WITH_HANDLES_TY.validate_mojomparse_handles(WithHandles::h2(dummy_handle().into()), || {
        base_union_mojom_h2(dummy_handle())
    });
    WITH_HANDLES_TY.validate_mojomparse_handles(WithHandles::n(42), || base_union_mojom_n(42));

    NESTED_HANDLES_TY.validate_mojomparse_handles(
        NestedHandles {
            h1: dummy_handle(),
            arr: vec![Some(dummy_handle()), None, Some(dummy_handle())],
            h2: dummy_handle(),
            wh: WithHandles::h2(dummy_handle().into()),
            h3: dummy_handle(),
        },
        || {
            nested_handles_mojom(
                dummy_handle(),
                vec![Some(dummy_handle()), None, Some(dummy_handle())],
                dummy_handle(),
                base_union_mojom_h2(dummy_handle()),
                dummy_handle(),
            )
        },
    );
}
