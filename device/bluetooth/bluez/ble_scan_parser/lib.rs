// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod cxx;

use std::pin::Pin;

use crate::cxx::ffi;

// Definitions of the data type flags:
// https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/
const DATA_TYPE_FLAGS: u8 = 0x01;
const DATA_TYPE_SERVICE_UUIDS_16BIT_PARTIAL: u8 = 0x02;
const DATA_TYPE_SERVICE_UUIDS_16BIT_COMPLETE: u8 = 0x03;
const DATA_TYPE_SERVICE_UUIDS_32BIT_PARTIAL: u8 = 0x04;
const DATA_TYPE_SERVICE_UUIDS_32BIT_COMPLETE: u8 = 0x05;
const DATA_TYPE_SERVICE_UUIDS_128BIT_PARTIAL: u8 = 0x06;
const DATA_TYPE_SERVICE_UUIDS_128BIT_COMPLETE: u8 = 0x07;
const DATA_TYPE_LOCAL_NAME_SHORT: u8 = 0x08;
const DATA_TYPE_LOCAL_NAME_COMPLETE: u8 = 0x09;
const DATA_TYPE_TX_POWER_LEVEL: u8 = 0x0A;
const DATA_TYPE_SERVICE_DATA: u8 = 0x16;
const DATA_TYPE_MANUFACTURER_DATA: u8 = 0xFF;

const UUID_PLACEHOLDER: [u8; 16] = [
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB,
];

pub fn parse(advertisement_data: &[u8], record: Pin<&mut ffi::ScanRecord>) -> bool {
    parse_impl(advertisement_data, record).is_some()
}

fn parse_impl<'a>(
    mut advertisement_data: &'a [u8],
    mut record: Pin<&mut ffi::ScanRecord>,
) -> Option<()> {
    // A reference for BLE advertising data:
    // https://community.silabs.com/s/article/kba-bt-0201-bluetooth-advertising-data-basics
    // The data consists of a sequence of lengths and packets, where:
    // - a single byte <length> describes the length of the following packet.
    //   Packets must be at least 2 bytes in length; a packet must contain at least
    //   one byte of data.
    // - a packet starting with a single byte <type> that describes the data type.
    // - and the remainder is the packet data, interpreted according to <type>.

    // TODO(dcheng): The C++ style guide very clearly discourages treating signed
    // types as bitfields, and yet here we are.
    let mut advertising_flags: i8 = -1;
    let mut advertisement_name: &'a [u8] = &[];
    // TODO(dcheng): It's unclear what the correct default value here is, as the
    // original C++ implementation does not bother initializing this.
    let mut tx_power: i8 = 0;

    loop {
        let Some((&length, remainder)) = advertisement_data.split_first() else {
            break;
        };

        let length: usize = length.into();

        if length <= 1 || length > remainder.len() {
            return None;
        }

        let packet;
        (packet, advertisement_data) = remainder.split_at(length);
        // Length must be at least one, so this is always safe.
        let (&data_type, data) = packet.split_first().unwrap();

        match data_type {
            // For flags, additional bytes past the first are silently ignored. The unwrap() is
            // guaranteed to succeed, due to the length <= 1 check above.
            DATA_TYPE_FLAGS => advertising_flags = *data.first().unwrap() as i8,
            // TODO(dcheng): The core supplement says:
            // Two Service or Service Class UUID data types are assigned to each size of
            // Service UUID. One Service or Service Class UUID data type indicates that the
            // Service or Service Class UUID list is incomplete and the other indicates the
            // Service or Service Class UUID list is complete.
            //
            // A packet or data block shall not contain more than one instance for each
            // Service or Service Class UUID data size.
            //
            // But the parser does not seem to consider that.
            DATA_TYPE_SERVICE_UUIDS_16BIT_PARTIAL | DATA_TYPE_SERVICE_UUIDS_16BIT_COMPLETE => {
                parse_service_uuids(data, UuidFormat::With16Bits, |uuid| {
                    ffi::add_service_uuid(record.as_mut(), uuid);
                })?
            }
            DATA_TYPE_SERVICE_UUIDS_32BIT_PARTIAL | DATA_TYPE_SERVICE_UUIDS_32BIT_COMPLETE => {
                parse_service_uuids(data, UuidFormat::With32Bits, |uuid| {
                    ffi::add_service_uuid(record.as_mut(), uuid);
                })?
            }
            DATA_TYPE_SERVICE_UUIDS_128BIT_PARTIAL | DATA_TYPE_SERVICE_UUIDS_128BIT_COMPLETE => {
                parse_service_uuids(data, UuidFormat::With128Bits, |uuid| {
                    ffi::add_service_uuid(record.as_mut(), uuid);
                })?
            }
            DATA_TYPE_LOCAL_NAME_SHORT | DATA_TYPE_LOCAL_NAME_COMPLETE => {
                // TODO(crbug.com/423064072): The Core Specification Supplement, Part A, Section
                // 1.2, states that this should be a valid UTF-8 string. The original C++
                // implementation certainly never bothered to validate, but maybe it's possible
                // to be stricter...
                advertisement_name = data;
            }
            // For TX power, additional bytes past the first are silently ignored. The unwrap() is
            // guaranteed to succeed, due to the length <= 1 check above.
            DATA_TYPE_TX_POWER_LEVEL => tx_power = *data.first().unwrap() as i8,
            DATA_TYPE_SERVICE_DATA => {
                if data.len() < 4 {
                    return None;
                }
                let (uuid, data) = data.split_at(2);
                let uuid = parse_uuid(uuid, UuidFormat::With16Bits)?;
                ffi::add_service_data(record.as_mut(), &uuid, data);
            }
            DATA_TYPE_MANUFACTURER_DATA => {
                if data.len() < 4 {
                    return None;
                }
                // This unwrap() is safe due to the length check above.
                let (key_bytes, data) = data.split_first_chunk::<2>().unwrap();
                let manufacturer_key = u16::from_le_bytes(*key_bytes);
                ffi::add_manufacturer_data(record.as_mut(), manufacturer_key, data);
            }
            // Unknown packet types are silently ignored.
            _ => (),
        }
    }

    ffi::set_advertising_flags(record.as_mut(), advertising_flags);
    ffi::set_tx_power(record.as_mut(), tx_power);
    ffi::set_advertisement_name(record.as_mut(), advertisement_name);
    Some(())
}

use ffi::UuidFormat;

impl UuidFormat {
    fn get_offset_and_len(self) -> (usize, usize) {
        match self {
            UuidFormat::With16Bits => (2, 2),
            UuidFormat::With32Bits => (0, 4),
            UuidFormat::With128Bits => (0, 16),
            _ => unreachable!(),
        }
    }
}

/// Parses
fn parse_service_uuids<F>(bytes: &[u8], format: UuidFormat, mut f: F) -> Option<()>
where
    F: FnMut(&[u8; 16]),
{
    let (_, len) = format.get_offset_and_len();
    let chunks = bytes.chunks_exact(len);
    if !chunks.remainder().is_empty() {
        return None;
    }
    for chunk in chunks {
        let uuid = parse_uuid(chunk, format)?;
        f(&uuid);
    }
    Some(())
}

fn parse_uuid(bytes: &[u8], format: UuidFormat) -> Option<[u8; 16]> {
    let (offset, len) = format.get_offset_and_len();
    if bytes.len() != len {
        return None;
    }
    let mut uuid = UUID_PLACEHOLDER;
    for (src, dst) in bytes.iter().rev().zip(uuid[offset..offset + len].iter_mut()) {
        *dst = *src;
    }
    Some(uuid)
}

fn parse_service_uuids_for_test(
    bytes: &[u8],
    format: UuidFormat,
    mut uuid_list_builder: Pin<&mut ffi::UuidListBuilderForTest>,
) -> bool {
    parse_service_uuids(bytes, format, |uuid| {
        uuid_list_builder.as_mut().add_uuid(uuid);
    })
    .is_some()
}

fn parse_uuid_for_test(bytes: &[u8], format: UuidFormat, out: &mut [u8; 16]) -> bool {
    if let Some(uuid) = parse_uuid(bytes, format) {
        *out = uuid;
        true
    } else {
        false
    }
}
