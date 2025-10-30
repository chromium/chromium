// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! FOR_RELEASE: Docs

use crate::parse_messages::parse_message;
use crate::parsing_trait::MojomParse;

use anyhow::Result;

/// Deserialize a Mojom Message into a Rust struct
pub fn deserialize<T: MojomParse>(data_slice: &[u8]) -> Result<T> {
    let parsed_value = parse_message(data_slice, T::wire_type())?;
    // FOR_RELEASE: Error handling
    return Ok(T::from_mojom_value(parsed_value).unwrap());
}

/// Serialize a Rust struct into a Mojom message
pub fn serialize<T: MojomParse>(value: T) -> Result<Vec<u8>> {
    let data: Vec<u8> = vec![];
    let packed_format = T::wire_type();
    // FOR_RELEASE: We haven't quite finished the deparser, but we'd call the
    // equivalent to parse_message here.
    let _ = (value.to_mojom_value(), packed_format);
    return Ok(data);
}
