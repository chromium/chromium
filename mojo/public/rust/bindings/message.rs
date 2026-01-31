// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the rust representation of a Mojom message. It consists
//! of a structured header, followed by an unstructured payload (a series of
//! bytes representing a value that has been serialized to its Mojom wire
//! representation).

chromium::import! {
    "//mojo/public/rust/mojom_parser";
}

use crate::message_header::*;
use mojom_parser::ParsingResult;

/// Represents a Mojom message with a structured header and unstructured
/// payload.
///
/// This type does not make any guarantees. It is the user's responsibility to
/// ensure that the header and payload combine to create a valid Mojom message.
/// In practice, this means ensuring that the payload represents an encoded
/// mojom value (obtained from mojom_parser::serialize), and that the header
/// matches the value. See message_header.rs for more information on headers.
///
/// FOR_RELEASE: Integrate/replace this with the new RawMojoMessage type in the
/// system bindings
pub struct MojomMessage {
    pub header: MessageHeaderV3,
    pub payload: Vec<u8>,
}

impl MojomMessage {
    /// Parse the header from a binary message.
    pub fn from_bytes(mut data: Vec<u8>) -> ParsingResult<Self> {
        let (remaining_bytes, header) = MessageHeaderV3::deserialize_with_version(&mut data)?;
        let remaining_bytes_len = remaining_bytes.len();
        let num_consumed_bytes = data.len() - remaining_bytes_len;
        let _ = data.drain(0..num_consumed_bytes);
        Ok(MojomMessage { header, payload: data })
    }

    /// Serialize this message into its binary equivalent.
    pub fn into_bytes(self) -> Vec<u8> {
        let mut serialized = self.header.serialize_with_version();
        serialized.extend(self.payload);
        serialized
    }
}
