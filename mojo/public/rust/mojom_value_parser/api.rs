// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the public API of the value parser; namely, the ability
//! to translate between types implementing the `MojomParse` trait, and the
//! binary representation of that type.

use crate::ast::{MojomValue, UntypedHandle};
use crate::errors::ParsingResult;
use crate::parsing_trait::MojomParse;

/// Deserialize a Rust struct from the provided Mojom message.
///
/// This function assumes the byte slice begins with a mojom-encoded value of
/// type `T`. It returns the decoded value, and the remaining bytes in
/// the message. If ownership of any handles in `handles` was transferred, their
/// entries will be `None`.
///
/// The `interface_ids_offset` argument should be the distance from the
/// beginning of `data_slice` to the start of the embedded interface ID array,
/// or 0 if the array is absent.
pub fn deserialize<'a, Context, T: MojomParse<Context>>(
    data_slice: &'a [u8],
    handles: &'a mut [Option<UntypedHandle>],
    interface_ids_offset: u64,
    context: &Context,
) -> ParsingResult<(&'a [u8], T)> {
    let (remaining_bytes, parsed_value) = crate::parse_values::parse_top_level_value(
        data_slice,
        handles,
        interface_ids_offset,
        T::wire_type(),
    )?;
    // Convert the parsed MojomValue to a T. This conversion should never fail,
    // since we passed T's wire type to the parser.
    Ok((remaining_bytes, T::try_from_mojom_value(parsed_value, context).unwrap()))
}

/// This function is the same as `deserialize`, but returns a `TooMuchData`
/// parsing error if there are bytes leftover after deserializating
pub fn deserialize_exact<Context, T: MojomParse<Context>>(
    data_slice: &[u8],
    handles: &mut [Option<UntypedHandle>],
    interface_ids_offset: u64,
    context: &Context,
) -> ParsingResult<T> {
    let (remaining_bytes, parsed_value) =
        deserialize::<Context, T>(data_slice, handles, interface_ids_offset, context)?;
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
/// Returns the serialized bytes, any handles that were contained within
/// `value`, and the offset from the beginning of the serialized value to the
/// embedded array of interface IDs. If that array is absent, the offset is 0.
///
/// Panics if called on a non-struct (structs are the only valid top-level
/// type).
pub fn serialize<Context, T: MojomParse<Context>>(
    value: T,
    context: &Context,
) -> (Vec<u8>, Vec<UntypedHandle>, u64) {
    let mojom_value: MojomValue = value.into_mojom_value(context);
    crate::deparse_values::deparse_top_level_value(mojom_value, T::wire_type())
        // The Err return value is mostly useful for internal debugging
        .expect("Deparsing should always succeed")
}
