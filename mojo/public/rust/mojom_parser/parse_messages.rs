// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Defines functions for parsing entire Mojom messages
//!
//! A Mojom message is structured as a header, followed by a struct, followed
//! (in versions 2+) by a footer of associated interface IDs.

use crate::ast::*;
use crate::errors::*;
use crate::parse_primitives::ParserData;

/// Parse the header of a Mojom message.
/// The format is described in mojo/public/cpp/bindings/lib/bindings_internal.h
// FOR_RELEASE: Actually return something instead of ignoring it.
// FOR_RELEASE: I think the header is literally treated as a mojom struct.
// In the future, we should parse it by defining the appropriate mojom type and
// parsing it. However, that requires us to support versions in general. For
// now, just handle the different possible header versions manually.
fn parse_header(data: &mut ParserData) -> ParsingResult<()> {
    use crate::parse_primitives::*;
    let size_in_bytes = crate::parse_values::parse_size(data)?;
    let version_number = parse_u32(data)?;
    let _interface_id = parse_u32(data)?;
    let _msg_name = parse_u32(data)?;
    let _flags = parse_u32(data)?;
    let _trace_nonce = parse_u32(data)?;
    if version_number == 0 {
        assert!(size_in_bytes == data.bytes_parsed());
        return Ok(());
    }

    let _request_id = parse_u64(data)?;
    if version_number == 1 {
        assert!(size_in_bytes == data.bytes_parsed());
        return Ok(());
    }

    let _payload_ptr = parse_u64(data)?;
    let _interface_ids_ptr = parse_u64(data)?;
    // FOR_RELEASE: validate the payload pointer somehow?
    if version_number == 2 {
        assert!(size_in_bytes == data.bytes_parsed());
        return Ok(());
    }

    let _creation_timeticks_us = parse_u64(data)?;

    if version_number == 3 {
        assert!(size_in_bytes == data.bytes_parsed());
        return Ok(());
    }

    // FOR_RELEASE: We probably want to have a special header validation pass
    // when we get to that.
    panic!("Bad version number")
}

/// Parse an entire mojom message, given the format of the encoded data
// FOR_RELEASE: We'll need to handle associated interface IDs after the message,
// even if we just ignore them. We'll also need to take in more information
// about the message, so we know e.g. the possible message IDs that can appear
// in the header.
pub fn parse_message(data_slice: &[u8], ty: &MojomWireType) -> ParsingResult<MojomValue> {
    let mut data = ParserData::new(data_slice);
    let _ = parse_header(&mut data)?;
    match ty {
        MojomWireType::Pointer {
            nested_data_type:
                PackedStructuredType::Struct { packed_field_names, packed_field_types },
            ..
        } => {
            let ret = crate::parse_values::parse_struct(
                &mut data,
                packed_field_names,
                packed_field_types,
            )?;
            if data.remaining_bytes() != 0 {
                // We don't support the interface ID struct yet
                Err(ParsingError::too_much_data(data.bytes_parsed(), data.remaining_bytes()))
            } else {
                Ok(ret)
            }
        }
        _ => panic!("All message bodies are structs"),
    }
}
