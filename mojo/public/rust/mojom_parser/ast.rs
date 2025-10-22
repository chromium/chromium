// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Provides a generic syntax for Mojom types and values
//!
//! This module provides the ability to represent Mojom types and values as
//! rust enums.

// FOR_RELEASE: The current AST is dead simple: standard recursive data
// structures. We'll probably need an intermediate one as well to represent data
// on the wire, since it doesn't quite match the MojomValue structure (e.g.
// packed bitfields). For a more optimized version, we should look into:
// - Flat ASTs (using a single vector instead of a nested recursive structure
// - Mapping between these and rust types that are created by the bindings
//   generator
// - Some way to mark fields that doesn't rely on using strings to tag them.
// - For example, using fixed-length arrays indexed by by the field's ordinal.
//   This is much less intuitive, though, so it should wait until we're certain
//   things are already working.

/// Representation of a type that can appear in a .mojom file.
///
/// These include the primitive types from
/// public/tools/bindings/README.md#Primitive-Types, as well as non-primitive
/// types like structs and enums.
// FOR_RELEASE: Not all types are currently supported, and we won't support all of them
// for the initial release, but we'll need at least these plus enums and nullables.
#[derive(Debug, Clone, PartialEq)]
pub enum MojomType {
    Bool,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    String,
    Struct { fields: Vec<(String, MojomType)> },
    // Mojom has separate sized/unsized array types; we could have two variants here, but
    // rust's type system can't enforce that the length is correct so there's little point.
    Array { element_type: Box<MojomType>, num_elements: Option<usize> },
}

/// Representation of a value of a MojomType. These are what get encoded/decoded
/// into/from Mojom messages.
// FOR_RELEASE: For the first iteration of the parser where we don't worry
// about trying to be zero-copy, we just have this type own all its data.
// We should migrate to a view type when we figure out how.
#[derive(Debug, Clone, PartialEq)]
pub enum MojomValue {
    Bool(bool),
    Int8(i8),
    UInt8(u8),
    Int16(i16),
    UInt16(u16),
    Int32(i32),
    UInt32(u32),
    Int64(i64),
    UInt64(u64),
    String(String),
    Struct(Vec<(String, MojomValue)>),
    // Invariant: all MojomValues in the array are the same type.
    Array(Vec<MojomValue>),
}

/******************************************************************************
 * All the following types relate to how Mojom values are laid out when
 * serialized to be sent in a message.
 ******************************************************************************/

/// Represents a field of a Mojom struct by its index in the struct's definition,
/// i.e. "the nth field"
pub type Ordinal = usize;

#[derive(Debug, Clone, PartialEq)]
/// Representation of a Mojom type that has been packed into the wire format. It contains
/// enough information to both parse and deparse the associated type.
///
/// Every value in a message corresponds to a member of some struct. To map values to their
/// associated struct field, the wire type for that value tracks that field's ordinal. The
/// ordinal is used as an index into the vector of fields during parsing and deparsing.
///
/// Bitfields are a special case; since they can contain values from multiple fields of the
/// struct, each bit of the field is associated with an ordinal.
pub enum MojomWireType {
    /// A single value with no additional structure, which is encoded directly
    /// at this location.
    Leaf { ordinal: Ordinal, leaf_type: PackedLeafType },
    /// Up to 8 booleans packed into a single byte.
    Bitfield {
        /// A list of the ordinal associated with each bit, starting with the LSB.
        /// Bits are never skipped, so the array is a contiguous block of `Some`s,
        /// followed by zero or more `None`s.
        ordinals: [Option<Ordinal>; 8],
        // The associated data is always a single byte, so no need to store a
        // type here.
    },
    /// A 64-bit pointer to either an array or struct, which will appear at the
    /// end of the containing struct.
    Pointer { ordinal: Ordinal, nested_data_type: PackedStructuredType },
}

#[derive(Debug, Clone, PartialEq)]
/// A type which is simply encoded as itself
pub enum PackedLeafType {
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
}

#[derive(Debug, Clone, PartialEq)]
pub enum PackedStructuredType {
    Struct { packed_field_types: Vec<(String, MojomWireType)> },
    Array { element_type: Box<MojomWireType>, array_type: PackedArrayType },
}

#[derive(Debug, Clone, PartialEq)]
/// An array on the wire may originate from one of three Mojom types:
/// An unsized array, A size N array, or a string.
/// FOR_RELEASE: Arrays of nullables may also be their own category?
pub enum PackedArrayType {
    UnsizedArray,
    SizedArray(usize),
    String,
}

impl MojomWireType {
    /// Returns the size (in bytes) of a wire type, when stored as a struct field.
    pub fn size(&self) -> usize {
        match self {
            MojomWireType::Leaf { leaf_type, .. } => match leaf_type {
                PackedLeafType::Int8 | PackedLeafType::UInt8 => 1,
                PackedLeafType::Int16 | PackedLeafType::UInt16 => 2,
                PackedLeafType::Int32 | PackedLeafType::UInt32 => 4,
                PackedLeafType::Int64 | PackedLeafType::UInt64 => 8,
            },
            MojomWireType::Bitfield { .. } => 1,
            // Structs and arrays are stored as 64-bit pointers
            MojomWireType::Pointer { .. } => 8,
        }
    }

    /// The alignment requirement for each type is equal to its size in bytes.
    pub fn alignment(&self) -> usize {
        return self.size();
    }
}
