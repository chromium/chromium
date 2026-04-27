// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This file implements the mojom packing algorithm, which determines the wire
//! format for a mojom struct.
//!
//! The canonical implementation lives in
//! mojo/public/tools/mojom/mojom/generate/pack.py. We re-implement it here so
//! that we can run it at runtime time instead of bindings-generation time.
//! Ideally we would run it at compile time and embed the resulting wire type
//! in the generated code, but since wire types involve heap allocations they
//! aren't valid constants.
//!
//! At a high level, the algorithm is as follows: put the fields in declaration
//! order, and then move fields backwards into empty padding bytes whenever
//! there's enough room. Note that nested structures (structs, arrays, and
//! sometimes unions) are represented as 8-byte pointers.
//!
//! As an optimization, booleans get packed together as a bitfield instead of
//! each getting their own byte. When packing a boolean, if there's an existing
//! boolean earlier in the message, the later one will get assigned to the
//! second bit of the prior one. The next boolean we see will get assigned to
//! bit 3, and so on until the byte is full, at which point the process starts
//! over with the next boolean we see.
//!
//! When a primitive is nullable, it gets split into two fields: a tag bit,
//! followed by a value field. These fields get packed separately and are not
//! necessarily contiguous (the tag bit will likely get lumped in with other
//! booleans).
//!
//! A useful property of this algorithm is that if a field A appears before B,
//! and sizeof(A) < sizeof(B), then A is guaranteed to still appear before B in
//! the packed representation.

use crate::ast::*;
use std::collections::BTreeMap;
use std::sync::Arc;

/// Return the number of bytes we need to skip to reach the given alignment.
fn bytes_to_align(current_offset: usize, required_alignment: usize) -> usize {
    (required_alignment - (current_offset % required_alignment)) % required_alignment
}

/// Represents a single field which has been packed into wire format, with
/// enough information to both construct the binary representation and map back
/// to the original type.
struct PackedField {
    /// The name of the field in the original struct definition.
    name: String,
    /// The type of the field, which has been recursively packed.
    ty: StructuredBodyElementOwned,
    /// Number of bytes from the beginning of the struct to the start of the
    /// field.
    start_offset: usize,
    /// Number of bytes from the beginning of the struct to the end of the
    /// field.
    end_offset: usize,
}

impl PackedField {
    /// Create a new PackedField given the original field's information and its
    /// location
    fn new(name: String, ty: StructuredBodyElementOwned, start_offset: usize) -> PackedField {
        PackedField { start_offset, end_offset: start_offset + ty.size(), name, ty }
    }
}

/// Tries to pack a boolean into an existing field.
///
/// This succeeds if and only if the existing field is a bitfield with an empty
/// slot, and the current field is a boolean. If so, it inserts the boolean's
/// ordinal (and nullness tag) into the empty slot. Otherwise, it does nothing.
///
/// Returns true if the bool was successfully packed, and false otherwise.
fn try_pack_bool(
    old_field: &mut StructuredBodyElementOwned,
    new_field: &StructuredBodyElementOwned,
) -> bool {
    match (old_field, new_field) {
        (
            StructuredBodyElement::Bitfield(old_ordinals),
            StructuredBodyElement::Bitfield([new_ordinal, ..]),
        ) => {
            if let Some(first_empty_slot) = old_ordinals.iter().position(|opt| opt.is_none()) {
                old_ordinals[first_empty_slot] = *new_ordinal;
                true
            } else {
                false
            }
        }
        _ => false,
    }
}

/// Transform the fields of a Mojom struct into their packed representation.
/// This uses the basic algorithm from
/// mojo/public/tools/mojom/mojom/generate/pack.py
fn pack_struct(fields: &[MojomType], field_names: &[String]) -> PackedStructuredType {
    let mut packed_fields: Vec<PackedField> = vec![];
    let mut total_length = 0;

    // First, recursively pack each field. Nullable primitives get split into
    // two fields at this point. Note: the tag bit always precedes the value!
    let fields_to_pack = fields.iter().enumerate().flat_map(|(ordinal, field_ty)| {
        let field_name = field_names
            .get(ordinal)
            .expect("pack_struct: field_names should have the same length as fields");
        // Since this might output either one or two fields, we output a vector
        // and flatten afterwards.
        match pack_struct_field(ordinal, field_ty) {
            (Some(tag_bit), packed_ty) => vec![
                (format!("{field_name}_tag"), tag_bit),
                (format!("{field_name}_val"), packed_ty),
            ],
            (None, packed_ty) => vec![(field_name.clone(), packed_ty)],
        }
    });

    // For each field, see if we can fit it between two existing packed fields.
    // If not, put it at the end.
    'outer: for (field_name, field_ty) in fields_to_pack {
        let field_alignment = field_ty.alignment();

        // If our current field is a boolean, then we want to pack it into an
        // existing bitfield if possible. Since the below loop checks pairs of
        // elements, it will miss the first element of the vector, so check it
        // here.
        if let Some(first_packed_field) = packed_fields.first_mut()
            && try_pack_bool(&mut first_packed_field.ty, &field_ty)
        {
            continue;
        }

        // Try every pair (i-1, i) of adjacent packed fields.
        for i in 1..packed_fields.len() {
            if try_pack_bool(&mut packed_fields[i].ty, &field_ty) {
                continue 'outer;
            };

            let end_of_last_field = packed_fields[i - 1].end_offset;
            let empty_space = packed_fields[i].start_offset - end_of_last_field;
            // If we fit, then pack this field here
            if (field_ty.size() + bytes_to_align(end_of_last_field, field_alignment)) <= empty_space
            {
                packed_fields.insert(
                    i,
                    PackedField::new(
                        field_name,
                        field_ty,
                        end_of_last_field + bytes_to_align(end_of_last_field, field_alignment),
                    ),
                );
                continue 'outer;
            }
        } // End `for i in 1..packed_fields.len()`

        // If we get all the way here then we failed to pack the field anywhere
        // earlier, so add it to the end.
        let packed_field = PackedField::new(
            field_name,
            field_ty,
            total_length + bytes_to_align(total_length, field_alignment),
        );
        total_length = packed_field.end_offset;
        packed_fields.push(packed_field);
    }

    // Transform each packed field back into a regular MojomType
    // Also recursively pack each one, to handle nested structs.
    let (packed_field_types, packed_field_names): (Vec<StructuredBodyElementOwned>, Vec<String>) =
        packed_fields
            .into_iter()
            .map(|packed_field| (packed_field.ty, packed_field.name.clone()))
            .unzip();

    PackedStructuredType::Struct {
        packed_field_types,
        packed_field_names,
        num_elements_in_value: fields.len(),
    }
}

fn pack_union_variants(variants: &BTreeMap<i32, MojomType>) -> BTreeMap<i32, MojomWireType> {
    variants
        .iter()
        .map(|(tag, ty)| {
            let wire_ty = pack_mojom_type(ty);
            let ret_ty = match wire_ty {
                // Special case: Unions nested in other unions are represented as pointers
                MojomWireType::Union { variants, is_nullable } => MojomWireType::Pointer {
                    nested_data_type: PackedStructuredType::Union { variants },
                    is_nullable,
                },
                _ if wire_ty.is_nullable_primitive() => {
                    panic!("Mojom unions cannot contain nullable primitives")
                }
                _ => wire_ty,
            };
            (*tag, ret_ty)
        })
        .collect()
}

/// Given a MojomType, return its wire representation in isolation (i.e., not as
/// a member of a struct, union, or array).
///
/// Note that nullable primitives require additional representation in their
/// enclosing structured body (a tag bit). Since this function does not
/// consider context, the caller is responsible for setting those up as
/// necessary.
pub fn pack_mojom_type(ty: &MojomType) -> MojomWireType {
    let (ty, is_nullable) = match ty {
        MojomType::Nullable { inner_type } => (&**inner_type, true),
        _ => (ty, false),
    };

    match ty {
        MojomType::Struct { fields, field_names } => MojomWireType::Pointer {
            nested_data_type: pack_struct(fields, field_names),
            is_nullable,
        },
        MojomType::Array { element_type, num_elements } => {
            let array_type = match num_elements {
                None => PackedArrayType::UnsizedArray,
                Some(n) => PackedArrayType::SizedArray(*n),
            };

            MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Array {
                    element_type: Arc::new(pack_mojom_type(element_type)),
                    array_type,
                },
                is_nullable,
            }
        }
        // Strings are packed as byte arrays
        MojomType::String => MojomWireType::Pointer {
            nested_data_type: PackedStructuredType::Array {
                element_type: Arc::new(pack_mojom_type(&MojomType::UInt8)),
                array_type: PackedArrayType::String,
            },
            is_nullable,
        },
        MojomType::Map { key_type, value_type } => {
            let key_type = pack_mojom_type(key_type);
            if !key_type.is_valid_map_key() {
                panic!("Mojom map keys may only be non-nullable primitives or strings.")
            };
            MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Map {
                    key_type: Arc::new(key_type),
                    value_type: Arc::new(pack_mojom_type(value_type)),
                },
                is_nullable,
            }
        }
        MojomType::Bool => MojomWireType::Leaf { leaf_type: PackedLeafType::Bool, is_nullable },
        MojomType::Int8 => MojomWireType::Leaf { leaf_type: PackedLeafType::Int8, is_nullable },
        MojomType::Int16 => MojomWireType::Leaf { leaf_type: PackedLeafType::Int16, is_nullable },
        MojomType::Int32 => MojomWireType::Leaf { leaf_type: PackedLeafType::Int32, is_nullable },
        MojomType::Int64 => MojomWireType::Leaf { leaf_type: PackedLeafType::Int64, is_nullable },
        MojomType::UInt8 => MojomWireType::Leaf { leaf_type: PackedLeafType::UInt8, is_nullable },
        MojomType::UInt16 => MojomWireType::Leaf { leaf_type: PackedLeafType::UInt16, is_nullable },
        MojomType::UInt32 => MojomWireType::Leaf { leaf_type: PackedLeafType::UInt32, is_nullable },
        MojomType::UInt64 => MojomWireType::Leaf { leaf_type: PackedLeafType::UInt64, is_nullable },
        MojomType::Handle => MojomWireType::Leaf { leaf_type: PackedLeafType::Handle, is_nullable },
        MojomType::PendingReceiver => {
            MojomWireType::Leaf { leaf_type: PackedLeafType::PendingReceiver, is_nullable }
        }
        MojomType::PendingRemote => {
            MojomWireType::Leaf { leaf_type: PackedLeafType::PendingRemote, is_nullable }
        }
        MojomType::Float32 => {
            MojomWireType::Leaf { leaf_type: PackedLeafType::Float32, is_nullable }
        }
        MojomType::Float64 => {
            MojomWireType::Leaf { leaf_type: PackedLeafType::Float64, is_nullable }
        }
        MojomType::Enum { is_valid } => MojomWireType::Leaf {
            leaf_type: PackedLeafType::Enum { is_valid: *is_valid },
            is_nullable,
        },
        MojomType::Union { variants } => {
            MojomWireType::Union { variants: pack_union_variants(variants), is_nullable }
        }
        MojomType::Nullable { .. } => {
            panic!("Mojom does not permit doubly-nullable types.")
        }
    }
}

/// Given a MojomType, return its wire representation as a member of a struct
/// with the given ordinal. If the type is a nullable primitive, also return the
/// representation of the tag bit (with the same ordinal) which will be packed
/// _before_ the primitive value.
pub fn pack_struct_field(
    ordinal: Ordinal,
    ty: &MojomType,
) -> (Option<StructuredBodyElementOwned>, StructuredBodyElementOwned) {
    // This is the type of a single boolean packed at this ordinal. It's used
    // for the value itself (if a bool), as well as the tag bit for nullable
    // primitives.
    let packed_bool_ty = |is_nullable_tag_bit| {
        StructuredBodyElement::Bitfield([
            Some((ordinal, is_nullable_tag_bit)),
            None,
            None,
            None,
            None,
            None,
            None,
            None,
        ])
    };

    let packed_ty = pack_mojom_type(ty);
    let tag_bit = if packed_ty.is_nullable_primitive() { Some(packed_bool_ty(true)) } else { None };
    let final_packed_ty = match packed_ty {
        MojomWireType::Leaf { leaf_type: PackedLeafType::Bool, .. } => packed_bool_ty(false),
        _ => StructuredBodyElement::SingleValue(ordinal, packed_ty),
    };

    (tag_bit, final_packed_ty)
}

// Create a set of bitfields which contain exactly `n` bools, with ordinals
// increasing from 0 to n-1.
fn pack_n_bits<'a>(
    n: usize,
    is_tag_bits: bool,
) -> impl Iterator<Item = StructuredBodyElementMixed<'a>> {
    let num_full_bitfields = n / 8; // Round down
    let remaining_bools = n % 8;
    let num_partial_bitfields = if remaining_bools == 0 { 0 } else { 1 };
    let mut bitfields: Vec<BitfieldOrdinals> =
        Vec::with_capacity(num_full_bitfields + num_partial_bitfields);
    for i in 0..num_full_bitfields {
        bitfields.push(std::array::from_fn(|idx| Some((i * 8 + idx, is_tag_bits))))
    }
    if remaining_bools != 0 {
        bitfields.push(std::array::from_fn(|idx| {
            if idx < remaining_bools {
                Some((num_full_bitfields * 8 + idx, is_tag_bits))
            } else {
                None
            }
        }))
    }

    bitfields.into_iter().map(StructuredBodyElement::Bitfield)
}

/// Create a structured body out of an array. This can be thought of as part of
/// the packing algorithm, but we can't actually run it until we know the number
/// of elements in the array.
pub fn pack_array_body<'a>(
    element_type: &'a MojomWireType,
    num_elements: usize,
) -> impl Iterator<Item = StructuredBodyElementMixed<'a>> + 'a {
    // If the array contains nullable primitives, then each element needs a tag
    // bit. Those tag bits appear at the beginning of the array body, in order,
    // before any of the array elements.
    let tag_bits = if element_type.is_nullable_primitive() {
        pack_n_bits(num_elements, true)
    } else {
        pack_n_bits(0, true) // Returns an empty iterator
    };

    let packed_body = match element_type {
        // If the array contains booleans we'll need to convert them to bitfields
        // with the proper ordinals
        MojomWireType::Leaf { leaf_type: PackedLeafType::Bool, .. } => {
            itertools::Either::Left(pack_n_bits(num_elements, false))
        }
        // All non-bool types just pack as themselves, in order
        _ => itertools::Either::Right(
            (0..num_elements).map(move |idx| StructuredBodyElement::SingleValue(idx, element_type)),
        ),
    };

    tag_bits.chain(packed_body)
}
