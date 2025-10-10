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

/// Represents a single field which has been packed into wire format, with
/// enough information to both construct the binary representation and map back
/// to the original type.
struct PackedField<'a> {
    /// The name of the field in the original struct definition.
    name: &'a str,
    /// The type of the field.
    ty: &'a MojomType,
    /// The ordinal number of the field in the original struct definition.
    _ordinal: usize,
    /// Number of bytes from the beginning of the struct to the start of the
    /// field.
    start_offset: u32,
    /// Number of bytes from the beginning of the struct to the end of the
    /// field.
    end_offset: u32,
}

impl<'a> PackedField<'a> {
    /// Create a new PackedField given the original field's information and its
    /// location
    fn new(field: &'a (String, MojomType), ordinal: usize, start_offset: u32) -> PackedField<'a> {
        PackedField {
            name: &field.0,
            ty: &field.1,
            start_offset: start_offset,
            end_offset: start_offset + get_size_and_alignment(&field.1),
            _ordinal: ordinal,
        }
    }
}

/// Return the number of bytes we need to skip to reach the given alignment.
fn bytes_to_align(current_offset: u32, required_alignment: u32) -> u32 {
    return (required_alignment - (current_offset % required_alignment)) % required_alignment;
}

/// Transform the fields of a Mojom struct into their packed representation.
/// This uses the basic algorithm from
/// mojo/public/tools/mojom/mojom/generate/pack.py
fn pack_struct(fields: &Vec<(String, MojomType)>) -> MojomType {
    let mut packed_fields: Vec<PackedField> = vec![];
    let mut total_length = 0;
    // For each field, see if we can fit it between two existing packed fields.
    // If not, put it at the end.
    'outer: for (ordinal, field) in fields.iter().enumerate() {
        let field_size = get_size_and_alignment(&field.1);
        // Try every pair (i-1, i) of adjacent packed fields.
        for i in 1..packed_fields.len() {
            let end_of_last_field = packed_fields[i - 1].end_offset;
            let empty_space = packed_fields[i].start_offset - end_of_last_field;
            // If we fit, then pack this field here
            if (field_size + bytes_to_align(end_of_last_field, field_size)) <= empty_space {
                packed_fields.insert(
                    i,
                    PackedField::new(
                        field,
                        ordinal,
                        end_of_last_field + bytes_to_align(end_of_last_field, field_size),
                    ),
                );
                continue 'outer;
            }
        }
        // If we get here, we weren't able to fit it in anywhere.
        let packed_field = PackedField::new(
            field,
            ordinal,
            total_length + bytes_to_align(total_length, field_size),
        );
        total_length = packed_field.end_offset;
        packed_fields.push(packed_field);
    }

    // Transform each packed field back into a regular MojomType
    // Also recursively pack each one, to handle nested structs.
    return MojomType::Struct {
        fields: packed_fields
            .into_iter()
            .map(|packed_field| (packed_field.name.to_string(), pack_mojom_type(packed_field.ty)))
            .collect(),
    };
}

/// Given a MojomType, return its packed representation.
pub fn pack_mojom_type(ty: &MojomType) -> MojomType {
    match ty {
        MojomType::Struct { fields } => pack_struct(fields),
        // Non-struct types just pack as themselves.
        _ => ty.clone(),
    }
}
