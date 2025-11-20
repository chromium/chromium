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
use crate::errors::*;
use crate::parse_primitives::*;
use std::collections::HashMap;

/// Parse a type without nested data, i.e. anything but a struct or array
fn parse_leaf_element(data: &mut ParserData, ty: &PackedLeafType) -> ParsingResult<MojomValue> {
    let _ = (parse_f32, parse_f64); // Silence dead code warning until this is used
    match ty {
        PackedLeafType::UInt8 => Ok(MojomValue::UInt8(parse_u8(data)?)),
        PackedLeafType::UInt16 => Ok(MojomValue::UInt16(parse_u16(data)?)),
        PackedLeafType::UInt32 => Ok(MojomValue::UInt32(parse_u32(data)?)),
        PackedLeafType::UInt64 => Ok(MojomValue::UInt64(parse_u64(data)?)),
        PackedLeafType::Int8 => Ok(MojomValue::Int8(parse_i8(data)?)),
        PackedLeafType::Int16 => Ok(MojomValue::Int16(parse_i16(data)?)),
        PackedLeafType::Int32 => Ok(MojomValue::Int32(parse_i32(data)?)),
        PackedLeafType::Int64 => Ok(MojomValue::Int64(parse_i64(data)?)),
        PackedLeafType::Enum { is_valid } => {
            let value = parse_u32(data)?;
            if is_valid.call(value) {
                Ok(MojomValue::Enum(value))
            } else {
                // Report the error starting before the 32 bits we just parsed
                Err(ParsingError::invalid_discriminant(data.bytes_parsed() - 4, value))
            }
        }
    }
}

/// Parse and ignore the contents of as many bytes as necessary to meet the
/// given alignment requirement.
///
/// Does not validate that the skipped bytes were 0, since it's possible that
/// we're parsing a message with a version greater than our mojom file knows
/// about.
fn skip_to_alignment(data: &mut ParserData, alignment: usize) -> ParsingResult<()> {
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
    /// Tracks whether this pointer was contained in a union, and if so what
    /// its discriminant was.
    union_discriminant: Option<u32>,
}

/// Return the highest ordinal that appears in a packed struct.
// FOR_RELEASE: It would be nicer to store this in the wire type so we
// don't need to compute it each time.
fn get_max_ordinal(fields: &[MojomWireType]) -> Option<Ordinal> {
    use std::cmp::max;
    if fields.is_empty() {
        return None;
    }

    let mut max_so_far: Ordinal = 0;
    for wire_type in fields.into_iter() {
        match wire_type {
            MojomWireType::Leaf { ordinal, .. }
            | MojomWireType::Pointer { ordinal, .. }
            | MojomWireType::Union { ordinal, .. } => max_so_far = max(max_so_far, *ordinal),
            MojomWireType::Bitfield { ordinals } => {
                let mut iter = ordinals.into_iter();
                while let Some(Some(ordinal)) = iter.next() {
                    max_so_far = max(max_so_far, *ordinal)
                }
            }
        }
    }
    return Some(max_so_far);
}

/// Parse a 32-bit size as part of a struct or array header
pub fn parse_size(data: &mut ParserData) -> ParsingResult<usize> {
    let parsed_value = parse_u32(data)?;
    let mk_err = || Err(ParsingError::invalid_size(data.bytes_parsed() - 4, parsed_value));
    let size = parsed_value.try_into().or_else(|_| mk_err())?;
    if size < 8 || size % 8 != 0 {
        return mk_err();
    }
    Ok(size)
}

pub fn parse_pointer(data: &mut ParserData, may_be_null: bool) -> ParsingResult<usize> {
    let parsed_value = parse_u64(data)?;
    let mk_err = || Err(ParsingError::invalid_pointer(data.bytes_parsed() - 8, parsed_value));
    let ptr_value = parsed_value.try_into().or_else(|_| mk_err())?;
    if ptr_value == 0 && may_be_null {
        return Ok(ptr_value);
    }
    if ptr_value < 8 || ptr_value % 8 != 0 {
        return mk_err();
    }
    Ok(ptr_value)
}

pub fn parse_struct(
    data: &mut ParserData,
    field_names: &[String],
    fields: &[MojomWireType],
) -> ParsingResult<MojomValue> {
    // Parse the struct header
    let size_in_bytes = parse_size(data)?;
    let _version_number = parse_u32(data)?; // We're ignoring versioning for now
    let (parsed_names, parsed_fields) =
        parse_structured_body(data, None, size_in_bytes, field_names, fields)?;
    Ok(MojomValue::Struct(parsed_names, parsed_fields))
}

/// Parse a union value.
///
/// Despite being structured data, union values might be packed directly in the
/// body of a struct or array, instead of being behind a pointer. This can cause
/// problems if the union itself contains a pointer.
///
/// Specifically, if a union (1) contains a pointer and (2) is packed directly
/// into an enclosing body, then then the pointed-to data will appear at the end
/// of the enclosing body instead of the end of the union.
///
/// The `enclosing_nested_data_list` argument to this function tracks which case
/// we are in. If the union value we are parsing appears in an enclosing body,
/// then it should be Some; otherwise, it should be None.
///
/// If `enclosing_nested_data_list` is Some and the union contains a pointer,
/// then pointer's information will be appended to the contained list, and the
/// returned MojomValue will be None. The actual union value will have to be
/// constructed later.
///
/// Otherwise, the returned MojomValue will be Some, and no further work is
/// required.
///
/// This function also returns the union's discriminant value.
fn parse_union<'a>(
    data: &mut ParserData,
    enclosing_nested_data_list: Option<&mut Vec<NestedDataInfo<'a>>>,
    variants: &'a HashMap<u32, MojomWireType>,
) -> ParsingResult<(u32, Option<MojomValue>)> {
    // Parse the union header
    let size_in_bytes = parse_size(data)?;
    let tag = parse_u32(data)?;
    let field_ty = match variants.get(&tag) {
        Some(wire_ty) => wire_ty,
        None => return Err(ParsingError::invalid_discriminant(data.bytes_parsed() - 4, tag)),
    };

    let in_enclosing_body = enclosing_nested_data_list.is_some();

    // A union is structured data with a single, variable field.
    // Make up a dummy field name for debugging.
    static UNION_FIELD_NAME: std::sync::LazyLock<String> =
        std::sync::LazyLock::new(|| "Union_Field".to_string());
    let mut parsed_fields = parse_structured_body(
        data,
        enclosing_nested_data_list,
        size_in_bytes,
        std::slice::from_ref(&*UNION_FIELD_NAME),
        std::slice::from_ref(field_ty),
    )?
    .1;
    assert_eq!(parsed_fields.len(), 1);

    let ret = match (in_enclosing_body, field_ty) {
        // If the union contained a pointer and we were in an enclosing body,
        // then it appended nested data info instead of constructing a union value
        (true, MojomWireType::Pointer { .. }) => None,
        (_, MojomWireType::Union { .. }) => {
            // Sanity check because unions are really complicated
            unreachable!("Unions should never be packed directly in other unions")
        }
        // If the union contained a non-pointer value, or it was standalone,
        // then the union value has already been fully constructed.
        _ => Some(MojomValue::Union(tag, Box::new(parsed_fields.pop().unwrap()))),
    };

    Ok((tag, ret))
}

/// Parse the body of a struct, array, or union, having already consumed its
/// header to figure out its expected size and what fields it has.
///
/// The enclosing_nested_data_list argument is only used for a special case
/// involving unions. See the documentation of parse_union for details.
fn parse_structured_body<'a>(
    data: &mut ParserData,
    enclosing_nested_data_list: Option<&mut Vec<NestedDataInfo<'a>>>,
    expected_size_in_bytes: usize,
    field_names: &'a [String],
    fields: &'a [MojomWireType],
) -> ParsingResult<(Vec<String>, Vec<MojomValue>)> {
    // Start counting from the beginning of the header which we already parsed
    let initial_bytes_parsed = data.bytes_parsed() - 8;

    let mut local_nested_data_list: Vec<NestedDataInfo> = vec![];
    let nested_data_list = match enclosing_nested_data_list {
        Some(list) => list,
        None => &mut local_nested_data_list,
    };

    // Pre-allocate space for the parsed values, so we can write directly into them
    // by index. We have to provide dummy values since rust won't allow
    // uninitialized memory.
    let num_fields = get_max_ordinal(fields).map_or(0, |o| o + 1);
    let mut ret_names: Vec<String> = vec![String::new(); num_fields];
    let mut ret_values: Vec<MojomValue> = vec![MojomValue::Int8(0); num_fields];

    for (index, mojom_wire_type) in fields.iter().enumerate() {
        // Make sure we're at the right alignment for this field
        skip_to_alignment(data, mojom_wire_type.alignment())?;

        let name = field_names
            .get(index)
            .expect("parse_structured_body: field_names should have the same length as fields");

        match mojom_wire_type {
            // Nested structured data, record for later
            MojomWireType::Pointer { ordinal, nested_data_type } => {
                // We don't currently support null pointers
                let pointer_value = parse_pointer(data, false)?;
                let nested_info = NestedDataInfo {
                    ty: nested_data_type,
                    ordinal: *ordinal,
                    field_name: name.clone(),
                    expected_offset: data.bytes_parsed() - initial_bytes_parsed
                        - 8 // Don't count the bytes we just parsed
                        + pointer_value,
                    // We'll fill this in later if necessary (in the Union branch below)
                    union_discriminant: None,
                };
                nested_data_list.push(nested_info);
            }
            // Nested leaf data, just parse it
            MojomWireType::Leaf { ordinal, leaf_type } => {
                let parsed_value = parse_leaf_element(data, leaf_type)?;
                ret_names[*ordinal] = name.clone();
                ret_values[*ordinal] = parsed_value;
            }
            MojomWireType::Bitfield { ordinals } => {
                let mut iter = ordinals.into_iter().enumerate();
                let parsed_bits = parse_u8(data)?;
                while let Some((idx, Some(ordinal))) = iter.next() {
                    let bit = (parsed_bits >> idx) & 1;
                    ret_names[*ordinal] = name.clone();
                    ret_values[*ordinal] = MojomValue::Bool(bit == 1);
                }
            }
            MojomWireType::Union { ordinal, variants } => {
                let bytes_parsed_at_union_start = data.bytes_parsed() - initial_bytes_parsed;
                match parse_union(data, Some(nested_data_list), variants)? {
                    // If we have a complete value, we can just store it as usual.
                    (_, Some(parsed_value)) => {
                        ret_names[*ordinal] = name.clone();
                        ret_values[*ordinal] = parsed_value;
                    }
                    // Otherwise, the union contained a pointer, and its info was appended to
                    // nested_data_list. We need to adjust the appended info so that it's
                    // relative to the enclosing struct, instead of this union.
                    (tag, None) => {
                        // We just pushed an entry, so we know it exists.
                        // We know no elements are later than it because unions cannot nest
                        // directly in other unions, so we'll never recurse more than once.
                        let nested_data_info = nested_data_list.last_mut().unwrap();
                        // This was previously None
                        nested_data_info.union_discriminant = Some(tag);
                        // This was previously the ordinal in the _union_ (i.e. 0)
                        nested_data_info.ordinal = *ordinal;
                        // This was previously the distance from the start of the _union_ body
                        // We need it to be the distance from the start of _this_ body
                        nested_data_info.expected_offset += bytes_parsed_at_union_start;
                    }
                }
            }
        };
    }

    // We've reached the end of the struct (not including nested data!)
    // Make sure we parsed the expected number of bytes.
    let bytes_parsed_so_far = data.bytes_parsed() - initial_bytes_parsed;
    if bytes_parsed_so_far > expected_size_in_bytes {
        return Err(ParsingError::wrong_size(
            data.bytes_parsed(),
            expected_size_in_bytes,
            bytes_parsed_so_far,
        ));
    } else if bytes_parsed_so_far < expected_size_in_bytes {
        parse_padding(data, expected_size_in_bytes - bytes_parsed_so_far)?
    }

    // At this point, we'll only parse nested things if we didn't have an eclosing
    // nested data list; if we did, then the nested things will be at the end of the
    // enclosing object instead.
    for nested_data in local_nested_data_list {
        // Nested data is required to appear in the same order as the (packed) fields
        // of the struct. So the expected offset is only useful for validation.
        let bytes_parsed_so_far = data.bytes_parsed() - initial_bytes_parsed;
        if nested_data.expected_offset != bytes_parsed_so_far {
            return Err(ParsingError::wrong_pointer(
                data.bytes_parsed(),
                nested_data.field_name,
                nested_data.expected_offset,
                bytes_parsed_so_far,
            ));
        }
        let mut parsed_data = match nested_data.ty {
            PackedStructuredType::Struct { packed_field_names, packed_field_types } => {
                parse_struct(data, packed_field_names, packed_field_types)?
            }
            PackedStructuredType::Array { .. } => {
                panic!("Arrays are not yet implemented");
            }
            PackedStructuredType::Union { variants } => parse_union(data, None, variants)?
                .1
                .expect("Parsing nested union should always return a value"),
        };
        if let Some(tag) = nested_data.union_discriminant {
            parsed_data = MojomValue::Union(tag, Box::new(parsed_data))
        };
        ret_names[nested_data.ordinal] = nested_data.field_name;
        ret_values[nested_data.ordinal] = parsed_data;
    }
    Ok((ret_names, ret_values))
}

/// Parse a single mojom value of the given type, outside the context of a
/// struct. This function is only useful for unit testing, since all mojom
/// values in practice are members of a struct. The function only works for
/// some mojom types, since e.g. booleans can't be parsed individually.
pub fn parse_single_value_for_testing(
    data: &[u8],
    wire_type: &MojomWireType,
) -> ParsingResult<MojomValue> {
    let mut data = ParserData::new(data);
    match wire_type {
        MojomWireType::Leaf { leaf_type, .. } => parse_leaf_element(&mut data, leaf_type),
        MojomWireType::Bitfield { .. } => unimplemented!("Bitfields cannot be parsed individually"),
        MojomWireType::Pointer { nested_data_type, .. } => match nested_data_type {
            PackedStructuredType::Struct { packed_field_names, packed_field_types } => {
                parse_struct(&mut data, packed_field_names, packed_field_types)
            }
            PackedStructuredType::Array { .. } => {
                panic!("Arrays are not yet implemented");
            }
            PackedStructuredType::Union { .. } => {
                panic!("Standalone unions are never behind pointers")
            }
        },
        MojomWireType::Union { variants, .. } => Ok(parse_union(&mut data, None, variants)?
            .1
            .expect("Parsing standalone union should always return a value")),
    }
}
