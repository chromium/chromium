// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! FOR_RELEASE: Docs

use crate::ast::MojomValue;
use crate::errors::ParsingResult;
use crate::parse_messages::parse_message;
use crate::parsing_trait::MojomParse;

/// Deserialize a Mojom Message into a Rust struct
pub fn deserialize<T: MojomParse>(data_slice: &[u8]) -> ParsingResult<T> {
    let parsed_value = parse_message(data_slice, T::wire_type())?;
    // Convert the parsed MojomValue to a T. This conversion should never fail,
    // since we passed T's wire type to the parser.
    return Ok(parsed_value.try_into().unwrap());
}

/// Serialize a Rust struct into a Mojom message
/// FOR_RELEASE: See if we can take a reference instead (or maybe in addition)
pub fn serialize<T: MojomParse>(value: T) -> ParsingResult<Vec<u8>> {
    let data: Vec<u8> = vec![];
    let packed_format = T::wire_type();
    // FOR_RELEASE: We haven't quite finished the deparser, but we'd call the
    // equivalent to parse_message here.
    let _: MojomValue = value.into();
    let _ = packed_format;
    return Ok(data);
}
