// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Provides a generic syntax for Mojom types and values
//!
//! This module provides the ability to represent Mojom types and values as
//! rust enums.

chromium::import! {
    "//mojo/public/rust/system";
}

use ordered_float::OrderedFloat;
use std::collections::BTreeMap;
use std::sync::Arc;

pub use system::mojo_types::UntypedHandle;

// FOR_RELEASE: The current AST is dead simple: standard recursive data
// structures. We'll probably need an intermediate one as well to represent data
// on the wire, since it doesn't quite match the MojomValue structure (e.g.
// packed bitfields). For a more optimized version, we could look into
// flat ASTs (using a single vector instead of a nested recursive structure).

/// Representation of a type that can appear in a .mojom file.
///
/// These include the primitive types from
/// public/tools/bindings/README.md#Primitive-Types, as well as non-primitive
/// types like structs and enums.
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
    Float32,
    Float64,
    String,
    Handle,
    Enum { is_valid: Predicate<u32> },
    Union { variants: BTreeMap<u32, MojomType> },
    // `field_names` is only for debugging; it should have the same
    // length as `fields`
    Struct { field_names: Vec<String>, fields: Vec<MojomType> },
    // Mojom has separate sized/unsized array types; we could have two variants here, but
    // rust's type system can't enforce that the length is correct so there's little point.
    Array { element_type: Box<MojomType>, num_elements: Option<usize> },
    Map { key_type: Box<MojomType>, value_type: Box<MojomType> },
    Nullable { inner_type: Box<MojomType> },
}

/// Representation of a value of a MojomType. These are what get encoded/decoded
/// into/from Mojom messages.
///
/// Note: the Hash trait is used to verify that maps don't have duplicate keys;
/// the Ord trait is used so we can store these in BTreeMaps.
// FOR_RELEASE: For the first iteration of the parser where we don't worry
// about trying to be zero-copy, we just have this type own all its data.
// We should migrate to a view type when we figure out how.
#[derive(Debug, PartialEq, PartialOrd, Ord, Eq, Hash, Default)]
pub enum MojomValue {
    /// This value is only produced during parsing/deparsing to serve as a
    /// default value in vectors where we take elements individually.
    #[default]
    Invalid,
    Bool(bool),
    Int8(i8),
    UInt8(u8),
    Int16(i16),
    UInt16(u16),
    Int32(i32),
    UInt32(u32),
    Int64(i64),
    UInt64(u64),
    Float32(OrderedFloat<f32>),
    Float64(OrderedFloat<f64>),
    String(String),
    Enum(u32),
    Handle(UntypedHandle),
    Union(u32, Box<MojomValue>),
    Struct(Vec<String>, Vec<MojomValue>),
    // Invariant: all MojomValues in the array are the same type.
    Array(Vec<MojomValue>),
    // We use a BTreeMap so that we get a consistent ordering when serializing.
    Map(BTreeMap<MojomValue, MojomValue>),
    Nullable(Option<Box<MojomValue>>),
}

/**************************************************************** */
// The following types relate to how Mojom values are laid
// out when serialized to be sent in a message.
/*************************************************************** */

/// Represents a field of a Mojom struct by its index in the struct's
/// definition, i.e. "the nth field"
pub type Ordinal = usize;

/// A bitfield consist of up to 8 booleans packed into a single byte.
/// The ordinals represent which bits correspond to which elements of the
/// enclosing body, starting with the LSB. Bits are never skipped, so the
/// array is a contiguous block of `Some`s, followed by zero or more
/// `None`s.
///
/// Each ordinal is associated with a boolean indicating whether it is the tag
/// bit for a nullable primitive. This is only used during deparsing.
pub type BitfieldOrdinals = [Option<(Ordinal, bool)>; 8];

#[derive(Debug, Clone, PartialEq)]
/// Representation of a Mojom type that has been packed into the wire format. It
/// contains enough information to both parse and deparse the associated type.
///
/// Every value in a message corresponds to a member of some struct/array/union.
/// We will assume struct members without loss of generality. To map
/// values to their associated struct field, the wire type for that value tracks
/// that field's ordinal. The ordinal is used as an index into the vector of
/// fields during parsing and deparsing. For non-structs the ordinal is ignored.
///
/// Bitfields are a special case; since they can contain values from multiple
/// fields of the struct, each bit of the field is associated with an ordinal.
///
/// Unions can be represented as either a direct value, or a pointer.
///
/// # Nullability
/// Each wire type carries a bit indicating whether it is valid for values of
/// that type to be null. The interpretation is different for different types.
/// For unions and pointers, it indicates that 0 is a valid value.
///
/// For leaf values, nullability is instead indicated by the presence of a tag
/// bit earlier the enclosing structured body (nullable leaves are not allowed
/// inside unions). In our AST, that tag bit will be a boolean with the same
/// ordinal as the value itself. If the tag is 1, then the nullable is Some,
/// otherwise it is None.
pub enum MojomWireType {
    /// A single value with no additional structure, which is encoded directly
    /// at this location.
    Leaf { leaf_type: PackedLeafType, is_nullable: bool },
    /// A 64-bit pointer to an array, struct, or union, which will appear at the
    /// end of the containing value.
    Pointer { nested_data_type: PackedStructuredType, is_nullable: bool },
    /// A 128-bit value (not a pointer!) which contains a tag and a 64-bit value
    /// (which may be a pointer).
    Union { variants: BTreeMap<u32, MojomWireType>, is_nullable: bool },
}

/// This type represents an element in the body of a struct, array, or union.
///
/// You should read `WireTy` as `MojomWireType` and `BitfieldTy` as
/// `BitfieldOrdinal`. However, we need to parameterize the type because
/// sometimes we'll want to have references and sometimes we'll want to have
/// owned values. In the AST, all values are owned, but during parsing/deparsing
/// we'll sometimes need to create these on-the-fly from existing references.
///
/// For convenience, we define StructuredBodyElementOwned (which owns all its
/// data), StructuredBodyElementMixed (which owns the bitfield but not the wire
/// type), and StructuredBodyElementRef (which has a reference to the wire type,
/// and may or may not own the bitfield).
///
/// Note: std::borrow::Cow is not appropriate because we need to enforce when
/// data is/isn't owned (always owned in the AST, always refs during parsing).
#[derive(Debug, Clone, PartialEq)]
pub enum StructuredBodyElement<WireTy, BitfieldTy>
where
    WireTy: std::borrow::Borrow<MojomWireType>,
    BitfieldTy: std::borrow::Borrow<BitfieldOrdinals>,
{
    /// A single value with the given ordinal
    SingleValue(Ordinal, WireTy),
    /// Up to 8 bools packed into a single byte
    Bitfield(BitfieldTy),
}

pub type StructuredBodyElementOwned = StructuredBodyElement<MojomWireType, BitfieldOrdinals>;
pub type StructuredBodyElementMixed<'a> =
    StructuredBodyElement<&'a MojomWireType, BitfieldOrdinals>;
pub type StructuredBodyElementRef<'a, T> = StructuredBodyElement<&'a MojomWireType, T>;

#[derive(Debug, Clone, PartialEq)]
/// A type which contains no nested data
pub enum PackedLeafType {
    // Note that single booleans should never appear in a struct; they get
    // packed into a bitfield instead.
    Bool,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float32,
    Float64,
    Enum { is_valid: Predicate<u32> },
    Handle,
}

#[derive(Debug, Clone, PartialEq)]
pub enum PackedStructuredType {
    Struct {
        packed_field_names: Vec<String>,
        packed_field_types: Vec<StructuredBodyElementOwned>,
        /// Stores the number of elements this struct has in its _non-packed_
        /// form. It's equal to 1 + the maximum ordinal in packed_field_types,
        /// or 0 if packed_field_types is empty.
        num_elements_in_value: usize,
    },
    Array {
        // This uses Arc instead of Box so we can cheaply clone it during (de)parsing
        element_type: Arc<MojomWireType>,
        array_type: PackedArrayType,
    },
    Union {
        variants: BTreeMap<u32, MojomWireType>,
    },
    Map {
        key_type: Arc<MojomWireType>,
        value_type: Arc<MojomWireType>,
    },
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

/**************************************************************** */
// Implementations of useful functions and methods for AST types.
/*************************************************************** */

impl MojomWireType {
    /// Returns the size (in bytes) of a wire type, when stored as a struct
    /// field.
    pub fn size(&self) -> usize {
        match self {
            MojomWireType::Leaf { leaf_type, .. } => match leaf_type {
                PackedLeafType::Int8 | PackedLeafType::UInt8 | PackedLeafType::Bool => 1,
                PackedLeafType::Int16 | PackedLeafType::UInt16 => 2,
                PackedLeafType::Int32 | PackedLeafType::UInt32 | PackedLeafType::Float32 => 4,
                PackedLeafType::Int64 | PackedLeafType::UInt64 | PackedLeafType::Float64 => 8,
                PackedLeafType::Enum { .. } => 4,
                PackedLeafType::Handle => 4,
            },
            MojomWireType::Pointer { .. } => 8,
            MojomWireType::Union { .. } => 16,
        }
    }

    /// The alignment requirement for leaf data and pointers is equal to the
    /// value's size in bytes. The alignment requirement for structured data
    /// is always 8 bytes.
    pub fn alignment(&self) -> usize {
        match self {
            MojomWireType::Union { .. } => 8,
            _ => self.size(),
        }
    }

    pub fn is_nullable(&self) -> bool {
        match self {
            MojomWireType::Leaf { is_nullable, .. }
            | MojomWireType::Pointer { is_nullable, .. }
            | MojomWireType::Union { is_nullable, .. } => *is_nullable,
        }
    }

    pub fn is_nullable_primitive(&self) -> bool {
        match self {
            // Handles aren't primitives; they have their own nullability semantics
            MojomWireType::Leaf { leaf_type: PackedLeafType::Handle, .. } => false,
            MojomWireType::Leaf { is_nullable, .. } => *is_nullable,
            _ => false,
        }
    }

    pub fn make_nullable(self) -> Self {
        match self {
            MojomWireType::Leaf { leaf_type, .. } => {
                MojomWireType::Leaf { leaf_type, is_nullable: true }
            }
            MojomWireType::Pointer { nested_data_type, .. } => {
                MojomWireType::Pointer { nested_data_type, is_nullable: true }
            }
            MojomWireType::Union { variants, .. } => {
                MojomWireType::Union { variants, is_nullable: true }
            }
        }
    }

    /// Check if this type is valid as the key to a Mojom map.
    /// Valid keys are non-nullable primitives and strings.
    pub fn is_valid_map_key(&self) -> bool {
        matches!(
            self,
            MojomWireType::Leaf { is_nullable: false, .. }
                | MojomWireType::Pointer {
                    nested_data_type: PackedStructuredType::Array {
                        array_type: PackedArrayType::String,
                        ..
                    },
                    is_nullable: false,
                },
        )
    }
}

impl<T, T2> StructuredBodyElement<T, T2>
where
    T: std::borrow::Borrow<MojomWireType>,
    T2: std::borrow::Borrow<BitfieldOrdinals>,
{
    pub fn size(&self) -> usize {
        match self {
            Self::SingleValue(_, wire_type) => wire_type.borrow().size(),
            Self::Bitfield(_) => 1,
        }
    }

    pub fn alignment(&self) -> usize {
        match self {
            Self::SingleValue(_, wire_type) => wire_type.borrow().alignment(),
            Self::Bitfield(_) => 1,
        }
    }
}

impl StructuredBodyElementOwned {
    pub fn as_ref<'a>(&'a self) -> StructuredBodyElementRef<'a, &'a BitfieldOrdinals> {
        match self {
            Self::SingleValue(ordinal, wire_type) => {
                StructuredBodyElement::SingleValue(*ordinal, wire_type)
            }
            Self::Bitfield(ordinals) => StructuredBodyElement::Bitfield(ordinals),
        }
    }
}

/**************************************************************** */
// Helper types and impls that don't logically appear as part of
//  the AST, but which are needed to satisfy the compiler.
/*************************************************************** */

/// A function which returns boolean. This is its own type so that
/// we can derive our own equality function for it, since function
/// pointer comparison is unreliable.
#[derive(Debug, Clone, Copy)]
pub struct Predicate<T>
where
    T: 'static,
{
    f: &'static fn(T) -> bool,
    /// An identifier saying where this function came from.
    /// Only useful for equality checking
    id: std::any::TypeId,
}

impl<T> PartialEq for Predicate<T> {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<T> Predicate<T> {
    pub const fn new<SrcTy: 'static>(f: &'static fn(T) -> bool) -> Predicate<T> {
        Predicate { f, id: std::any::TypeId::of::<SrcTy>() }
    }

    pub fn call(&self, arg: T) -> bool {
        (self.f)(arg)
    }
}
