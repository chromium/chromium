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
    Array { element_type: Box<MojomType>, num_elements: Option<u32> },
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

/// Returns the size (in bytes) of a Mojom type, when stored as a struct field.
/// This is also the alignment requirement for that type.
pub fn get_size_and_alignment(ty: &MojomType) -> u32 {
    match ty {
        MojomType::Int8 | MojomType::UInt8 => 1,
        MojomType::Int16 | MojomType::UInt16 => 2,
        MojomType::Int32 | MojomType::UInt32 => 4,
        MojomType::Int64 | MojomType::UInt64 => 8,
        // Structs and arrays are stored as 64-bit pointers
        // Strings are encoded as arrays
        MojomType::Struct { .. } | MojomType::Array { .. } | MojomType::String => 8,
        MojomType::Bool => todo!("Packing is weird"),
    }
}
