// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    // `pub` since a macro refers to `$crate::system`.
    pub "//mojo/public/rust:mojo_system" as system;
}

pub mod data;

use std::mem::size_of;

use bytemuck::Zeroable;

/// Describes why a mojo message is invalid.
///
/// This mirrors //mojo/public/cpp/bindings/lib/validation_errors.h; refer to
/// that file for documentation. Do not assume the integer representations are
/// the same as in C++. `VALIDATION_ERROR_NONE` has no corresponding variant;
/// use `Result<(), ValidationError>` if needed.
#[derive(Clone, Copy, Debug)]
pub enum ValidationError {
    IllegalMemoryRange,
    UnexpectedStructHeader,
    MessageHeaderInvalidFlags,
    MessageHeaderMissingRequestId,
}

impl ValidationError {
    pub fn to_str(self) -> &'static str {
        use ValidationError::*;
        match self {
            IllegalMemoryRange => "VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE",
            UnexpectedStructHeader => "VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER",
            MessageHeaderInvalidFlags => "VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS",
            MessageHeaderMissingRequestId => "VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID",
        }
    }
}

pub type Result<T> = std::result::Result<T, ValidationError>;

/// A read-only view to a serialized mojo message.
pub struct MessageView<'a> {
    // The entire message buffer, including the header.
    _buf: &'a [u8],
    // Parsed message header.
    _header: data::MessageHeader,
}

impl<'a> MessageView<'a> {
    /// Create from a buffer that contains a serialized message. Validates the
    /// message header.
    pub fn new(buf: &'a [u8]) -> Result<Self> {
        let struct_header: data::StructHeader = bytemuck::pod_read_unaligned(
            buf.get(0..size_of::<data::StructHeader>())
                .ok_or(ValidationError::IllegalMemoryRange)?,
        );

        let mut msg_header: data::MessageHeader = Zeroable::zeroed();

        let bytes_to_copy =
            std::cmp::min(std::mem::size_of_val(&msg_header), struct_header.size as usize);
        bytemuck::bytes_of_mut(&mut msg_header)[0..bytes_to_copy]
            .copy_from_slice(buf.get(0..bytes_to_copy).ok_or(ValidationError::IllegalMemoryRange)?);
        let msg_header = msg_header;

        match msg_header.header.version {
            0 => {
                if msg_header.header.size as usize != data::MESSAGE_HEADER_V0_SIZE {
                    return Err(ValidationError::UnexpectedStructHeader);
                }

                // Either of these flags require the request_id field, which
                // implies the version should be at least 1.
                if msg_header.flags.intersects(
                    data::MessageHeaderFlags::EXPECTS_RESPONSE
                        | data::MessageHeaderFlags::IS_RESPONSE,
                ) {
                    return Err(ValidationError::MessageHeaderMissingRequestId);
                }
            }
            1 => {
                if msg_header.header.size as usize != data::MESSAGE_HEADER_V1_SIZE {
                    return Err(ValidationError::UnexpectedStructHeader);
                }
            }
            2 => {
                if msg_header.header.size as usize != data::MESSAGE_HEADER_V2_SIZE {
                    return Err(ValidationError::UnexpectedStructHeader);
                }
            }
            _ => {
                if (msg_header.header.size as usize) < data::MESSAGE_HEADER_V2_SIZE {
                    return Err(ValidationError::UnexpectedStructHeader);
                }
            }
        }

        // These flags are mutually exclusive.
        if msg_header.flags.contains(
            data::MessageHeaderFlags::EXPECTS_RESPONSE | data::MessageHeaderFlags::IS_RESPONSE,
        ) {
            return Err(ValidationError::MessageHeaderInvalidFlags);
        }

        Ok(Self { _buf: buf, _header: msg_header })
    }
}
