// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the format of mojom message headers, and the code to
//! parse them. These headers are always the first thing in a mojom message.
//!
//! They are encoded as a regular mojom struct; however, that encoding makes
//! use of mojom's versioning feature, which is not yet implemented in the
//! parser. Since headers are unavoidable, we define special parsing logic for
//! them.
//!
//! This module defines a MojomHeader type which supports V1-V3, and stores the
//! fields of that version, categorized by which version they were defined in.
//! It also includes functions for converting to/from a binary representation.
//! Those functions must be used for (de)serializing, because the types
//! deliberately do not implement `MojomParse`.
//!
//! We do not support header version V0 because it is not clear if any code
//! relies on it. Furthermore, that version does not support the `request_id`
//! field, which is fundamental enough that we should avoid special-casing it
//! if possible.
//!
//! FOR_RELEASE: Once we completely support versioning, move all of this into
//! `bindings/message_header.rs`.

use crate::errors::*;
use crate::parse_primitives::*;

/// A struct representing version 1 of the Mojom message header.
/// This includes the base header fields and the request ID.
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct MessageHeaderV1 {
    /// The ordinal of the mojom `interface` this message corresponds to. Used
    /// for sending multiple interfaces via the same pipe.
    pub interface_id: u32,
    /// The ordinal of the message within the above `interface`.
    pub name: u32,
    /// See the MessageHeaderFlags type for possible flags.
    pub flags: u32,
    /// A randomly-generated, hopefully-unique value for a message. Used in
    /// tracing, forming the lower part of the 64-bit trace id, which is
    /// used to match trace  events for sending and receiving a message
    /// (`name` forms the upper part).
    pub trace_nonce: u32,
    /// Contains an ID that's used to match responses with their corresponding
    /// request. Unused for messages that don't expect a response.
    pub request_id: u64,
}

/// A struct representing the fields added in version 2 of the Mojom message
/// header.
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct MessageHeaderV2 {
    /// A mojom pointer (i.e. a byte offset) to the message's payload.
    pub payload_ptr: u64,
    /// Used for associated interfaces. Rust doesn't support these yet.
    pub interface_ids_ptr: u64,
}

/// A struct representing the fields added in version 3 of the Mojom message
/// header.
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct MessageHeaderV3 {
    /// Tracks when the header was created.
    pub creation_timeticks_us: i64,
}

/// An enum representing the different versions of the Mojom message header.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum MessageHeader {
    V1(MessageHeaderV1),
    V2(MessageHeaderV1, MessageHeaderV2),
    V3(MessageHeaderV1, MessageHeaderV2, MessageHeaderV3),
}

impl MessageHeader {
    /// Parse a Mojom message header from the given data. Returns the header and
    /// the remaining unparsed bytes.
    ///
    /// This function supports versions 1, 2, and 3. Version 0 is not supported.
    /// If a version higher than 3 is encountered, it is parsed as version 3.
    pub fn deserialize(bytes: &[u8]) -> ParsingResult<(&[u8], Self)> {
        let mut parser_data = ParserData::new(bytes, &mut []);
        let data = &mut parser_data;
        let start_offset = data.bytes_parsed();
        let size = parse_u32(data)? as usize;
        let version = parse_u32(data)?;

        if version == 0 {
            return Err(ParsingError::not_implemented(
                start_offset,
                "Mojom V0 Headers".to_string(),
            ));
        }
        if version == 1 && size != 32 {
            return Err(ParsingError::not_implemented(
                start_offset,
                "V1 Headers must have 32 bytes".to_string(),
            ));
        }
        if version == 2 && size != 48 {
            return Err(ParsingError::not_implemented(
                start_offset,
                "V2 Headers must have 48 bytes".to_string(),
            ));
        }
        if version == 3 && size != 56 {
            return Err(ParsingError::not_implemented(
                start_offset,
                "V3 Headers must have 56 bytes".to_string(),
            ));
        }
        if version > 3 && size < 56 {
            return Err(ParsingError::not_implemented(
                start_offset,
                "V4+ Headers must have at least 56 bytes".to_string(),
            ));
        }

        let v1 = MessageHeaderV1 {
            interface_id: parse_u32(data)?,
            name: parse_u32(data)?,
            flags: parse_u32(data)?,
            trace_nonce: parse_u32(data)?,
            request_id: parse_u64(data)?,
        };

        if version == 1 {
            return Ok((parser_data.into_bytes(), MessageHeader::V1(v1)));
        }

        let v2 =
            MessageHeaderV2 { payload_ptr: parse_u64(data)?, interface_ids_ptr: parse_u64(data)? };

        if version == 2 {
            return Ok((parser_data.into_bytes(), MessageHeader::V2(v1, v2)));
        }

        let v3 = MessageHeaderV3 { creation_timeticks_us: parse_i64(data)? };

        // If we have some higher version number that's fine,
        // just skip all the extra fields.
        parse_padding(data, size - 56)?;

        Ok((parser_data.into_bytes(), MessageHeader::V3(v1, v2, v3)))
    }

    /// Serialize the header into a byte vector.
    pub fn serialize(self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(56);
        match self {
            MessageHeader::V1(v1) => {
                buf.extend(32u32.to_le_bytes());
                buf.extend(1u32.to_le_bytes());
                Self::serialize_v1(&mut buf, &v1);
            }
            MessageHeader::V2(v1, v2) => {
                buf.extend(48u32.to_le_bytes());
                buf.extend(2u32.to_le_bytes());
                Self::serialize_v1(&mut buf, &v1);
                Self::serialize_v2(&mut buf, &v2);
            }
            MessageHeader::V3(v1, v2, v3) => {
                buf.extend(56u32.to_le_bytes());
                buf.extend(3u32.to_le_bytes());
                Self::serialize_v1(&mut buf, &v1);
                Self::serialize_v2(&mut buf, &v2);
                Self::serialize_v3(&mut buf, &v3);
            }
        }
        buf
    }

    fn serialize_v1(buf: &mut Vec<u8>, v1: &MessageHeaderV1) {
        buf.extend(v1.interface_id.to_le_bytes());
        buf.extend(v1.name.to_le_bytes());
        buf.extend(v1.flags.to_le_bytes());
        buf.extend(v1.trace_nonce.to_le_bytes());
        buf.extend(v1.request_id.to_le_bytes());
    }

    fn serialize_v2(buf: &mut Vec<u8>, v2: &MessageHeaderV2) {
        buf.extend(v2.payload_ptr.to_le_bytes());
        buf.extend(v2.interface_ids_ptr.to_le_bytes());
    }

    fn serialize_v3(buf: &mut Vec<u8>, v3: &MessageHeaderV3) {
        buf.extend(v3.creation_timeticks_us.to_le_bytes());
    }
}
