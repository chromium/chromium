// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ast::*;

use anyhow::{bail, Context, Result};
use std::collections::HashMap;

fn get_field_at_ordinal(field_values: &[MojomValue], ordinal: Ordinal) -> Result<&MojomValue> {
    let field_value = field_values.get(ordinal).with_context(|| {
        format!(
            "Wire type asked for field with ordinal {}, but there are only {} fields.",
            ordinal,
            field_values.len()
        )
    })?;
    Ok(field_value)
}

// FOR_RELEASE: If we can figure out how to make it typecheck, it would be nicer
// to have a single function/macro that validates the type and extracts the
// value at once. Currently we have to match twice, since the rust type system
// forgets that we validated the type.
fn check_value_has_expected_type(value: &MojomValue, expected_type: &MojomWireType) -> Result<()> {
    let matches = match (expected_type, value) {
        (MojomWireType::Leaf { leaf_type, .. }, _) => match (leaf_type, value) {
            (PackedLeafType::Int8, MojomValue::Int8(_))
            | (PackedLeafType::UInt8, MojomValue::UInt8(_))
            | (PackedLeafType::Int16, MojomValue::Int16(_))
            | (PackedLeafType::UInt16, MojomValue::UInt16(_))
            | (PackedLeafType::Int32, MojomValue::Int32(_))
            | (PackedLeafType::UInt32, MojomValue::UInt32(_))
            | (PackedLeafType::Int64, MojomValue::Int64(_))
            | (PackedLeafType::UInt64, MojomValue::UInt64(_))
            | (PackedLeafType::Enum { .. }, MojomValue::Enum(_)) => true,
            _ => false,
        },
        (MojomWireType::Bitfield { .. }, MojomValue::Bool(_))
        | (MojomWireType::Union { .. }, MojomValue::Union { .. }) => true,
        (MojomWireType::Pointer { nested_data_type, .. }, _) => match (nested_data_type, value) {
            (PackedStructuredType::Struct { .. }, MojomValue::Struct { .. })
            // FOR_RELEASE: Should we care about which type of array this was originally?
            | (PackedStructuredType::Array { .. }, MojomValue::Array { .. })
            | (PackedStructuredType::Array { .. }, MojomValue::String { .. })
            | (PackedStructuredType::Union { .. }, MojomValue::Union { .. }) => true,
            _ => false,
        },
        _ => false,
    };
    if matches {
        return Ok(());
    } else {
        bail!("Expected to find type {:?}, but got value {:?}", expected_type, value)
    }
}

fn pad_to_alignment(data: &mut Vec<u8>, alignment: usize) {
    let mismatch = data.len() % alignment;
    if mismatch != 0 {
        data.extend(vec![0; alignment - mismatch])
    }
}

/// Write out the bytes for a leaf node
fn deparse_leaf_value(data: &mut Vec<u8>, value: &MojomValue) -> Result<()> {
    match value {
        MojomValue::Int8(value) => data.extend(value.to_le_bytes()),
        MojomValue::UInt8(value) => data.extend(value.to_le_bytes()),
        MojomValue::Int16(value) => data.extend(value.to_le_bytes()),
        MojomValue::UInt16(value) => data.extend(value.to_le_bytes()),
        MojomValue::Int32(value) => data.extend(value.to_le_bytes()),
        MojomValue::UInt32(value) => data.extend(value.to_le_bytes()),
        MojomValue::Int64(value) => data.extend(value.to_le_bytes()),
        MojomValue::UInt64(value) => data.extend(value.to_le_bytes()),
        MojomValue::Enum(value) => data.extend(value.to_le_bytes()),
        _ => bail!("deparse_leaf_value: {:?} is not a leaf value", value),
    }
    Ok(())
}

// Mojom structs and arrays are never nested inside each other. Instead, they
// are represented by a pointer with an offset to the actual data which appears
// later in the message.

enum NestedData<'a> {
    Struct { field_values: &'a [MojomValue], packed_fields: &'a [MojomWireType] },
    Array { elements: &'a Vec<MojomValue>, element_type: &'a Box<MojomWireType> },
    Union { tag: u32, value: &'a Box<MojomValue>, variants: &'a HashMap<u32, MojomWireType> },
}
/// Information about a nested struct/array, which we will emit later
struct NestedDataInfo<'a> {
    nested_data: NestedData<'a>,
    // Logically this can be thought of as a &mut [u8; 8] pointing to the bytes
    // in the data vector in which we'll store the pointer value. But since the
    // borrow checker isn't that fine in granularity, we store its index instead.
    ptr_loc: usize,
}

/// Overwrite data[start..start+len] with the contents of `value`.
/// Panics if the range doesn't exist or the `value` is too short, probably.
fn write_to_slice(data: &mut Vec<u8>, start: usize, len: usize, value: &[u8]) {
    let struct_size_field: &mut [u8] = &mut data[start..start + len];
    struct_size_field.copy_from_slice(&value[0..len]);
}

pub fn deparse_struct(
    data: &mut Vec<u8>,
    field_values: &[MojomValue],
    packed_fields: &[MojomWireType],
) -> Result<()> {
    // Write the struct's header
    data.extend([0; 4]); // Size; we'll fill this in later
    data.extend([0; 4]); // Version number; not supported yet
    deparse_structured_body(data, None, field_values, packed_fields)
}

/// Serialize a union to the wire.
///
/// See the documentation of parse_union in parse_values.rs for an explanation
/// of the `enclosing_nested_data_list` argument
fn deparse_union<'a>(
    data: &mut Vec<u8>,
    enclosing_nested_data_list: Option<&mut Vec<NestedDataInfo<'a>>>,
    tag: u32,
    value: &'a Box<MojomValue>,
    variants: &'a HashMap<u32, MojomWireType>,
) -> Result<()> {
    // Write the union's header
    let expected_wire_type = variants
        .get(&tag)
        .context(format!("Tag value {tag} is not a valid discriminant for this union!"))?;

    data.extend([0; 4]); // Size; we'll fill this in later
    data.extend(tag.to_le_bytes()); // Union tag

    deparse_structured_body(
        data,
        enclosing_nested_data_list,
        std::slice::from_ref(&*value),
        std::slice::from_ref(expected_wire_type),
    )
}

/// Deparse the fields of a struct (or union) after having parsed its header
///
/// See the documentation of parse_union in parse_values.rs for an explanation
/// of the `enclosing_nested_data_list` argument
// FOR_RELEASE: Try to take the value by value instead of by reference
fn deparse_structured_body<'a>(
    data: &mut Vec<u8>,
    enclosing_nested_data_list: Option<&mut Vec<NestedDataInfo<'a>>>,
    field_values: &'a [MojomValue],
    packed_fields: &'a [MojomWireType],
) -> Result<()> {
    // Start counting from the beginning of the header, which we already wrote
    let initial_bytes = data.len() - 8;

    let mut local_nested_data_list: Vec<NestedDataInfo> = vec![];
    let nested_data_list = match enclosing_nested_data_list {
        Some(list) => list,
        None => &mut local_nested_data_list,
    };

    // Go through all the fields and either write them to the vector, or
    // (for nested data) prepare for them to be written later, in order.
    for packed_field in packed_fields {
        match packed_field {
            MojomWireType::Leaf { ordinal, leaf_type: _ } => {
                let leaf_value = get_field_at_ordinal(field_values, *ordinal)?;
                check_value_has_expected_type(leaf_value, packed_field)?;
                pad_to_alignment(data, packed_field.alignment());
                deparse_leaf_value(data, leaf_value)?
            }
            MojomWireType::Bitfield { ordinals } => {
                let mut iter = ordinals.into_iter().enumerate();
                let mut bitfield: u8 = 0;
                // Construct the bitfield bit-by-bit
                while let Some((idx, Some(ordinal))) = iter.next() {
                    let bit_value = get_field_at_ordinal(field_values, *ordinal)?;
                    if let MojomValue::Bool(bit) = bit_value {
                        bitfield |= (*bit as u8) << idx;
                    } else {
                        // We know this will fail, but calling it lets us avoid
                        // writing a custom error message here.
                        check_value_has_expected_type(bit_value, packed_field)?;
                    }
                }
                // Now we've set all the bits, write it to the wire
                data.push(bitfield)
            }
            MojomWireType::Pointer { ordinal, nested_data_type } => {
                let nested_data_value = get_field_at_ordinal(field_values, *ordinal)?;
                let nested_data = match (nested_data_value, nested_data_type) {
                    (
                        MojomValue::Struct(_field_names, nested_data_fields),
                        PackedStructuredType::Struct { packed_field_types, packed_field_names: _ },
                    ) => NestedData::Struct {
                        field_values: nested_data_fields,
                        packed_fields: packed_field_types,
                    },
                    (
                        MojomValue::Array(nested_data_fields),
                        PackedStructuredType::Array { element_type, .. },
                    ) => NestedData::Array { elements: nested_data_fields, element_type },
                    (
                        MojomValue::Union(tag, value),
                        PackedStructuredType::Union { variants, .. },
                    ) => NestedData::Union { tag: *tag, value, variants },
                    _ => bail!(
                        "Unexpected type for nested data: Expected {:?}, got {:?}",
                        nested_data_type,
                        nested_data_value
                    ),
                };
                nested_data_list.push(NestedDataInfo { nested_data, ptr_loc: data.len() });
                // Allocate space for the pointer, we'll write to it later.
                pad_to_alignment(data, 8);
                data.extend([0; 8]);
            }
            MojomWireType::Union { ordinal, variants } => {
                let union_value = get_field_at_ordinal(field_values, *ordinal)?;
                let (tag, contained_value) = match union_value {
                    MojomValue::Union(tag, value) => (tag, value),
                    _ => {
                        // We know this will fail, but calling it lets us avoid
                        // writing a custom error message here.
                        return check_value_has_expected_type(union_value, packed_field);
                    }
                };
                pad_to_alignment(data, 8);
                deparse_union(data, Some(nested_data_list), *tag, contained_value, variants)?;
            }
        }
    }

    // Struct bodies must be a multiple of 8 bytes.
    pad_to_alignment(data, 8);

    let bytes_written = data.len() - initial_bytes;
    // Write the length of the struct to the first 4 bytes of the header
    // The usize->u32 cast should always work, because hopefully our message
    // is less than 2^32 bytes long!
    write_to_slice(data, initial_bytes, 4, &u32::to_le_bytes(bytes_written.try_into().unwrap()));

    for nested_data_info in local_nested_data_list {
        // Write to this nested data's pointer.
        let bytes_from_ptr = data.len() - nested_data_info.ptr_loc;
        write_to_slice(
            data,
            nested_data_info.ptr_loc,
            8,
            &u64::to_le_bytes(bytes_from_ptr.try_into().unwrap()),
        );

        match nested_data_info.nested_data {
            NestedData::Struct { field_values, packed_fields } => {
                deparse_struct(data, field_values, packed_fields)?
            }
            NestedData::Array { elements, element_type } => {
                let _ = (elements, element_type); // Selectively silence unused code warning
                bail!("Arrays not yet implemented")
            }
            NestedData::Union { tag, value, variants } => {
                deparse_union(data, None, tag, value, variants)?
            }
        }
    }

    Ok(())
}

/// Serialize a single mojom value of the given type, outside the context of a
/// struct. This function is only useful for unit testing, since all mojom
/// values in practice are members of a struct. The function only works for
/// some mojom types, since e.g. booleans can't be parsed individually.
pub fn deparse_single_value_for_testing(
    value: &MojomValue,
    wire_type: &MojomWireType,
) -> Result<Vec<u8>> {
    let mut data: Vec<u8> = vec![];
    match (wire_type, value) {
        (MojomWireType::Leaf { .. }, _) => deparse_leaf_value(&mut data, value)?,
        (MojomWireType::Bitfield { .. }, _) => {
            unimplemented!("Bitfields cannot be deparsed individually")
        }
        (
            MojomWireType::Pointer {
                nested_data_type:
                    PackedStructuredType::Struct { packed_field_types, packed_field_names: _ },
                ..
            },
            MojomValue::Struct(_field_names, fields),
        ) => deparse_struct(&mut data, fields, packed_field_types)?,
        (
            MojomWireType::Pointer { nested_data_type: PackedStructuredType::Array { .. }, .. },
            MojomValue::Array(_),
        ) => panic!("Arrays are not yet implemented"),
        (
            MojomWireType::Union { variants, .. }
            | MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Union { variants },
                ..
            },
            MojomValue::Union(tag, value),
        ) => deparse_union(&mut data, None, *tag, value, variants)?,
        _ => {
            // This will fail, just calling for the error message
            check_value_has_expected_type(value, wire_type)?
        }
    };
    Ok(data)
}
