// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ast::*;

use anyhow::{bail, Context, Result};
use std::collections::BTreeMap;
use std::sync::Arc;

/// Wrapper type for the output of the deparser
pub struct DeparsedData {
    bytes: Vec<u8>,
    handles: Vec<UntypedHandle>,
}

impl Default for DeparsedData {
    fn default() -> Self {
        Self::new()
    }
}

impl DeparsedData {
    pub fn new() -> Self {
        DeparsedData { bytes: vec![], handles: vec![] }
    }

    pub fn into_parts(self) -> (Vec<u8>, Vec<UntypedHandle>) {
        (self.bytes, self.handles)
    }
}

// Convenient implementations: In almost all circumstances we only care about
// the `bytes` field, so make that easily available (via the . operator).
// The fields can also be accessed directly, of course.
impl std::ops::Deref for DeparsedData {
    type Target = Vec<u8>;

    fn deref(&self) -> &Self::Target {
        &self.bytes
    }
}

impl std::ops::DerefMut for DeparsedData {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.bytes
    }
}

/// Return a reference to the field at index `ordinal`
fn get_field_at_ordinal(field_values: &mut [MojomValue], ordinal: Ordinal) -> Result<&MojomValue> {
    field_values.get(ordinal).with_context(|| {
        anyhow::anyhow!(
            "Wire type asked for field with ordinal {}, but there are only {} fields.",
            ordinal,
            field_values.len()
        )
    })
}

/// Take ownership of the field at index `ordinal` in the vector, replacing it
/// with an invalid value. Fails if the value was already taken.
fn take_field_at_ordinal(field_values: &mut [MojomValue], ordinal: Ordinal) -> Result<MojomValue> {
    let Some(field_ref) = field_values.get_mut(ordinal) else {
        anyhow::bail!(
            "Wire type asked for field with ordinal {}, but there are only {} fields.",
            ordinal,
            field_values.len()
        )
    };
    let field_value = std::mem::take(field_ref);
    if field_value == MojomValue::Invalid {
        anyhow::bail!(
            "Wire type tried to retrieve the field with with ordinal {} multiple times!",
            ordinal,
        )
    };
    Ok(field_value)
}

/// Abstract over possibly-nullable MojomValues.
///
/// This function takes a MojomValue which _might_ be nullable, and returns an
/// option:
/// - If `value` is not nullable, it returns `Some(value)`.
/// - If `value` is Nullable(Some(inner_value)), it returns `Some(inner_value)`.
/// - If `value` is None, it returns `None`.
///
/// If the value is not of the expected nullability, it returns an error.
///
/// This function allows other code to not care about whether the value was
/// originally a nullable or not, since `value` and `Nullable(Some(value))`
/// map to the same thing.
///
/// Note that:
/// - If `None` is returned, it means the original value was _validly_ `None`.
/// - If `Some(value)` is returned, then `value` should not be a
///   `MojomValue::Nullable`, because Mojom does not permit nested nullable
///   types.
fn flatten_possibly_nullable_value(
    maybe_nullable: MojomValue,
    expect_nullable: bool,
) -> Result<Option<MojomValue>> {
    match maybe_nullable {
        MojomValue::Nullable(_) if !expect_nullable => {
            bail!("Got nullable value for non-nullable type")
        }
        MojomValue::Nullable(None) => Ok(None),
        MojomValue::Nullable(Some(inner_value)) => Ok(Some(*inner_value)),
        _ if expect_nullable => bail!("Got non-nullable value for nullable type"),
        _ => Ok(Some(maybe_nullable)),
    }
}

/// If the contained value is (validly) null, then write num_bytes empty bytes
/// to the data vector; otherwise, return the contained value.
///
/// This is a macro instead of a function so we can `continue` inside it.
macro_rules! write_or_extract_nullable {
    ($data:expr, $val:expr, $is_nullable:expr, $num_bytes:expr, $none_indicator_byte:expr) => {
        match flatten_possibly_nullable_value($val, $is_nullable)? {
            None => {
                // For a null value, we need only write a bunch of 0s
                $data.extend(vec![$none_indicator_byte; $num_bytes]);
                continue;
            }
            Some(v) => v,
        }
    };
    ($data:expr, $val:expr, $is_nullable:expr, $num_bytes:expr) => {
        write_or_extract_nullable!($data, $val, $is_nullable, $num_bytes, 0x00)
    };
}

// Standard error message for getting a value that doesn't match its type
macro_rules! wrong_type {
    ($expected_type:expr, $value:expr) => {
        bail!("Expected to find type {:?}, but got value {:?}", $expected_type, $value)
    };
}

fn pad_to_alignment(data: &mut DeparsedData, alignment: usize) {
    let mismatch = data.len() % alignment;
    if mismatch != 0 {
        data.extend(vec![0; alignment - mismatch])
    }
}

/// Write out the bytes for a leaf node
fn deparse_leaf_value(
    data: &mut DeparsedData,
    value: MojomValue,
    leaf_type: &PackedLeafType,
) -> Result<()> {
    match (value, leaf_type) {
        (MojomValue::Bool(value), PackedLeafType::Bool) => {
            data.extend(u8::from(value).to_le_bytes())
        }
        (MojomValue::Int8(value), PackedLeafType::Int8) => data.extend(value.to_le_bytes()),
        (MojomValue::UInt8(value), PackedLeafType::UInt8) => data.extend(value.to_le_bytes()),
        (MojomValue::Int16(value), PackedLeafType::Int16) => data.extend(value.to_le_bytes()),
        (MojomValue::UInt16(value), PackedLeafType::UInt16) => data.extend(value.to_le_bytes()),
        (MojomValue::Int32(value), PackedLeafType::Int32) => data.extend(value.to_le_bytes()),
        (MojomValue::UInt32(value), PackedLeafType::UInt32) => data.extend(value.to_le_bytes()),
        (MojomValue::Int64(value), PackedLeafType::Int64) => data.extend(value.to_le_bytes()),
        (MojomValue::UInt64(value), PackedLeafType::UInt64) => data.extend(value.to_le_bytes()),
        (MojomValue::Float32(value), PackedLeafType::Float32) => data.extend(value.to_le_bytes()),
        (MojomValue::Float64(value), PackedLeafType::Float64) => data.extend(value.to_le_bytes()),
        (MojomValue::Enum(value), PackedLeafType::Enum { .. }) => data.extend(value.to_le_bytes()),
        (MojomValue::Handle(handle), PackedLeafType::Handle) => {
            // Handles are represented on the wire as a 32-bit index into the
            // attached handles array. So instead of writing the value directly
            // to the wire, push it to the array and write its index instead.
            let handle_idx = u32::try_from(data.handles.len()).unwrap();
            data.handles.push(handle);
            data.extend(handle_idx.to_le_bytes())
        }
        (value, _) => wrong_type!(leaf_type, value),
    }
    Ok(())
}

// Mojom structs and arrays are never nested inside each other. Instead, they
// are represented by a pointer with an offset to the actual data which appears
// later in the message.

enum NestedData<'a> {
    Struct {
        field_values: Vec<MojomValue>,
        packed_fields: &'a [StructuredBodyElementOwned],
    },
    Array {
        contents: MojomValue,
        element_type: &'a Arc<MojomWireType>,
        array_type: &'a PackedArrayType,
    },
    Union {
        tag: u32,
        value: MojomValue,
        variants: &'a BTreeMap<u32, MojomWireType>,
    },
    Map {
        values_map: BTreeMap<MojomValue, MojomValue>,
        key_type: &'a Arc<MojomWireType>,
        value_type: &'a Arc<MojomWireType>,
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
fn write_to_slice(data: &mut [u8], start: usize, len: usize, value: &[u8]) {
    let struct_size_field: &mut [u8] = &mut data[start..start + len];
    struct_size_field.copy_from_slice(&value[0..len]);
}

pub fn deparse_struct(
    data: &mut DeparsedData,
    field_values: Vec<MojomValue>,
    packed_fields: &[StructuredBodyElementOwned],
) -> Result<()> {
    // Write the struct's header
    data.extend([0; 4]); // Size; we'll fill this in later
    data.extend([0; 4]); // Version number; not supported yet
    deparse_structured_body(
        data,
        None,
        false,
        field_values,
        packed_fields.iter().map(StructuredBodyElementOwned::as_ref),
    )
}

fn deparse_array(
    data: &mut DeparsedData,
    contents: MojomValue,
    element_type: &Arc<MojomWireType>,
    array_type: &PackedArrayType,
) -> Result<()> {
    let element_values = match (array_type, contents) {
        (PackedArrayType::String, MojomValue::String(string)) => {
            return deparse_string(data, string);
        }
        (
            PackedArrayType::SizedArray(_) | PackedArrayType::UnsizedArray,
            MojomValue::Array(element_values),
        ) => element_values,
        (array_type, contents) => {
            bail!("deparse_array: Got mismatched type and value: {array_type:?} vs. {contents:?}")
        }
    };

    let num_elements = element_values.len();

    if let PackedArrayType::SizedArray(expected_num_elements) = array_type
        && *expected_num_elements != num_elements
    {
        bail!(
            "Array type expected {expected_num_elements} elements but had {num_elements} elements."
        )
    };

    data.extend([0; 4]); // Size field of the header; we'll fill this in later

    // We don't support more than 2^32 elements.
    let num_elements_u32: u32 = num_elements.try_into().unwrap();
    data.extend(num_elements_u32.to_le_bytes());

    let array_body = crate::pack::pack_array_body(element_type, num_elements);

    deparse_structured_body(data, None, true, element_values, array_body.into_iter())
}

/// Serialize a union to the wire.
///
/// See the documentation of parse_union in parse_values.rs for an explanation
/// of the `enclosing_nested_data_list` argument
fn deparse_union<'a>(
    data: &mut DeparsedData,
    enclosing_nested_data_list: Option<&mut Vec<NestedDataInfo<'a>>>,
    tag: u32,
    contained_value: MojomValue,
    variants: &'a BTreeMap<u32, MojomWireType>,
) -> Result<()> {
    // Write the union's header
    let expected_wire_type = variants
        .get(&tag)
        .context(format!("Tag value {tag} is not a valid discriminant for this union!"))?;

    data.extend([0; 4]); // Size; we'll fill this in later
    data.extend(tag.to_le_bytes()); // Union tag

    let struct_ref_element: StructuredBodyElementMixed =
        StructuredBodyElement::SingleValue(0, expected_wire_type);

    deparse_structured_body(
        data,
        enclosing_nested_data_list,
        false,
        vec![contained_value],
        std::iter::once(struct_ref_element),
    )
}

fn deparse_map(
    data: &mut DeparsedData,
    values_map: BTreeMap<MojomValue, MojomValue>,
    key_type: &Arc<MojomWireType>,
    value_type: &Arc<MojomWireType>,
) -> Result<()> {
    let (keys, values) = values_map.into_iter().unzip();
    let field_values = vec![MojomValue::Array(keys), MojomValue::Array(values)];
    let packed_fields = [
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
    deparse_struct(data, field_values, &packed_fields)
}

fn deparse_string(data: &mut DeparsedData, value: String) -> Result<()> {
    let bytes = value.as_bytes();
    let num_bytes: u32 = bytes
        .len()
        .try_into()
        .with_context(|| "Mojom cannot serialize strings of more than 2^32 bytes")?;
    // Write header size (8 + num elements) and number of elements
    data.extend(u32::to_le_bytes(8 + num_bytes));
    data.extend(u32::to_le_bytes(num_bytes));

    // Write the actual string, then pad to 8 byte alignment
    data.extend(bytes);
    pad_to_alignment(data, 8);
    Ok(())
}

/// Deparse the fields of a struct (or union) after having parsed its header
///
/// See the documentation of parse_union in parse_values.rs for an explanation
/// of the `enclosing_nested_data_list` argument
///
/// The is_array argument controls whether we should write padding at the end
/// of the body before or after we record the size of the body in its header.
/// For arrays, the padding is not included in the header; for structs, it is.
// FOR_RELEASE: Try to take the value by value instead of by reference
fn deparse_structured_body<'a, 'b, IterT, BitfieldT>(
    data: &mut DeparsedData,
    enclosing_nested_data_list: Option<&mut Vec<NestedDataInfo<'a>>>,
    is_array: bool,
    mut field_values: Vec<MojomValue>,
    packed_fields: IterT,
) -> Result<()>
where
    BitfieldT: std::fmt::Debug + std::borrow::Borrow<BitfieldOrdinals>,
    IterT: Iterator<Item = StructuredBodyElementRef<'a, BitfieldT>>,
{
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
            StructuredBodyElement::Bitfield(ref ordinals) => {
                let mut iter = ordinals.borrow().iter().enumerate();
                let mut bitfield: u8 = 0;
                // Construct the bitfield bit-by-bit
                while let Some((idx, Some((ordinal, is_tag_bit)))) = iter.next() {
                    let bit_mojom_value = get_field_at_ordinal(&mut field_values, *ordinal)?;

                    let bit_value = match bit_mojom_value {
                        MojomValue::Bool(bit) => *bit,
                        MojomValue::Nullable(None) => false,
                        MojomValue::Nullable(Some(_)) if *is_tag_bit => true,
                        MojomValue::Nullable(Some(inner_value)) => {
                            // If deref patterns are ever stabilized, we won't need
                            // a nested match here.
                            match &**inner_value {
                                MojomValue::Bool(bit) => *bit,
                                _ => bail!("Got non-bool value when deparsing a bitfield"),
                            }
                        }
                        _ => {
                            wrong_type!(&packed_field, bit_mojom_value);
                        }
                    };
                    bitfield |= (bit_value as u8) << idx;
                }
                // Now we've set all the bits, write it to the wire
                data.push(bitfield)
            }
            StructuredBodyElement::SingleValue(ordinal, wire_type) => {
                match wire_type {
                    MojomWireType::Leaf { leaf_type, is_nullable } => {
                        let num_bytes = wire_type.size();
                        let leaf_value = take_field_at_ordinal(&mut field_values, ordinal)?;
                        // Null handles are indicated with all `f`s, everything else is all `0`s.
                        let none_indicator_byte =
                            if leaf_type == &PackedLeafType::Handle { 0xff } else { 0x00 };
                        let leaf_value = write_or_extract_nullable!(
                            data,
                            leaf_value,
                            *is_nullable,
                            num_bytes,
                            none_indicator_byte
                        );
                        pad_to_alignment(data, packed_field.alignment());
                        deparse_leaf_value(data, leaf_value, leaf_type)?
                    }

                    MojomWireType::Pointer { nested_data_type, is_nullable } => {
                        let nested_data_value = take_field_at_ordinal(&mut field_values, ordinal)?;
                        let nested_data_value =
                            write_or_extract_nullable!(data, nested_data_value, *is_nullable, 8);
                        let nested_data = match (nested_data_value, nested_data_type) {
                            (
                                MojomValue::Struct(_field_names, nested_data_fields),
                                PackedStructuredType::Struct {
                                    packed_field_types,
                                    packed_field_names: _,
                                    num_elements_in_value: _,
                                },
                            ) => NestedData::Struct {
                                field_values: nested_data_fields,
                                packed_fields: packed_field_types,
                            },
                            (
                                nested_data_value @ (MojomValue::Array(_) | MojomValue::String(_)),
                                PackedStructuredType::Array { element_type, array_type },
                            ) => NestedData::Array {
                                contents: nested_data_value,
                                element_type,
                                array_type,
                            },
                            (
                                MojomValue::Union(tag, value),
                                PackedStructuredType::Union { variants, .. },
                            ) => NestedData::Union { tag, value: *value, variants },
                            (
                                MojomValue::Map(values_map),
                                PackedStructuredType::Map { key_type, value_type },
                            ) => NestedData::Map { values_map, key_type, value_type },
                            (_, nested_data_value) => bail!(
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
                    MojomWireType::Union { variants, is_nullable } => {
                        let union_value = take_field_at_ordinal(&mut field_values, ordinal)?;
                        let union_value =
                            write_or_extract_nullable!(data, union_value, *is_nullable, 16);
                        let (tag, contained_value) = match union_value {
                            MojomValue::Union(tag, value) => (tag, value),
                            _ => {
                                wrong_type!(&packed_field, union_value);
                            }
                        };
                        pad_to_alignment(data, 8);
                        deparse_union(
                            data,
                            Some(nested_data_list),
                            tag,
                            *contained_value,
                            variants,
                        )?;
                    }
                }
            }
        }
    }

    if !is_array {
        // Non-array bodies must be a multiple of 8 bytes before we record
        // their size in their header.
        pad_to_alignment(data, 8);
    }

    let bytes_written = data.len() - initial_bytes;
    // Write the length of the struct to the first 4 bytes of the header
    // The usize->u32 cast should always work, because hopefully our message
    // is less than 2^32 bytes long!
    write_to_slice(data, initial_bytes, 4, &u32::to_le_bytes(bytes_written.try_into().unwrap()));

    // This is a no_op unless is_array is true.
    pad_to_alignment(data, 8);

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
            NestedData::Array { contents, element_type, array_type } => {
                deparse_array(data, contents, element_type, array_type)?
            }
            NestedData::Union { tag, value, variants } => {
                deparse_union(data, None, tag, value, variants)?
            }
            NestedData::Map { values_map, key_type, value_type } => {
                deparse_map(data, values_map, key_type, value_type)?
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
    value: MojomValue,
    wire_type: &MojomWireType,
) -> Result<DeparsedData> {
    let mut data = DeparsedData::new();
    match (wire_type, value) {
        (MojomWireType::Leaf { leaf_type, .. }, value) => {
            deparse_leaf_value(&mut data, value, leaf_type)?
        }
        (
            MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Struct { packed_field_types, .. },
                ..
            },
            MojomValue::Struct(_field_names, fields),
        ) => deparse_struct(&mut data, fields, packed_field_types)?,
        (
            MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Array { element_type, array_type },
                ..
            },
            value @ (MojomValue::Array(_) | MojomValue::String(_)),
        ) => deparse_array(&mut data, value, element_type, array_type)?,
        (
            MojomWireType::Union { variants, .. }
            | MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Union { variants },
                ..
            },
            MojomValue::Union(tag, value),
        ) => deparse_union(&mut data, None, tag, *value, variants)?,
        (
            MojomWireType::Pointer {
                nested_data_type: PackedStructuredType::Map { key_type, value_type },
                ..
            },
            MojomValue::Map(values_map),
        ) => deparse_map(&mut data, values_map, key_type, value_type)?,
        _ if wire_type.is_nullable() => {
            // Nullables only make sense in the context of an enclosing body
            panic!("Cannot deparse single nullable values for testing")
        }
        (_, value) => {
            wrong_type!(wire_type, value);
        }
    };
    Ok(data)
}
