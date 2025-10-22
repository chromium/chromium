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

/// Return the number of bytes we need to skip to reach the given alignment.
fn bytes_to_align(current_offset: usize, required_alignment: usize) -> usize {
    return (required_alignment - (current_offset % required_alignment)) % required_alignment;
}

/// Represents a single field which has been packed into wire format, with enough
/// information to both construct the binary representation and map back to the
/// original type.
struct PackedField<'a> {
    /// The name of the field in the original struct definition.
    name: &'a str,
    /// The type of the field, which has been recursively packed.
    ty: MojomWireType,
    /// Number of bytes from the beginning of the struct to the start of the field.
    start_offset: usize,
    /// Number of bytes from the beginning of the struct to the end of the field.
    end_offset: usize,
}

impl<'a> PackedField<'a> {
    /// Create a new PackedField given the original field's information and its location
    fn new(name: &'a str, ty: MojomWireType, start_offset: usize) -> PackedField<'a> {
        PackedField { start_offset: start_offset, end_offset: start_offset + ty.size(), name, ty }
    }
}

/// Checks if the packed field is a bitfield with an empty slot, and inserts
/// the ordinal if so.
///
/// Returns true if the bool was successfully packed, and false otherwise.
fn try_pack_bool(ordinal: Ordinal, packed_field: &mut MojomWireType) -> bool {
    match packed_field {
        MojomWireType::Bitfield { ordinals } => {
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
/// This uses the basic algorithm from mojo/public/tools/mojom/mojom/generate/pack.py
fn pack_struct(fields: &Vec<(String, MojomType)>) -> Vec<(String, MojomWireType)> {
    let mut packed_fields: Vec<PackedField> = vec![];
    let mut total_length = 0;
    // For each field, see if we can fit it between two existing packed fields.
    // If not, put it at the end.
    'outer: for (ordinal, (field_name, field_ty)) in fields.iter().enumerate() {
        let is_bool = match field_ty {
            MojomType::Bool => true,
            _ => false,
        };
        // Recursively pack any structs this field contains
        let field_ty = pack_mojom_type(field_ty, ordinal);
        let field_size = field_ty.size();
        // Try every pair (i-1, i) of adjacent packed fields.
        for i in 1..packed_fields.len() {
            let end_of_last_field = packed_fields[i - 1].end_offset;
            let empty_space = packed_fields[i].start_offset - end_of_last_field;
            if is_bool && try_pack_bool(ordinal, &mut packed_fields[i - 1].ty) {
                continue 'outer;
            };
            // If we fit, then pack this field here
            if (field_size + bytes_to_align(end_of_last_field, field_size)) <= empty_space {
                packed_fields.insert(
                    i,
                    PackedField::new(
                        field_name,
                        field_ty,
                        end_of_last_field + bytes_to_align(end_of_last_field, field_size),
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
            total_length + bytes_to_align(total_length, field_size),
        );
        total_length = packed_field.end_offset;
        packed_fields.push(packed_field);
    }

    // Transform each packed field back into a regular MojomType
    // Also recursively pack each one, to handle nested structs.
    return packed_fields
        .into_iter()
        .map(|packed_field| (packed_field.name.to_string(), packed_field.ty))
        .collect();
}

/// Given a MojomType, return its packed representation.
pub fn pack_mojom_type(ty: &MojomType, ordinal: Ordinal) -> MojomWireType {
    match ty {
        MojomType::Struct { fields } => MojomWireType::Pointer {
            ordinal,
            nested_data_type: PackedStructuredType::Struct {
                packed_field_types: pack_struct(fields),
            },
        },
        MojomType::Array { element_type, num_elements } => {
            let array_type = match num_elements {
                None => PackedArrayType::UnsizedArray,
                Some(n) => PackedArrayType::SizedArray(*n),
            };

            MojomWireType::Pointer {
                ordinal,
                nested_data_type: PackedStructuredType::Array {
                    element_type: Box::new(pack_mojom_type(element_type, 0)),
                    array_type,
                },
            }
        }
        // Strings are packed as byte arrays
        MojomType::String => MojomWireType::Pointer {
            ordinal,
            nested_data_type: PackedStructuredType::Array {
                element_type: Box::new(pack_mojom_type(&MojomType::UInt8, 0)),
                array_type: PackedArrayType::String,
            },
        },
        MojomType::Int8 => MojomWireType::Leaf { ordinal, leaf_type: PackedLeafType::Int8 },
        MojomType::Int16 => MojomWireType::Leaf { ordinal, leaf_type: PackedLeafType::Int16 },
        MojomType::Int32 => MojomWireType::Leaf { ordinal, leaf_type: PackedLeafType::Int32 },
        MojomType::Int64 => MojomWireType::Leaf { ordinal, leaf_type: PackedLeafType::Int64 },
        MojomType::UInt8 => MojomWireType::Leaf { ordinal, leaf_type: PackedLeafType::UInt8 },
        MojomType::UInt16 => MojomWireType::Leaf { ordinal, leaf_type: PackedLeafType::UInt16 },
        MojomType::UInt32 => MojomWireType::Leaf { ordinal, leaf_type: PackedLeafType::UInt32 },
        MojomType::UInt64 => MojomWireType::Leaf { ordinal, leaf_type: PackedLeafType::UInt64 },
        MojomType::Bool => MojomWireType::Bitfield {
            ordinals: [Some(ordinal), None, None, None, None, None, None, None],
        },
    }
}
