// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ast::*;

use anyhow::{bail, Context, Result};

fn get_field_at_ordinal(
    field_values: &Vec<(String, MojomValue)>,
    ordinal: Ordinal,
) -> Result<&MojomValue> {
    let (_field_name, field_value) = field_values.get(ordinal).with_context(|| {
        format!(
            "Wire type asked for field with ordinal {}, but there are only {} fields.",
            ordinal,
            field_values.len()
        )
    })?;
    Ok(field_value)
}

// FOR_RELEASE: If we can figure out how to make it typecheck, it would be nicer to have a
// single function/macro that validates the type and extracts the value at once. Currently we
// have to match twice, since the rust type system forgets that we validated the type.
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
            | (PackedLeafType::UInt64, MojomValue::UInt64(_)) => true,
            _ => false,
        },
        (MojomWireType::Bitfield { .. }, MojomValue::Bool(_)) => true,
        (MojomWireType::Pointer { nested_data_type, .. }, _) => match (nested_data_type, value) {
            (PackedStructuredType::Struct { .. }, MojomValue::Struct { .. })
            // FOR_RELEASE: Should we care about which type of array this was originally?
            | (PackedStructuredType::Array { .. }, MojomValue::Array { .. })
            | (PackedStructuredType::Array { .. }, MojomValue::String { .. }) => true,
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
        _ => bail!("deparse_leaf_value: {:?} is not a leaf value", value),
    }
    Ok(())
}

// Mojom structs and arrays are never nested inside each other. Instead, they
// are represented by a pointer with an offset to the actual data which appears
// later in the message.

enum NestedData<'a> {
    Struct {
        field_values: &'a Vec<(String, MojomValue)>,
        packed_fields: &'a Vec<(String, MojomWireType)>,
    },
    Array {
        elements: &'a Vec<MojomValue>,
        element_type: &'a Box<MojomWireType>,
    },
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
    field_values: &Vec<(String, MojomValue)>,
    packed_fields: &Vec<(String, MojomWireType)>,
) -> Result<()> {
    let initial_bytes = data.len();
    // Write the struct's header
    data.extend([0; 4]); // Size; we'll fill this in later
    data.extend([0; 4]); // Version number; not supported yet

    // Go through all the fields and either write them to the vector, or
    // (for nested data) prepare for them to be written later, in order.
    let mut nested_data_infos: Vec<NestedDataInfo> = vec![];
    for (_name, packed_field) in packed_fields {
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
                pad_to_alignment(data, 8);
                let nested_data = match (nested_data_value, nested_data_type) {
                    (
                        MojomValue::Struct(nested_data_fields),
                        PackedStructuredType::Struct { packed_field_types },
                    ) => NestedData::Struct {
                        field_values: nested_data_fields,
                        packed_fields: packed_field_types,
                    },
                    (
                        MojomValue::Array(nested_data_fields),
                        PackedStructuredType::Array { element_type, .. },
                    ) => NestedData::Array { elements: nested_data_fields, element_type },
                    _ => bail!(
                        "Unexpected type for nested data: Expected {:?}, got {:?}",
                        nested_data_type,
                        nested_data_value
                    ),
                };
                nested_data_infos.push(NestedDataInfo { nested_data, ptr_loc: data.len() });
                // Allocate space for the pointer, we'll write to it later.
                pad_to_alignment(data, 8);
                data.extend([0; 8]);
            }
        }
    }

    // Struct bodies must be a multiple of 8 bytes.
    pad_to_alignment(data, 8);

    let bytes_written = data.len() - initial_bytes;
    // Write the length of the struct to the first 4 bytes of the header
    write_to_slice(data, initial_bytes, 4, &usize::to_le_bytes(bytes_written));

    for nested_data_info in nested_data_infos {
        // Write to this nested data's pointer.
        let bytes_from_ptr = data.len() - nested_data_info.ptr_loc;
        write_to_slice(data, nested_data_info.ptr_loc, 8, &usize::to_le_bytes(bytes_from_ptr));

        match nested_data_info.nested_data {
            NestedData::Struct { field_values, packed_fields } => {
                deparse_struct(data, field_values, packed_fields)?
            }
            NestedData::Array { elements, element_type } => {
                let _ = (elements, element_type); // Selectively silence unused code warning
                bail!("Arrays not yet implemented")
            }
        }
    }

    Ok(())
}
