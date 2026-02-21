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
use std::collections::BTreeMap;
use std::sync::Arc;

/// Parse a type without nested data, i.e. anything but a struct or array
fn parse_leaf_element(
    data: &mut ParserData,
    ty: &PackedLeafType,
    is_nullable: bool,
) -> ParsingResult<MojomValue> {
    match ty {
        PackedLeafType::Bool => Ok(MojomValue::Bool(parse_u8(data)? == 1)),
        PackedLeafType::UInt8 => Ok(MojomValue::UInt8(parse_u8(data)?)),
        PackedLeafType::UInt16 => Ok(MojomValue::UInt16(parse_u16(data)?)),
        PackedLeafType::UInt32 => Ok(MojomValue::UInt32(parse_u32(data)?)),
        PackedLeafType::UInt64 => Ok(MojomValue::UInt64(parse_u64(data)?)),
        PackedLeafType::Int8 => Ok(MojomValue::Int8(parse_i8(data)?)),
        PackedLeafType::Int16 => Ok(MojomValue::Int16(parse_i16(data)?)),
        PackedLeafType::Int32 => Ok(MojomValue::Int32(parse_i32(data)?)),
        PackedLeafType::Int64 => Ok(MojomValue::Int64(parse_i64(data)?)),
        PackedLeafType::Float32 => Ok(MojomValue::Float32(parse_f32(data)?.into())),
        PackedLeafType::Float64 => Ok(MojomValue::Float64(parse_f64(data)?.into())),
        PackedLeafType::Enum { is_valid } => {
            let value = parse_u32(data)?;
            if is_valid.call(value) {
                Ok(MojomValue::Enum(value))
            } else {
                // Report the error starting before the 32 bits we just parsed
                Err(ParsingError::invalid_discriminant(data.bytes_parsed() - 4, value))
            }
        }
        PackedLeafType::Handle => {
            // On the wire, handles are represented as a 32-bit index into the
            // message's attached handle array, which is part of `data`.
            let idx_u32 = parse_u32(data)?;
            let idx: usize = idx_u32.try_into().unwrap();

            // This value indicates the handle is `None`.
            if idx_u32 == 0xffffffff {
                if is_nullable {
                    return Ok(MojomValue::Nullable(None));
                } else {
                    return Err(ParsingError::invalid_handle_index(data.bytes_parsed() - 4, idx));
                }
            };

            let handle = data
                .take_handle(idx)
                .ok_or_else(|| ParsingError::invalid_handle_index(data.bytes_parsed() - 4, idx))?;
            let handle_val = MojomValue::Handle(handle);

            if is_nullable {
                return Ok(MojomValue::Nullable(Some(Box::new(handle_val))));
            } else {
                return Ok(handle_val);
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

/// Check if the parsed keys for a map have duplicates, and error out if so.
///
/// This function promises not to mutate `keys` if it returns `Ok()`.
/// FOR_RELEASE: Get itertools approved and use that instead.
fn check_for_duplicate_keys(offset: usize, keys: &mut [MojomValue]) -> ParsingResult<()> {
    // Check by inserting the keys into a hashset.
    // insert returns false if the value was already present.
    // Note that inserting references still compares the underlying values.
    let mut unique_keys = std::collections::HashSet::new();
    let mut dup_idx = 0;
    let dup_exists = keys.iter().enumerate().any(|(idx, item)| {
        if !unique_keys.insert(item) {
            dup_idx = idx;
            true
        } else {
            false
        }
    });
    if dup_exists {
        Err(ParsingError::duplicate_map_key(offset, std::mem::take(&mut keys[dup_idx])))
    } else {
        Ok(())
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
    /// its discriminant was, and whether the outer union was nullable.
    union_discriminant: Option<(u32, bool)>,
    /// Tracks whether the pointer was nullable (if so, we should wrap the
    /// parsed result in an option.)
    was_nullable: bool,
}

/// Parse a 32-bit size as part of a struct or array header
/// FOR_RELEASE: Maybe make a (slightly) more general parse_header function
/// which parses this and the following 4 bytes as well.
pub fn parse_size(
    data: &mut ParserData,
    is_array: bool,
    may_be_null: bool,
) -> ParsingResult<usize> {
    let parsed_value = parse_u32(data)?;
    let mk_err = || Err(ParsingError::invalid_size(data.bytes_parsed() - 4, parsed_value));
    let size = parsed_value.try_into().or_else(|_| mk_err())?;
    // Non-array sizes are always divisible by 8.
    let invalid_remainder = !is_array && size % 8 != 0;
    let too_small = (size == 0 && !may_be_null) || (0 < size && size < 8);
    if too_small || invalid_remainder {
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
    fields: &[StructuredBodyElementOwned],
    num_elements_in_value: usize,
) -> ParsingResult<MojomValue> {
    // Parse the struct header
    let size_in_bytes = parse_size(data, false, false)?;
    let version_number = parse_u32(data)?;

    // We're ignoring versioning for now
    if version_number != 0 {
        return Err(ParsingError::not_implemented(
            data.bytes_parsed() - 4,
            "Versioning".to_string(),
        ));
    }

    let (parsed_names, parsed_fields) = parse_structured_body(
        data,
        None,
        size_in_bytes,
        field_names,
        fields.iter().map(StructuredBodyElementOwned::as_ref),
        num_elements_in_value,
    )?;

    Ok(MojomValue::Struct(parsed_names, parsed_fields))
}

fn parse_array(
    data: &mut ParserData,
    element_type: &Arc<MojomWireType>,
    array_type: &PackedArrayType,
) -> ParsingResult<MojomValue> {
    // Parse the array header
    let size_in_bytes = parse_size(data, true, false)?;
    // usize always fits into 32 bits on our platforms
    let num_elements = parse_u32(data)?.try_into().unwrap();

    if let PackedArrayType::SizedArray(expected_num_elements) = array_type
        && *expected_num_elements != num_elements
    {
        return Err(ParsingError::wrong_array_size(
            // Report the error as originating from the 4 bytes we just parsed
            data.bytes_parsed() - 4,
            *expected_num_elements,
            num_elements,
        ));
    }

    // If this array represents a string, we don't need to do fancy parsing
    // of the body, just grab the bytes and call it a day.
    if *array_type == PackedArrayType::String {
        return parse_string(data, size_in_bytes, num_elements);
    }

    // Make up dummy field names for debugging.
    let num_tag_bitfields = if element_type.is_nullable_primitive() {
        // Nullable primitives need some bitfields at the beginning to hold
        // the tag bits; one bitfield for every 8 elements.
        num_elements.div_ceil(8) // Divide by 8, rounding up
    } else {
        0
    };
    let tag_names = (0..num_tag_bitfields).map(|idx| format!("Array_Tags_{idx}"));
    let elt_names = (0..num_elements).map(|idx| format!("Array_Element_{idx}"));
    let field_names = tag_names.chain(elt_names).collect::<Vec<_>>();
    // An array body is equivalent to a struct body with `num_elements` copies of
    // its field
    let array_body = crate::pack::pack_array_body(element_type, num_elements);
    let (_names, parsed_fields) = parse_structured_body(
        data,
        None,
        size_in_bytes,
        &field_names,
        array_body.into_iter(),
        num_elements,
    )?;

    Ok(MojomValue::Array(parsed_fields))
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
    mut enclosing_nested_data_list: Option<&mut Vec<NestedDataInfo<'a>>>,
    variants: &'a BTreeMap<u32, MojomWireType>,
    is_nullable: bool,
) -> ParsingResult<(u32, Option<MojomValue>)> {
    // Parse the union header
    let size_in_bytes = parse_size(data, false, is_nullable)?;

    // The only way for the size to be 0 is if the union is (validly) null.
    // In that case we've nothing else to do here.
    if size_in_bytes == 0 {
        // Skip the remaining bytes in the value
        parse_padding(data, 12)?;
        return Ok((0, Some(MojomValue::Nullable(None))));
    }

    let tag = parse_u32(data)?;

    let field_ty = match variants.get(&tag) {
        Some(wire_ty) => wire_ty,
        None => return Err(ParsingError::invalid_discriminant(data.bytes_parsed() - 4, tag)),
    };

    let enclosing_nested_data_len = enclosing_nested_data_list.as_deref().map(Vec::len);

    // A union is structured data with a single, variable field.
    // Make up a dummy field name for debugging.
    let field_name = format!("Union_Field_{tag}");
    let struct_ref_element: StructuredBodyElementMixed =
        StructuredBodyElement::SingleValue(0, field_ty);

    let (_names, mut parsed_fields) = parse_structured_body(
        data,
        enclosing_nested_data_list.as_deref_mut(),
        size_in_bytes,
        std::slice::from_ref(&field_name),
        std::iter::once(struct_ref_element),
        1,
    )?;
    assert_eq!(parsed_fields.len(), 1);

    // If we were in an enclosing body, and we pushed something to the nested
    // data list, then we haven't actually parsed the union's contents yet, so
    // we can't return anything useful (parsed_fields contains a dummy value).
    let ret = if let Some(old_len) = enclosing_nested_data_len
        && let new_len = enclosing_nested_data_list.unwrap().len()
        && new_len > old_len
    {
        None
    } else {
        Some(MojomValue::Union(tag, Box::new(parsed_fields.pop().unwrap())))
    };

    let ret = ret.map(|parsed_value| {
        if is_nullable {
            MojomValue::Nullable(Some(Box::new(parsed_value)))
        } else {
            parsed_value
        }
    });

    Ok((tag, ret))
}

fn parse_map(
    data: &mut ParserData,
    key_type: &Arc<MojomWireType>,
    value_type: &Arc<MojomWireType>,
) -> ParsingResult<MojomValue> {
    let initial_bytes_parsed = data.bytes_parsed();
    // Maps are encoded as a struct containing a pair of arrays, one for
    // the keys and one for the corresponding values.
    let field_names = ["map_keys".to_string(), "map_values".to_string()];
    // FOR_RELEASE: This code is duplicated in deparse_values, maybe abstract
    // it out.
    let fields = [
        StructuredBodyElement::SingleValue(
            0,
            MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Array {
                    // This clone is cheap because it's in an Arc
                    element_type: key_type.clone(),
                    array_type: PackedArrayType::UnsizedArray,
                },
                is_nullable: false,
            },
        ),
        StructuredBodyElement::SingleValue(
            1,
            MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Array {
                    // This clone is cheap because it's in an Arc
                    element_type: value_type.clone(),
                    array_type: PackedArrayType::UnsizedArray,
                },
                is_nullable: false,
            },
        ),
    ];
    let parsed_arrays = parse_struct(data, &field_names, &fields, 2)?;
    let parsed_fields = match parsed_arrays {
        MojomValue::Struct(_, fields) => fields,
        _ => panic!(
            "Tried to parse a map like a struct, but got a non-struct MojomValue: {:?}",
            parsed_arrays
        ),
    };
    let [keys, values] = parsed_fields.try_into().unwrap();
    match (keys, values) {
        (MojomValue::Array(mut keys), MojomValue::Array(values)) => {
            if keys.len() != values.len() {
                return Err(ParsingError::mismatched_map(
                    initial_bytes_parsed,
                    keys.len(),
                    values.len(),
                ));
            }
            // Map bodies are 24 bytes, and the key array immediately follows them.
            let key_offset = initial_bytes_parsed + 24;
            check_for_duplicate_keys(key_offset, &mut keys)?;
            let map_val = keys.into_iter().zip(values).collect();
            Ok(MojomValue::Map(map_val))
        }
        elts => {
            panic!("Tried to parse a map like a struct, but got non-array elements: {:?} ", elts)
        }
    }
}

/// Parse a string embedded in the mojom message. We don't do any validation of
/// the bytes (since mojom doesn't provide any guarantees), so this is as
/// simple as grabbing the bytes and ensuring the header was correct.
fn parse_string(
    data: &mut ParserData,
    size_in_bytes: usize,
    num_elements: usize,
) -> ParsingResult<MojomValue> {
    // Array header size should be the size of the header (8) + the number of bytes
    if size_in_bytes != num_elements + 8 {
        return Err(ParsingError::wrong_size(data.bytes_parsed(), size_in_bytes, num_elements + 8));
    }
    let string_bytes = parse_raw_bytes(data, num_elements)?.to_vec();
    let rust_string = String::from_utf8(string_bytes)
        .map_err(|err| ParsingError::non_utf8_string(data.bytes_parsed(), err))?;
    // Array bodies always end at 8 byte alignment, though it's not
    // reflected in the header's reported size.
    skip_to_alignment(data, 8)?;
    Ok(MojomValue::String(rust_string))
}

/// Create a MojomValue::Nullable out of the given value, if appropriate.
///
/// Our parser handles nullable primitives as follows: first, we read the tag
/// bit, which the packing algorithm guarantees will appear before the primitive
/// value. We treat this like any other value, and so we will write either
/// Bool(true) or Bool(false) into the appropriate ordinals.
///
/// When we reach the primitive value itself, we then check whether the current
/// value is a Bool, or whether it's the designated dummy value. In the former
/// case, we assume that we're a nullable, and wrap the given value. Otherwise
/// we assume we're _not_ nullable, and return the value unchanged.
fn wrap_nullable_primitive(
    ret_values: &[MojomValue],
    ordinal: Ordinal,
    parsed_value: MojomValue,
) -> MojomValue {
    match ret_values[ordinal] {
        MojomValue::Bool(false) => MojomValue::Nullable(None),
        MojomValue::Bool(true) => MojomValue::Nullable(Some(Box::new(parsed_value))),
        MojomValue::Invalid => parsed_value,
        _ => panic!("We tried to overwrite an already-parsed value!"),
    }
}

/// Parse the body of a struct, array, or union, having already consumed its
/// header to figure out its expected size and what fields it has.
///
/// The enclosing_nested_data_list argument is only used for a special case
/// involving unions. See the documentation of parse_union for details.
///
/// FOR_RELEASE: This function has a lot of arguments, document them more
/// explicitly. Also maybe explain higher up the general parsing strategy
/// (all structured data calls this one way or another)
fn parse_structured_body<'a, 'b, IterT, BitfieldT>(
    data: &mut ParserData,
    enclosing_nested_data_list: Option<&mut Vec<NestedDataInfo<'a>>>,
    expected_size_in_bytes: usize,
    // FOR_RELEASE: See if we can put names into the iterator too
    field_names: &[String],
    fields: IterT,
    num_elements_in_value: usize,
) -> ParsingResult<(Vec<String>, Vec<MojomValue>)>
where
    BitfieldT: std::borrow::Borrow<BitfieldOrdinals>,
    IterT: Iterator<Item = StructuredBodyElementRef<'a, BitfieldT>>,
{
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
    let mut ret_names: Vec<String> = vec![String::new(); num_elements_in_value];
    let mut ret_values: Vec<MojomValue> =
        (0..num_elements_in_value).map(|_| MojomValue::Invalid).collect();

    for (index, struct_ref_element) in fields.enumerate() {
        // Make sure we're at the right alignment for this field
        skip_to_alignment(data, struct_ref_element.alignment())?;

        // FOR_RELEASE: It would be nice to use zip_eq from itertools instead of
        // pulling out the name by index, if itertools gets approved for chromium
        let name = field_names
            .get(index)
            .expect("parse_structured_body: field_names should have the same length as fields");

        match struct_ref_element {
            StructuredBodyElement::Bitfield(ordinals) => {
                let mut iter = ordinals.borrow().iter().enumerate();
                let parsed_bits = parse_u8(data)?;
                while let Some((idx, Some((ordinal, _)))) = iter.next() {
                    let bit = (parsed_bits >> idx) & 1;
                    let parsed_val = MojomValue::Bool(bit == 1);
                    let parsed_val = wrap_nullable_primitive(&ret_values, *ordinal, parsed_val);
                    ret_names[*ordinal] = name.clone();
                    ret_values[*ordinal] = parsed_val;
                }
            }
            StructuredBodyElement::SingleValue(ordinal, mojom_wire_type) => {
                match mojom_wire_type {
                    // Nested structured data, record for later unless it was null.
                    MojomWireType::Pointer { nested_data_type, is_nullable } => {
                        let pointer_value = parse_pointer(data, *is_nullable)?;

                        // If the pointer was (validly) null, then there's no
                        // nested data to parse, we can just say None here.
                        if pointer_value == 0 {
                            ret_names[ordinal] = name.clone();
                            ret_values[ordinal] = MojomValue::Nullable(None);
                            continue;
                        }

                        let nested_info = NestedDataInfo {
                            ty: nested_data_type,
                            ordinal,
                            field_name: name.clone(),
                            expected_offset: data.bytes_parsed() - initial_bytes_parsed
                        - 8 // Don't count the bytes we just parsed
                        + pointer_value,
                            // If this pointer is contained in a union value,
                            // but points to the tail of the enclosing body,
                            // then we'll fill in this field when we finish
                            // parsing the union value.
                            union_discriminant: None,
                            was_nullable: *is_nullable,
                        };
                        nested_data_list.push(nested_info);
                    }
                    // Nested leaf data, just parse it
                    MojomWireType::Leaf { leaf_type, is_nullable } => {
                        let mut parsed_value = parse_leaf_element(data, leaf_type, *is_nullable)?;

                        // Handles have their own special nullability markers,
                        // which are checked in `parse_leaf_element`.
                        if leaf_type != &PackedLeafType::Handle {
                            parsed_value =
                                wrap_nullable_primitive(&ret_values, ordinal, parsed_value);
                        }
                        ret_names[ordinal] = name.clone();
                        ret_values[ordinal] = parsed_value;
                    }
                    MojomWireType::Union { variants, is_nullable } => {
                        let bytes_parsed_at_union_start =
                            data.bytes_parsed() - initial_bytes_parsed;
                        match parse_union(data, Some(nested_data_list), variants, *is_nullable)? {
                            // If we have a complete value, we can just store it as usual.
                            (_, Some(parsed_value)) => {
                                ret_names[ordinal] = name.clone();
                                ret_values[ordinal] = parsed_value;
                            }
                            // Otherwise, the union contained a pointer, and its info was appended
                            // to nested_data_list. We need to adjust the appended info so that it's
                            // relative to the enclosing struct, instead of this union.
                            (tag, None) => {
                                // We just pushed an entry, so we know it exists.
                                // We know no elements are later than it because unions cannot nest
                                // directly in other unions, so we'll never recurse more than once.
                                let nested_data_info = nested_data_list.last_mut().unwrap();
                                // This was previously None
                                nested_data_info.union_discriminant = Some((tag, *is_nullable));
                                // This was previously the ordinal in the _union_ (i.e. 0)
                                nested_data_info.ordinal = ordinal;
                                // This was previously the distance from the start of the _union_
                                // body. We need it to be the distance from the start of _this_ body
                                nested_data_info.expected_offset += bytes_parsed_at_union_start;
                            }
                        }
                    }
                }
            }
        }
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
        // All nested data is 8-byte aligned
        skip_to_alignment(data, 8)?;
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
            PackedStructuredType::Struct {
                packed_field_names,
                packed_field_types,
                num_elements_in_value,
            } => {
                parse_struct(data, packed_field_names, packed_field_types, *num_elements_in_value)?
            }
            PackedStructuredType::Array { element_type, array_type } => {
                parse_array(data, element_type, array_type)?
            }
            PackedStructuredType::Union { variants } => {
                // Unions in a structured body are never nullable
                // (the pointer would have been nullable instead)
                parse_union(data, None, variants, false)?
                    .1
                    .expect("Parsing nested union should always return a value")
            }
            PackedStructuredType::Map { key_type, value_type } => {
                parse_map(data, key_type, value_type)?
            }
        };
        // If necessary, wrap the parsed value based on the type it was contained in
        if nested_data.was_nullable {
            // The pointer to this data was nullable
            parsed_data = MojomValue::Nullable(Some(Box::new(parsed_data)));
        }
        if let Some((tag, _)) = nested_data.union_discriminant {
            // This data was contained inside a union
            parsed_data = MojomValue::Union(tag, Box::new(parsed_data));
        };
        if let Some((_, true)) = nested_data.union_discriminant {
            // This data was contained inside a _nullable_ union
            parsed_data = MojomValue::Nullable(Some(Box::new(parsed_data)));
        };
        ret_names[nested_data.ordinal] = nested_data.field_name;
        ret_values[nested_data.ordinal] = parsed_data;
    }

    // All structured bodies end at an 8-byte alignment
    skip_to_alignment(data, 8)?;

    Ok((ret_names, ret_values))
}

/// Parse a single mojom value of the given type, outside the context of a
/// struct. This function is only useful for unit testing, since all mojom
/// values in practice are members of a struct. The function only works for
/// some mojom types, since e.g. booleans can't be parsed individually.
pub fn parse_single_value_for_testing(
    data: &[u8],
    handles: &mut [Option<UntypedHandle>],
    wire_type: &MojomWireType,
) -> ParsingResult<MojomValue> {
    let mut data = ParserData::new(data, handles);
    match wire_type {
        MojomWireType::Leaf { leaf_type, is_nullable: false } => {
            parse_leaf_element(&mut data, leaf_type, false)
        }
        MojomWireType::Leaf { leaf_type: PackedLeafType::Handle, is_nullable } => {
            parse_leaf_element(&mut data, &PackedLeafType::Handle, *is_nullable)
        }
        MojomWireType::Pointer { nested_data_type, is_nullable: false } => match nested_data_type {
            PackedStructuredType::Struct {
                packed_field_names,
                packed_field_types,
                num_elements_in_value,
            } => parse_struct(
                &mut data,
                packed_field_names,
                packed_field_types,
                *num_elements_in_value,
            ),
            PackedStructuredType::Array { element_type, array_type } => {
                parse_array(&mut data, element_type, array_type)
            }
            PackedStructuredType::Union { .. } => {
                panic!("Standalone unions are never behind pointers")
            }
            PackedStructuredType::Map { key_type, value_type } => {
                parse_map(&mut data, key_type, value_type)
            }
        },
        MojomWireType::Union { variants, is_nullable } => {
            Ok(parse_union(&mut data, None, variants, *is_nullable)?
                .1
                .expect("Parsing standalone union should always return a value"))
        }
        _ => panic!("Invalid argument to parse_single_value_for_testing: {wire_type:?}"),
    }
}

/// Deserialize a single value from the given bytes, and return the remaining
/// unparsed bytes.
pub fn parse_top_level_value<'a>(
    data_slice: &'a [u8],
    handles: &'a mut [Option<UntypedHandle>],
    ty: &MojomWireType,
) -> ParsingResult<(&'a [u8], MojomValue)> {
    let mut data = ParserData::new(data_slice, handles);
    match ty {
        MojomWireType::Pointer {
            nested_data_type:
                PackedStructuredType::Struct {
                    packed_field_names,
                    packed_field_types,
                    num_elements_in_value,
                },
            ..
        } => {
            return crate::parse_values::parse_struct(
                &mut data,
                packed_field_names,
                packed_field_types,
                *num_elements_in_value,
            )
            .map(|ret| (data.into_bytes(), ret));
        }
        _ => panic!("All message bodies are structs"),
    };
}
