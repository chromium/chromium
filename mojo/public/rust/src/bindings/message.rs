// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use bindings::decoding::{Decoder, ValidationError};
use bindings::encoding;
use bindings::encoding::{Context, DATA_HEADER_SIZE, DataHeaderValue, Encoder};
use bindings::mojom::{MojomEncodable, MojomPointer, MojomStruct};

/// A flag for the message header indicating that no flag has been set.
pub const MESSAGE_HEADER_NO_FLAG: u32 = 0;

/// A flag for the message header indicating that this message expects
/// a response.
pub const MESSAGE_HEADER_EXPECT_RESPONSE: u32 = 1;

/// A flag for the message header indicating that this message is
/// a response.
pub const MESSAGE_HEADER_IS_RESPONSE: u32 = 2;

const MESSAGE_HEADER_VERSIONS: [(u32, u32); 2] = [(0, 16), (1, 24)];

/// A message header object implemented as a Mojom struct.
pub struct MessageHeader {
    pub version: u32,
    pub name: u32,
    pub flags: u32,
    pub request_id: u64,
}

impl MessageHeader {
    /// Create a new MessageHeader.
    pub fn new(version: u32, name: u32, flags: u32) -> MessageHeader {
        MessageHeader {
            version: version,
            name: name,
            flags: flags,
            request_id: 0,
        }
    }
}

impl MojomPointer for MessageHeader {
    fn header_data(&self) -> DataHeaderValue {
        DataHeaderValue::Version(self.version)
    }

    /// Get the serialized size.
    ///
    /// This value differs based on whether or not
    /// a request_id is necessary.
    fn serialized_size(&self, _context: &Context) -> usize {
        let mut size = DATA_HEADER_SIZE + 8;
        if self.flags != MESSAGE_HEADER_NO_FLAG {
            size += 8;
        }
        encoding::align_default(size)
    }

    fn encode_value(self, encoder: &mut Encoder, context: Context) {
        MojomEncodable::encode(self.name, encoder, context.clone());
        MojomEncodable::encode(self.flags, encoder, context.clone());
        if self.version > 0 {
            MojomEncodable::encode(self.request_id, encoder, context.clone());
        }
    }

    fn decode_value(decoder: &mut Decoder, context: Context) -> Result<Self, ValidationError> {
        let mut state = decoder.get_mut(&context);
        let version = match state.decode_struct_header(&MESSAGE_HEADER_VERSIONS) {
            Ok(header) => header.data(),
            Err(err) => return Err(err),
        };
        let name = state.decode::<u32>();
        let flags = state.decode::<u32>();
        if flags > MESSAGE_HEADER_IS_RESPONSE {
            return Err(ValidationError::MessageHeaderInvalidFlags);
        }
        if version == 0 {
            if flags == MESSAGE_HEADER_IS_RESPONSE || flags == MESSAGE_HEADER_EXPECT_RESPONSE {
                return Err(ValidationError::MessageHeaderMissingRequestId);
            }
            Ok(MessageHeader {
                version: version,
                name: name,
                flags: flags,
                request_id: 0,
            })
        } else if version == 1 {
            Ok(MessageHeader {
                version: version,
                name: name,
                flags: flags,
                request_id: state.decode::<u64>(),
            })
        } else {
            return Err(ValidationError::UnexpectedStructHeader);
        }
    }
}

impl MojomEncodable for MessageHeader {
    impl_encodable_for_pointer!();
    fn compute_size(&self, context: Context) -> usize {
        self.serialized_size(&context)
    }
}

impl MojomStruct for MessageHeader {}

