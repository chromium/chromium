// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Defines functions for parsing encoded data into Mojom values.
//!
//! Because Mojom's wire format is not self-describing, the functions in this
//! module need to know the type of the data they're parsing. Their job is to
//! take an encoded value and produce a MojomValue of the corresponding type.
//!
//! These functions encode knowledge of Mojom's wire format, and are independent
//! of any particular message.

// IMPORTANT! These functions require the input MojomTypes to have
// their fields in wire order. Use pack_mojom_type in pack.rs to ensure this.
// In the future, we'll probably need a separate type for wire order, since
// (at the very least) MojomType can't handle bitfields.

use crate::ast::*;
use crate::parse_primitives::*;

use anyhow::{bail, Context, Result};

/// Parse a type without nested data, i.e. anything but a struct or array
fn parse_leaf_element(data: &mut ParserData, ty: &PackedLeafType) -> Result<MojomValue> {
    match ty {
        PackedLeafType::UInt8 => Ok(MojomValue::UInt8(parse_u8(data)?)),
        PackedLeafType::UInt16 => Ok(MojomValue::UInt16(parse_u16(data)?)),
        PackedLeafType::UInt32 => Ok(MojomValue::UInt32(parse_u32(data)?)),
        PackedLeafType::UInt64 => Ok(MojomValue::UInt64(parse_u64(data)?)),
        PackedLeafType::Int8 => Ok(MojomValue::Int8(parse_i8(data)?)),
        PackedLeafType::Int16 => Ok(MojomValue::Int16(parse_i16(data)?)),
        PackedLeafType::Int32 => Ok(MojomValue::Int32(parse_i32(data)?)),
        PackedLeafType::Int64 => Ok(MojomValue::Int64(parse_i64(data)?)),
    }
}

/// Parse and ignore the contents of as many bytes as necessary to meet the
/// given alignment requirement.
///
/// Does not validate that the skipped bytes were 0, since it's possible that
/// we're parsing a message with a version greater than our mojom file knows
/// about.
fn skip_to_alignment(data: &mut ParserData, alignment: usize) -> Result<()> {
    let mismatch = data.bytes_parsed() % alignment;
    if mismatch == 0 {
        Ok(())
    } else {
        parse_padding(data, alignment - mismatch)
    }
}

// Mojom structs and arrays are never nested inside each other. Instead, they
// are represented by a pointer with an offset to the actual data which appears
// later in the message.
// The actual nested structs/arrays will appear, in declaration order, after all
// the other fields. This struct holds the information we need to parse and
// validate them, when we reach them.

/// Information about a nested struct/array, which we expect to see later.
struct NestedDataInfo<'a> {
    ty: &'a PackedStructuredType,
    field_name: String,
    ordinal: Ordinal,
    /// The expected location of the nested data, as an offset in bytes from the
    /// start of the enclosing struct
    expected_offset: usize,
}

/// Return the highest ordinal that appears in a packed struct.
// FOR_RELEASE: It would be nicer to store this in the wire type so we
// don't need to compute it each time.
fn get_max_ordinal(fields: &Vec<(String, MojomWireType)>) -> Ordinal {
    use std::cmp::max;
    let mut max_so_far: Ordinal = 0;
    for (_, wire_type) in fields.into_iter() {
        match wire_type {
            MojomWireType::Leaf { ordinal, .. } => max_so_far = max(max_so_far, *ordinal),
            MojomWireType::Pointer { ordinal, .. } => max_so_far = max(max_so_far, *ordinal),
            MojomWireType::Bitfield { ordinals } => {
                let mut iter = ordinals.into_iter();
                while let Some(Some(ordinal)) = iter.next() {
                    max_so_far = max(max_so_far, *ordinal)
                }
            }
        }
    }
    return max_so_far;
}

pub fn parse_struct(
    data: &mut ParserData,
    fields: &Vec<(String, MojomWireType)>,
) -> Result<Vec<(String, MojomValue)>> {
    let initial_bytes_parsed = data.bytes_parsed();

    // Parse the struct header
    let size_in_bytes: usize = parse_u32(data)?.try_into()?;
    let _version_number = parse_u32(data)?; // We're ignoring versioning for now

    let mut nested_data_list: Vec<NestedDataInfo> = vec![];

    // Pre-allocate space for the parsed values, so we can write directly into them by
    // index. We have to provide dummy values since rust won't allow uninitialized memory.

    let mut ret: Vec<(String, MojomValue)> =
        vec![(String::new(), MojomValue::Int8(0)); get_max_ordinal(fields) + 1];
    for (name, mojom_wire_type) in fields {
        // Make sure we're at the right alignment for this field
        skip_to_alignment(data, mojom_wire_type.alignment())?;

        match mojom_wire_type {
            // Nested structured data, record for later
            MojomWireType::Pointer { ordinal, nested_data_type } => {
                let pointer_value = parse_u64(data)?;
                let nested_info = NestedDataInfo {
                    ty: nested_data_type,
                    ordinal: *ordinal,
                    field_name: name.clone(),
                    expected_offset: data.bytes_parsed() - initial_bytes_parsed
                        - 8 // Don't count the bytes we just parsed
                        + usize::try_from(pointer_value)
                            .context("Pointer value {pointer_value} doesn't fit into usize?!")?,
                };
                nested_data_list.push(nested_info);
            }
            // Nested leaf data, just parse it
            MojomWireType::Leaf { ordinal, leaf_type } => {
                let parsed_value = parse_leaf_element(data, leaf_type)?;
                ret[*ordinal] = (name.clone(), parsed_value);
            }
            MojomWireType::Bitfield { ordinals } => {
                let mut iter = ordinals.into_iter().enumerate();
                let parsed_bits = parse_u8(data)?;
                while let Some((idx, Some(ordinal))) = iter.next() {
                    let bit = (parsed_bits >> idx) & 1;
                    ret[*ordinal] = (name.clone(), MojomValue::Bool(bit == 1))
                }
            }
        };
    }

    // We've reached the end of the struct (not including nested data!)
    // Make sure we parsed the expected number of bytes.
    let bytes_parsed_so_far = data.bytes_parsed() - initial_bytes_parsed;
    if bytes_parsed_so_far > size_in_bytes {
        bail!(
            "Struct claimed to have {} bytes, but we somehow parsed {} bytes",
            size_in_bytes,
            bytes_parsed_so_far
        )
    } else if bytes_parsed_so_far < size_in_bytes {
        parse_padding(data, size_in_bytes - bytes_parsed_so_far)?
    }

    for nested_data in nested_data_list {
        // Nested data is required to appear in the same order as the (packed) fields
        // of the struct. So the expected offset is only useful for validation.
        let bytes_parsed_so_far = data.bytes_parsed() - initial_bytes_parsed;
        if nested_data.expected_offset != bytes_parsed_so_far {
            bail!(
                "Nested field {} was at {} bytes from the beginning of the struct, \
                 but expected to be at {} bytes",
                nested_data.field_name,
                bytes_parsed_so_far,
                nested_data.expected_offset
            );
        }
        let parsed_data = match nested_data.ty {
            PackedStructuredType::Struct { packed_field_types } => {
                let parsed_fields = parse_struct(data, packed_field_types)?;
                MojomValue::Struct(parsed_fields)
            }
            PackedStructuredType::Array { .. } => {
                bail!("Arrays are not yet implemented");
            }
        };
        ret[nested_data.ordinal] = (nested_data.field_name, parsed_data);
    }
    Ok(ret)
}
