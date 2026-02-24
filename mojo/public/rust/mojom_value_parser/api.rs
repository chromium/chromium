// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! FOR_RELEASE: Docs

use crate::ast::{MojomValue, UntypedHandle};
use crate::errors::ParsingResult;
use crate::parsing_trait::MojomParse;

/// Deserialize a Rust struct from the provided Mojom message.
///
/// This function assumes the byte slice begins with a mojom-encoded value of
/// type `T`. It returns the decoded value, and the remaining bytes in
/// the message. If ownership of any handles in `handles` was transferred, their
/// entries will be `None`.
pub fn deserialize<'a, T: MojomParse>(
    data_slice: &'a [u8],
    handles: &'a mut [Option<UntypedHandle>],
) -> ParsingResult<(&'a [u8], T)> {
    let (remaining_bytes, parsed_value) =
        crate::parse_values::parse_top_level_value(data_slice, handles, T::wire_type())?;
    // Convert the parsed MojomValue to a T. This conversion should never fail,
    // since we passed T's wire type to the parser.
    Ok((remaining_bytes, parsed_value.try_into().unwrap()))
}

/// This function is the same as `deserialize`, but returns a `TooMuchData`
/// parsing error if there are bytes leftover after deserializating
pub fn deserialize_exact<T: MojomParse>(
    data_slice: &[u8],
    handles: &mut [Option<UntypedHandle>],
) -> ParsingResult<T> {
    let (remaining_bytes, parsed_value) = deserialize::<T>(data_slice, handles)?;
    if !remaining_bytes.is_empty() {
        return Err(crate::errors::ParsingError::too_much_data(
            data_slice.len() - remaining_bytes.len(),
            remaining_bytes.len(),
        ));
    }
    Ok(parsed_value)
}

/// Serialize a Rust struct into a Mojom message.
///
/// Panics if called on a non-struct (structs are the only valid top-level
/// type).
pub fn serialize<T: MojomParse>(value: T) -> (Vec<u8>, Vec<UntypedHandle>) {
    let mut data = crate::deparse_values::DeparsedData::new();
    let packed_format = T::wire_type();
    let mojom_value: MojomValue = value.into();
    // Make sure we actually got a struct, and unpack it.
    let (field_values, packed_fields) = match (mojom_value, packed_format) {
        (
            MojomValue::Struct(_, field_values),
            crate::MojomWireType::Pointer {
                nested_data_type: crate::PackedStructuredType::Struct { packed_field_types, .. },
                is_nullable: false,
            },
        ) => (field_values, packed_field_types),
        _ => panic!("`serialize` can only be called on struct types"),
    };
    crate::deparse_values::deparse_struct(&mut data, field_values, packed_fields)
        // The Err return value is mostly useful for internal debugging
        .expect("Deparsing should always succeed");
    data.into_parts()
}
