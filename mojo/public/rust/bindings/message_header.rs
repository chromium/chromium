// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the header used by Mojom messages, and provides utility
//! functions for manipulating it. For a reference implementation, see the C++
//! version at //mojo/public/cpp/bindings/lib/message_internal.h
//!
//! Note that the rust bindings do not support Mojom's versioning feature. This
//! means we only track the most recent incarnation of the header, with all
//! possible fields. Messages containing older headers will fail to parse.

chromium::import! {
    "//mojo/public/rust/mojom_value_parser";
    "//mojo/public/rust/bindings:helper_functions_cxx";
}

use helper_functions_cxx::ffi as cxx_helpers;

/// The header format used by the most recent version of Mojom.
///
/// This value appears at the beginning of every Mojom method. It is encoded
/// just like any other struct, with one difference: it uses the version number
/// field in its header (bytes 4-7 in its encoding).
///
/// The version number is used by the C++ bindings to determine which fields
/// are present in the struct. Since the Rust bindings do not currently
/// support versioning, our serializer always sets this value to 0.
///
/// However, mojom headers set this value to 3 instead. This means we can't use
/// our normal serialization code. As a result, you should not use the derived
/// MojomParse implementation directly. Instead, call
/// `(de)serialize_with_version`.
///
/// The `pub` fields may be read and written by the user, so it is the user's
/// responsibility to ensure that the header is valid when sent. The main
/// validition conditions are:
/// - The `interface_id` and `name` fields exist for the message pipe it's being
///   sent on.
/// - The payload of the message is of the type indicated by the `name` field.
/// - The flags are correct.
/// - If this is a response, then the request_id matches the one in the request.
#[derive(mojom_value_parser::MojomParse, Debug)]
pub struct MessageHeaderV3 {
    /// The ordinal of the mojom `interface` this message corresponds to. Used
    /// for sending multiple interfaces via the same pipe.
    pub interface_id: u32,
    /// The ordinal of the message within the above `interface`.
    pub name: u32,
    /// See the MessageHeaderFlags type for possible flags.
    pub flags: MessageHeaderFlags,
    /// A randomly-generated, hopefully-unique value for a message. Used in
    /// tracing, forming the lower part of the 64-bit trace id, which is
    /// used to match trace  events for sending and receiving a message
    /// (`name` forms the upper part).
    trace_nonce: u32,
    /// Contains an ID that's used to match responses with their corresponding
    /// request. Unused for messages that don't expect a response.
    pub request_id: u64,
    /// A mojom pointer (i.e. a byte offset) to the message's payload.
    payload_ptr: u64,
    /// Used for associated interfaces. Rust doesn't support these yet.
    interface_ids_ptr: u64,
    /// Tracks when the header was created.
    creation_timeticks_us: i64,
}

impl MessageHeaderV3 {
    pub fn new(interface_id: u32, name: u32, flags: MessageHeaderFlags, request_id: u64) -> Self {
        MessageHeaderV3 {
            interface_id,
            name,
            flags,
            // The C++ equivalent uses static_cast<uint32_t> on the result here
            trace_nonce: cxx_helpers::GetNextGlobalTraceId() as u32,
            request_id,
            // The payload always immediately follows the header, so it's 24
            // bytes from the start of `payload_ptr`
            payload_ptr: 24,
            // This pointer is unused in Rust for now
            interface_ids_ptr: 0,
            creation_timeticks_us: cxx_helpers::CurrentTimeTicksInMicroseconds(),
        }
    }

    /// Return the timestamp when this header value was created.
    pub fn creation_timeticks_us(&self) -> i64 {
        self.creation_timeticks_us
    }

    /// Serialize the header as a mojom value, but replace the version number
    /// in the encoding with 3, because the serializer doesn't handle versioning
    pub fn serialize_with_version(self) -> Vec<u8> {
        let (mut serialized, _) = mojom_value_parser::serialize(self);
        Self::set_version_number(&mut serialized, 3);
        return serialized;
    }

    /// Deserialize the header from a mojom value, but first replace the version
    /// number in the encoding with 0, because the deserializer doesn't
    /// handle versioning.
    ///
    /// This function must have mutable access to the buffer so it can
    /// overwrite the version value before deserializing.
    pub fn deserialize_with_version(
        data: &mut [u8],
    ) -> mojom_value_parser::ParsingResult<(&[u8], Self)> {
        Self::set_version_number(data, 0);
        return mojom_value_parser::deserialize(data, &mut []);
    }

    /// Replace the version number of a serialized header with `new_num`.
    /// This is necessary after serializing because the serializer doesn't
    /// currently handle versioning, so by default the encoded version numbers
    /// will be 0, but on the wire they're expected to be 3.
    // TODO(crbug.com/483986574): Handle versioning
    fn set_version_number(serialized: &mut [u8], new_num: u8) {
        // The serialized value begins with 4 bytes containing the size, then
        // 4 bytes containing the version number. The version never goes above
        // 3, so it's sufficient to just replace byte number 5 (at index 4).
        serialized[4] = new_num;
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
// special semantics.
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

    impl From<MessageHeaderFlags> for MojomValue {
        fn from(value: MessageHeaderFlags) -> MojomValue {
            MojomValue::UInt32(value.bits())
        }
    }

    impl TryFrom<MojomValue> for MessageHeaderFlags {
        type Error = anyhow::Error;

        fn try_from(value: MojomValue) -> anyhow::Result<Self> {
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

    impl mojom_value_parser::MojomParse for MessageHeaderFlags {
        fn mojom_type() -> MojomType {
            MojomType::UInt32
        }
    }
};
