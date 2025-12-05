// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This file implements the mojom packing algorithm, which determines the wire
//! format for a mojom struct.
//!
//! The canonical implementation lives in
//! mojo/public/tools/mojom/mojom/generate/pack.py. We re-implement it here so
//! that we can run it at compile-time instead of bindings-generation-time.
//!
//! At a high level, the algorithm is as follows: put the fields in declaration
//! order, and then move fields backwards into empty padding bytes whenever
//! possible. Note that nested structures (structs and arrays) are represented
//! as 8-byte pointers.

// FOR_RELEASE: There are additional complications for nullable types (which
// get split into two fields), and booleans (which get packed together as
// bitfields.)

use crate::ast::*;
use std::collections::HashMap;

/// Return the number of bytes we need to skip to reach the given alignment.
fn bytes_to_align(current_offset: usize, required_alignment: usize) -> usize {
    return (required_alignment - (current_offset % required_alignment)) % required_alignment;
}

/// Represents a single field which has been packed into wire format, with
/// enough information to both construct the binary representation and map back
/// to the original type.
struct PackedField<'a> {
    /// The name of the field in the original struct definition.
    name: &'a String,
    /// The type of the field, which has been recursively packed.
    ty: StructuredBodyElementOwned,
    /// Number of bytes from the beginning of the struct to the start of the
    /// field.
    start_offset: usize,
    /// Number of bytes from the beginning of the struct to the end of the
    /// field.
    end_offset: usize,
}

impl<'a> PackedField<'a> {
    /// Create a new PackedField given the original field's information and its
    /// location
    fn new(
        name: &'a String,
        ty: StructuredBodyElementOwned,
        start_offset: usize,
    ) -> PackedField<'a> {
        PackedField { start_offset: start_offset, end_offset: start_offset + ty.size(), name, ty }
    }
}

/// Checks if the packed field is a bitfield with an empty slot, and inserts
/// the ordinal if so.
///
/// Returns true if the bool was successfully packed, and false otherwise.
fn try_pack_bool(ordinal: Ordinal, packed_field: &mut StructuredBodyElementOwned) -> bool {
    match packed_field {
        StructuredBodyElement::Bitfield(ordinals) => {
            if let Some(first_empty_slot) = ordinals.into_iter().position(|opt| opt.is_none()) {
                ordinals[first_empty_slot] = Some(ordinal);
                return true;
            } else {
                return false;
            }
        }
        _ => {
            return false;
        }
    }
}

/// Transform the fields of a Mojom struct into their packed representation.
/// This uses the basic algorithm from
/// mojo/public/tools/mojom/mojom/generate/pack.py
fn pack_struct(fields: &[MojomType], field_names: &[String]) -> PackedStructuredType {
    let mut packed_fields: Vec<PackedField> = vec![];
    let mut total_length = 0;
    // For each field, see if we can fit it between two existing packed fields.
    // If not, put it at the end.
    'outer: for (ordinal, field_ty) in fields.iter().enumerate() {
        let is_bool = match field_ty {
            MojomType::Bool => true,
            _ => false,
        };
        let field_name = field_names
            .get(ordinal)
            .expect("pack_struct: field_names should have the same length as fields");
        // Recursively pack any structs this field contains
        let field_ty = pack_struct_field(field_ty, ordinal);
        let field_alignment = field_ty.alignment();
        // Try every pair (i-1, i) of adjacent packed fields.
        for i in 1..packed_fields.len() {
            let end_of_last_field = packed_fields[i - 1].end_offset;
            let empty_space = packed_fields[i].start_offset - end_of_last_field;
            if is_bool && try_pack_bool(ordinal, &mut packed_fields[i - 1].ty) {
                continue 'outer;
            };
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
        }

        // The above loop didn't check the very last element of packed_fields,
        // so do that now
        if is_bool
            && let Some(last_packed_field) = packed_fields.last_mut()
            && try_pack_bool(ordinal, &mut last_packed_field.ty)
        {
            continue;
        }

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

    return PackedStructuredType::Struct {
        packed_field_types,
        packed_field_names,
        num_elements_in_value: fields.len(),
    };
}

fn pack_union_variants(variants: &HashMap<u32, MojomType>) -> HashMap<u32, MojomWireType> {
    variants
        .iter()
        .map(|(tag, ty)| {
            let wire_ty = pack_mojom_type(ty);
            let ret_ty = match wire_ty {
                // Special case: Unions nested in other unions are represented as pointers
                MojomWireType::Union { variants } => MojomWireType::Pointer {
                    nested_data_type: PackedStructuredType::Union { variants },
                },
                _ => wire_ty,
            };
            (*tag, ret_ty)
        })
        .collect()
}

/// Given a MojomType, return its wire representation in isolation
pub fn pack_mojom_type(ty: &MojomType) -> MojomWireType {
    match ty {
        MojomType::Struct { fields, field_names } => {
            MojomWireType::Pointer { nested_data_type: pack_struct(fields, field_names) }
        }
        MojomType::Array { element_type, num_elements } => {
            let array_type = match num_elements {
                None => PackedArrayType::UnsizedArray,
                Some(n) => PackedArrayType::SizedArray(*n),
            };

            MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Array {
                    element_type: Box::new(pack_mojom_type(element_type)),
                    array_type,
                },
            }
        }
        // Strings are packed as byte arrays
        MojomType::String => MojomWireType::Pointer {
            nested_data_type: PackedStructuredType::Array {
                element_type: Box::new(pack_mojom_type(&MojomType::UInt8)),
                array_type: PackedArrayType::String,
            },
        },
        MojomType::Bool => MojomWireType::Leaf { leaf_type: PackedLeafType::Bool },
        MojomType::Int8 => MojomWireType::Leaf { leaf_type: PackedLeafType::Int8 },
        MojomType::Int16 => MojomWireType::Leaf { leaf_type: PackedLeafType::Int16 },
        MojomType::Int32 => MojomWireType::Leaf { leaf_type: PackedLeafType::Int32 },
        MojomType::Int64 => MojomWireType::Leaf { leaf_type: PackedLeafType::Int64 },
        MojomType::UInt8 => MojomWireType::Leaf { leaf_type: PackedLeafType::UInt8 },
        MojomType::UInt16 => MojomWireType::Leaf { leaf_type: PackedLeafType::UInt16 },
        MojomType::UInt32 => MojomWireType::Leaf { leaf_type: PackedLeafType::UInt32 },
        MojomType::UInt64 => MojomWireType::Leaf { leaf_type: PackedLeafType::UInt64 },
        MojomType::Enum { is_valid } => {
            MojomWireType::Leaf { leaf_type: PackedLeafType::Enum { is_valid: *is_valid } }
        }
        MojomType::Union { variants } => {
            MojomWireType::Union { variants: pack_union_variants(variants) }
        }
    }
}

/// Given a MojomType, return its wire representation
/// as a member of a struct with the given ordinal.
pub fn pack_struct_field(ty: &MojomType, ordinal: Ordinal) -> StructuredBodyElementOwned {
    let packed_ty = pack_mojom_type(ty);
    match packed_ty {
        MojomWireType::Leaf { leaf_type: PackedLeafType::Bool } => {
            StructuredBodyElement::Bitfield([
                Some(ordinal),
                None,
                None,
                None,
                None,
                None,
                None,
                None,
            ])
        }
        _ => StructuredBodyElement::SingleValue(ordinal, packed_ty),
    }
}

/// Create a structured body out of an array. This can be thought of as part of
/// the packing algorithm, but we can't actually run it until we know the number
/// of elements in the array.
///
/// FOR_RELEASE: It would be nice to return an iterator directly, but the
/// cleanest way to do that (AFAIK) is to use itertools::Either, and that crate
/// isn't yet approved for use in chromium.
pub fn pack_array_body<'a>(
    element_type: &'a MojomWireType,
    num_elements: usize,
) -> Vec<StructuredBodyElementMixed<'a>> {
    match element_type {
        // If the array contains booleans we'll need to convert them to bitfields
        // with the proper ordinals
        MojomWireType::Leaf { leaf_type: PackedLeafType::Bool } => {
            let num_full_bitfields = num_elements / 8; // Round down
            let remaining_bools = num_elements % 8;
            let num_partial_bitfields = if remaining_bools == 0 { 0 } else { 1 };
            let mut bitfields: Vec<[Option<Ordinal>; 8]> =
                Vec::with_capacity(num_full_bitfields + num_partial_bitfields);
            for i in 0..num_full_bitfields {
                bitfields.push(std::array::from_fn(|idx| Some(i * 8 + idx)))
            }
            if remaining_bools != 0 {
                bitfields.push(std::array::from_fn(|idx| {
                    if idx < remaining_bools {
                        Some(num_full_bitfields * 8 + idx)
                    } else {
                        None
                    }
                }))
            }

            return bitfields
                .into_iter()
                .map(|bitfield| StructuredBodyElement::Bitfield(bitfield))
                .collect();
        }
        // All non-bool types just pack as themselves, in order
        _ => {
            return (0..num_elements)
                .map(|idx| StructuredBodyElement::SingleValue(idx, element_type))
                .collect();
        }
    }
}
