// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the rust representation of a Mojom message. It consists
//! of a structured header, followed by an unstructured payload (a series of
//! bytes representing a value that has been serialized to its Mojom wire
//! representation).

chromium::import! {
    "//mojo/public/rust/mojom_value_parser";
    "//mojo/public/rust/system";
}

use crate::message_header::*;
use mojom_value_parser::ParsingResult;
use system::message::RawMojoMessage;
use system::mojo_types::UntypedHandle;

/// Represents a Mojom message with a structured header and unstructured
/// payload.
///
/// This type does not make any guarantees. It is the user's responsibility to
/// ensure that the header and payload combine to create a valid Mojom message.
/// In practice, this means ensuring that the payload represents an encoded
/// mojom value (obtained from mojom_value_parser::serialize), and that the
/// header matches the value. See message_header.rs for more information on
/// headers.
///
/// FOR_RELEASE: This is kind of a crummy type, we should come up with a better
/// API that leverages the RawMojoMessage type in the system bindings
pub struct MojomMessage {
    pub header: MessageHeader,
    pub payload: Vec<u8>,
    pub handles: Vec<UntypedHandle>,
}

impl MojomMessage {
    /// Parse the given raw message into a structured representation.
    pub fn from_raw(msg: &RawMojoMessage) -> ParsingResult<Self> {
        // FOR_RELEASE: Make sure any MojoErrors are handled gracefully.
        let (raw_bytes, handles) = msg.read_data().unwrap();
        let (remaining_bytes, header) = MessageHeader::deserialize(raw_bytes)?;
        // FOR_RELEASE: Hopefully once we make our MojomMessage type better we
        // can avoid calling to_vec here.
        let payload = remaining_bytes.to_vec();
        Ok(MojomMessage { header, payload, handles })
    }

    /// Serialize this message into its binary equivalent, and return the
    /// attached handles
    pub fn into_data(self) -> (Vec<u8>, Vec<UntypedHandle>) {
        let mut serialized = self.header.serialize();
        serialized.extend(self.payload);
        (serialized, self.handles)
    }
}
