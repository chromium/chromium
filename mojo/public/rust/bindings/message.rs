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
use system::message::{BadMessageError, RawMojoMessage};
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
// TODO(crbug.com/493265340): This is kind of a crummy type, we should come up
// with a better API that leverages the RawMojoMessage type in the system
// bindings. As part of the process, we should catalogue the possible errors
// that each step might return.
pub struct MojomMessage {
    pub header: MessageHeader,
    pub payload: Vec<u8>,
    pub handles: Vec<UntypedHandle>,
    // This field should only be set for messages that came in across the wire;
    // we keep the raw handle around so we can report a bad message later if
    // necessary.
    pub raw_message_handle: Option<RawMojoMessage>,
}

impl MojomMessage {
    /// Parse the provided raw message object's header, and extract its data.
    ///
    /// If parsing fails, this will return `None` and report the original
    /// message as malformed.
    pub fn parse_raw_or_report_bad_message(mut msg: RawMojoMessage) -> Option<Self> {
        let (raw_bytes, handles) = msg.read_data().unwrap();
        let (remaining_bytes, header) = match MessageHeader::deserialize(raw_bytes) {
            Ok(data) => data,
            Err(err) => {
                let _ = msg.report_bad_message(&err.to_string());
                return None;
            }
        };

        // We might be able to avoid allocating here if we had a better
        // MojomMessage type.
        let payload = remaining_bytes.to_vec();
        Some(MojomMessage { header, payload, handles, raw_message_handle: Some(msg) })
    }

    /// Parse the given raw message into a structured representation.
    pub fn report_bad_message(&mut self, error_msg: &str) -> Result<(), BadMessageError> {
        match &mut self.raw_message_handle {
            Some(raw_msg) => raw_msg.report_bad_message(error_msg),
            // This should only happen if someone calls this function on a message
            // they didn't receive, which means they created it themselves.
            None => panic!("Cannot report a bad message that doesn't have an underlying handle"),
        }
    }

    /// Serialize this message into its binary equivalent, and return the
    /// attached handles
    pub fn into_data(self) -> (Vec<u8>, Vec<UntypedHandle>) {
        let mut serialized = self.header.serialize();
        serialized.extend(self.payload);
        (serialized, self.handles)
    }
}

impl From<MojomMessage> for RawMojoMessage {
    fn from(msg: MojomMessage) -> Self {
        let (payload, handles) = msg.into_data();
        // This can only fail if we're out of memory
        RawMojoMessage::new_with_data(&payload, handles).unwrap()
    }
}
