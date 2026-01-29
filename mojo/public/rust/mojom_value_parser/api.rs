// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! FOR_RELEASE: Docs

use crate::ast::MojomValue;
use crate::errors::ParsingResult;
use crate::parsing_trait::MojomParse;

/// Deserialize a Rust struct from the provided Mojom message.
///
/// This function assumes the byte slice begins with a mojom-encoded value of
/// type `T`. It returns the decoded value, and the remaining bytes in the
/// message.
pub fn deserialize<T: MojomParse>(data_slice: &[u8]) -> ParsingResult<(&[u8], T)> {
    let (remaining_bytes, parsed_value) =
        crate::parse_values::parse_top_level_value(data_slice, T::wire_type())?;
    // Convert the parsed MojomValue to a T. This conversion should never fail,
    // since we passed T's wire type to the parser.
    Ok((remaining_bytes, parsed_value.try_into().unwrap()))
}

/// This function is the same as `deserialize`, but returns a `TooMuchData`
/// parsing error if there are bytes leftover after deserializating
pub fn deserialize_exact<T: MojomParse>(data_slice: &[u8]) -> ParsingResult<T> {
    let (remaining_bytes, parsed_value) = deserialize::<T>(data_slice)?;
    if !remaining_bytes.is_empty() {
        return Err(crate::errors::ParsingError::too_much_data(
            data_slice.len() - remaining_bytes.len(),
            remaining_bytes.len(),
        ));
    }
    // Convert the parsed MojomValue to a T. This conversion should never fail,
    // since we passed T's wire type to the parser.
    Ok(parsed_value.try_into().unwrap())
}

/// Serialize a Rust struct into a Mojom message
/// FOR_RELEASE: See if we can take a reference instead (or maybe in addition)
pub fn serialize<T: MojomParse>(value: T) -> Vec<u8> {
    let data: Vec<u8> = vec![];
    let packed_format = T::wire_type();
    // FOR_RELEASE: We haven't quite finished the deparser, but we'd call the
    // equivalent to parse_message here.
    let _: MojomValue = value.into();
    let _ = packed_format;
    data
}
