// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the header used by Mojom messages, and provides utility
//! functions for manipulating it. For a reference implementation, see the C++
//! version at //mojo/public/cpp/bindings/lib/message_internal.h

chromium::import! {
    "//mojo/public/rust/mojom_value_parser";
    "//mojo/public/rust/bindings:helper_functions_cxx";
}

use helper_functions_cxx::ffi as cxx_helpers;

/// The header format used by Mojom.
///
/// A header value appears at the beginning of every Mojom message. It is
/// encoded as a regular mojom struct; however, it uses versioning, which is
/// not yet supported by the mojom value parser. Therefore, this type must be
/// serialized and deserialized using its own methods, rather than relying on
/// the methods in the mojom_value_parser API.
///
/// This type can represent a mojom header of version 1, 2, or 3. The versions
/// only differ in non-`pub` fields, so users of this type can typically ignore
/// this distinction.
///
/// The `pub` fields may be read and written by the user, so it is the user's
/// responsibility to ensure that the header is valid when sent. The main
/// validity conditions are:
/// - The `interface_id` and `name` fields exist for the message pipe it's being
///   sent on.
/// - The payload of the message is of the type indicated by the `name` field.
/// - The flags are correct.
/// - If this is a response, then the request_id matches the one in the request.
#[derive(Debug, Clone, Copy)]
pub struct MessageHeader {
    /// The version of the header.
    version: u32,
    /// The ordinal of the mojom `interface` this message corresponds to. Used
    /// for sending multiple interfaces via the same pipe.
    pub interface_id: u32,
    /// The ordinal of the message within the above `interface`.
    pub name: u32,
    /// See the MessageHeaderFlags type for possible flags.
    pub flags: MessageHeaderFlags,
    /// A randomly-generated, hopefully-unique value for a message. Used in
    /// tracing, forming the lower part of the 64-bit trace id, which is
    /// used to match trace events for sending and receiving a message
    /// (`name` forms the upper part).
    trace_nonce: u32,
    /// Contains an ID that's used to match responses with their corresponding
    /// request. Unused for messages that don't expect a response.
    pub request_id: u64,
    /// A mojom pointer (i.e. a byte offset) to the message's payload.
    payload_ptr: u64,
    /// A mojom pointer to the interface ID array at the end of the message (or
    /// 0 if it's absent). This field should only be set for messages that
    /// were received over the wire, or which have been fully serialized and
    /// are ready to be sent.
    /// Note that this field's value is measured from the pointer's location
    /// inside the _header_. Parsing code generally wants the distance from the
    /// beginning of the payload instead, which can be obtained using the
    /// `interface_ids_offset` method.
    interface_ids_ptr: u64,
    /// Tracks when the header was created.
    creation_timeticks_us: i64,
}

impl MessageHeader {
    /// Create a new message header with the given fields.
    ///
    /// Note that `interface_ids_offset` should be the offset from the start of
    /// the payload to the interface ID array (or 0, if there isn't an interface
    /// ID array). The constructor will compute the appropriate value for the
    /// `interface_ids_ptr` header field from that information.
    pub fn new(
        interface_id: u32,
        name: u32,
        flags: MessageHeaderFlags,
        request_id: u64,
        interface_ids_offset: u64,
    ) -> Self {
        Self::new_with_version(3, interface_id, name, flags, request_id, interface_ids_offset)
    }

    /// Create a new message at a specific version with the given fields. Fields
    /// which are not present at that version will be ignored.
    ///
    /// Note that `interface_ids_offset` should be the offset from the start of
    /// the payload to the interface ID array (or 0, if there isn't an interface
    /// ID array). The constructor will compute the appropriate value for the
    /// `interface_ids_ptr` from that information.
    pub fn new_with_version(
        version: u32,
        interface_id: u32,
        name: u32,
        flags: MessageHeaderFlags,
        request_id: u64,
        interface_ids_offset: u64,
    ) -> Self {
        // The header contains two pointers, which are next to each other. This
        // is the number of bytes from the end of the interface ID pointer to
        // the end of the header.
        let payload_ptr = match version {
            1 => 0, // Not present in this version
            2 => 16,
            3 => 24,
            _ => panic!("Mojom headers must have version 1, 2 or 3"),
        };
        let interface_ids_distance = match version {
            1 => 0, // Not present in this version
            2 => 8 + interface_ids_offset,
            3 => 16 + interface_ids_offset,
            _ => panic!("Mojom headers must have version 1, 2 or 3"),
        };
        let interface_ids_ptr = if interface_ids_offset == 0 { 0 } else { interface_ids_distance };
        MessageHeader {
            interface_id,
            name,
            flags,
            // The C++ equivalent uses static_cast<uint32_t> on the result here
            trace_nonce: cxx_helpers::GetNextGlobalTraceId() as u32,
            request_id,
            payload_ptr,
            interface_ids_ptr,
            creation_timeticks_us: cxx_helpers::CurrentTimeTicksInMicroseconds(),
            version,
        }
    }

    /// Return the timestamp when this header value was created.
    pub fn creation_timeticks_us(&self) -> i64 {
        self.creation_timeticks_us
    }

    /// Return the offset from the beginning of the _payload_ to the beginning
    /// of the interface IDs array, or 0 if the array is absent. Note that this
    /// is not the same as the `interface_ids_ptr` header field, which is
    /// measured from a point inside the header rather than relative to the
    /// payload.
    pub fn interface_ids_offset(&self) -> u64 {
        if self.interface_ids_ptr == 0 {
            return 0;
        }
        // Use saturating subtraction to prevent underflow if we got a bad message.
        // If it happens, we'll probably fail shortly when we try to actually parse the
        // message.
        match self.version {
            1 => 0, // Not present in this version
            2 => self.interface_ids_ptr.saturating_sub(8),
            3 => self.interface_ids_ptr.saturating_sub(16),
            _ => panic!("Mojom headers must have version 1, 2 or 3"),
        }
    }

    /// Serialize the header as a mojom value
    pub fn serialize(self) -> Vec<u8> {
        let v1 = mojom_value_parser::MessageHeaderV1 {
            interface_id: self.interface_id,
            name: self.name,
            flags: self.flags.bits(),
            trace_nonce: self.trace_nonce,
            request_id: self.request_id,
        };
        let v2 = mojom_value_parser::MessageHeaderV2 {
            payload_ptr: self.payload_ptr,
            interface_ids_ptr: self.interface_ids_ptr,
        };
        let v3 = mojom_value_parser::MessageHeaderV3 {
            creation_timeticks_us: self.creation_timeticks_us,
        };

        let header = match self.version {
            1 => mojom_value_parser::MessageHeader::V1(v1),
            2 => mojom_value_parser::MessageHeader::V2(v1, v2),
            3 => mojom_value_parser::MessageHeader::V3(v1, v2, v3),
            _ => panic!("Header version must be 1, 2, or 3"),
        };

        header.serialize()
    }

    /// Deserialize the header from a mojom value.
    pub fn deserialize(data: &[u8]) -> mojom_value_parser::ParsingResult<(&[u8], Self)> {
        let (remaining, header) = mojom_value_parser::MessageHeader::deserialize(data)?;

        let result = match header {
            mojom_value_parser::MessageHeader::V1(v1) => MessageHeader {
                interface_id: v1.interface_id,
                name: v1.name,
                flags: MessageHeaderFlags::from_bits_truncate(v1.flags),
                trace_nonce: v1.trace_nonce,
                request_id: v1.request_id,
                payload_ptr: 0,
                interface_ids_ptr: 0,
                creation_timeticks_us: 0,
                version: 1,
            },
            mojom_value_parser::MessageHeader::V2(v1, v2) => MessageHeader {
                interface_id: v1.interface_id,
                name: v1.name,
                flags: MessageHeaderFlags::from_bits_truncate(v1.flags),
                trace_nonce: v1.trace_nonce,
                request_id: v1.request_id,
                payload_ptr: v2.payload_ptr,
                interface_ids_ptr: v2.interface_ids_ptr,
                creation_timeticks_us: 0,
                version: 2,
            },
            mojom_value_parser::MessageHeader::V3(v1, v2, v3) => MessageHeader {
                interface_id: v1.interface_id,
                name: v1.name,
                flags: MessageHeaderFlags::from_bits_truncate(v1.flags),
                trace_nonce: v1.trace_nonce,
                request_id: v1.request_id,
                payload_ptr: v2.payload_ptr,
                interface_ids_ptr: v2.interface_ids_ptr,
                creation_timeticks_us: v3.creation_timeticks_us,
                version: 3,
            },
        };

        Ok((remaining, result))
    }
}

bitflags::bitflags! {
    /// Flags that can be set for a Mojom message.
    /// Only the first two flags are supported in Rust at the moment.
    #[derive(Clone, Copy, Default, Debug)]
    #[repr(transparent)]
    pub struct MessageHeaderFlags: u32 {
        const EXPECTS_RESPONSE = 1 << 0;
        const IS_RESPONSE = 1 << 1;
        const IS_SYNC = 1 << 2;
        const NO_INTERRUPT = 1 << 3;
        const IS_URGENT = 1 << 4;
    }
}

// This section is an implementation detail: we have to manually implement
// (de)serialization for the flag type, since the output of `bitflags!` has
// special semantics: it's a struct, but we want to treat it like an int. Also,
// not all flag combinations are valid.
const _: () = {
    chromium::import! {
            "//mojo/public/rust/mojom_value_parser:mojom_value_parser_core";
    }
    use mojom_value_parser_core::{MojomType, MojomValue};

    impl MessageHeaderFlags {
        /// Check if a given combination of flags is valid.
        /// Validity conditions:
        /// - Responses must not expect a response.
        fn is_valid(&self) -> bool {
            !(self.contains(Self::EXPECTS_RESPONSE) && self.contains(Self::IS_RESPONSE))
        }
    }

    impl<Context> mojom_value_parser::MojomParse<Context> for MessageHeaderFlags {
        fn mojom_type() -> MojomType {
            MojomType::UInt32
        }

        fn into_mojom_value(self, _context: &Context) -> MojomValue {
            MojomValue::UInt32(self.bits())
        }

        fn try_from_mojom_value(value: MojomValue, _context: &Context) -> anyhow::Result<Self> {
            let bits: u32 = if let MojomValue::UInt32(bits) = value {
                bits
            } else {
                anyhow::bail!(
                    "Cannot construct a value of type {} from this MojomValue: {:?}",
                    std::any::type_name::<Self>(),
                    value
                );
            };
            match Self::from_bits(bits) {
                None => anyhow::bail!("Mojom header bitflags set unused bits: {:?}", bits),
                Some(flags) if !flags.is_valid() => {
                    anyhow::bail!("Mojom header bitflags are invalid: {:?}", bits)
                }
                Some(flags) => Ok(flags),
            }
        }
    }
};
