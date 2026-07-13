// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ffi::{ActionType, MpegAudioHeaderInfo, MpegAudioParserAction};
use crate::ParserError;
use std::cmp;

/// StartCode is used to find the start of each frame header. Also referred to
/// as the sync code in the MP3 and ADTS header specifications.
#[derive(Clone, Copy)]
#[repr(u32)]
pub enum StartCode {
    Adts = 0xfff00000,
    Mp3 = 0xffe00000,
}

const ICY_START_CODE: u32 = u32::from_be_bytes(*b"ICY ");
const ID3_START_CODE_MASK: u32 = 0xffffff00;
const ID3V1_START_CODE: u32 = u32::from_be_bytes(*b"TAG\0");
const ID3V2_START_CODE: u32 = u32::from_be_bytes(*b"ID3\0");

type ParseFn = fn(&[u8]) -> Result<MpegAudioHeaderInfo, ParserError>;

/// Inspects the start of `data` to determine the next parsing action.
///
/// This function identifies whether the current parser position is at:
/// 1. An audio frame header (indicated by matching `start_code`).
/// 2. An Icecast metadata header ("ICY ").
/// 3. An ID3v1 metadata tag ("TAG").
/// 4. An ID3v2 metadata tag ("ID3").
///
/// If the data does not match any of these headers, it scans forward using
/// `find_next_start_code_action` to locate the next potential audio frame.
///
/// # Arguments
/// * `data` - The current buffer of stream bytes.
/// * `start_code` - The expected start code type (ADTS or MP3) of the stream.
/// * `parse_fn` - Parser function for the specific codec header format.
pub fn parse_one_action(
    data: &[u8],
    start_code: StartCode,
    parse_fn: ParseFn,
) -> MpegAudioParserAction {
    let mut result = MpegAudioParserAction::default();
    if data.len() < 4 {
        result.action_type = ActionType::NeedMoreData;
        return result;
    }

    let raw_start_code = u32::from_be_bytes(data[..4].try_into().unwrap());
    let start_code_mask = start_code as u32;

    if (raw_start_code & start_code_mask) == start_code_mask {
        return parse_frame_action(data, parse_fn);
    } else if raw_start_code == ICY_START_CODE {
        return parse_icecast_action(data);
    } else if (raw_start_code & ID3_START_CODE_MASK) == ID3V1_START_CODE {
        return parse_id3v1_action(data);
    } else if (raw_start_code & ID3_START_CODE_MASK) == ID3V2_START_CODE {
        return parse_id3v2_action(data);
    } else {
        return find_next_start_code_action(data, parse_fn);
    }
}

fn parse_frame_action(data: &[u8], parse_fn: ParseFn) -> MpegAudioParserAction {
    let mut result = MpegAudioParserAction::default();
    match parse_fn(data) {
        Ok(info) => {
            if data.len() < info.frame_size {
                result.action_type = ActionType::NeedMoreData;
                result.partial_frame = true;
                return result;
            }
            result.action_type = ActionType::AudioFrame;
            result.header_info = info;
        }
        Err(err) => {
            if err == ParserError::NeedMoreData {
                result.action_type = ActionType::NeedMoreData;
                result.partial_frame = true;
            } else {
                result.action_type = ActionType::Error;
            }
        }
    }
    return result;
}

fn parse_icecast_action(data: &[u8]) -> MpegAudioParserAction {
    // Arbitrary upper bound on the size of an IceCast header.
    const MAX_ICECAST_HEADER_SIZE: usize = 4096;
    const ICY_START_CODE_LEN: usize = std::mem::size_of_val(&ICY_START_CODE);

    let mut result = MpegAudioParserAction::default();
    let locate_size = cmp::min(data.len(), MAX_ICECAST_HEADER_SIZE);
    match locate_end_of_headers(&data[ICY_START_CODE_LEN..locate_size]) {
        Some(offset) => {
            result.action_type = ActionType::Metadata;
            result.bytes_to_skip = ICY_START_CODE_LEN + offset;
        }
        None => {
            if locate_size == MAX_ICECAST_HEADER_SIZE {
                result.action_type = ActionType::Error;
            } else {
                result.action_type = ActionType::NeedMoreData;
            }
        }
    }
    return result;
}

fn locate_end_of_headers(buf: &[u8]) -> Option<usize> {
    let mut was_lf = false;
    let mut last_b = 0;
    for (i, &b) in buf.iter().enumerate() {
        if b == b'\n' {
            if was_lf {
                return Some(i + 1);
            }
            was_lf = true;
        } else if b != b'\r' || last_b != b'\n' {
            was_lf = false;
        }
        last_b = b;
    }
    return None;
}

fn parse_id3v1_action(data: &[u8]) -> MpegAudioParserAction {
    debug_assert!(data.len() >= 4);
    const ID3V1_SIZE: usize = 128;
    const ID3V1_EXTENDED_SIZE: usize = 227;

    let mut result = MpegAudioParserAction::default();
    let needed_size = if &data[..4] == b"TAG+" { ID3V1_EXTENDED_SIZE } else { ID3V1_SIZE };
    if data.len() < needed_size {
        result.action_type = ActionType::NeedMoreData;
    } else {
        result.action_type = ActionType::Metadata;
        result.bytes_to_skip = needed_size;
    }
    return result;
}

fn parse_id3v2_action(data: &[u8]) -> MpegAudioParserAction {
    let mut result = MpegAudioParserAction::default();
    if data.len() < 10 {
        result.action_type = ActionType::NeedMoreData;
        return result;
    }

    let flags = data[5];
    let tag_size = parse_syncsafe_int(data[6..10].try_into().unwrap()).map(|id3_size| {
        let mut actual_tag_size = 10 + id3_size;
        // Bit 4 (0x10) indicates that a 10-byte footer is appended to the tag.
        if (flags & 0x10) != 0 {
            actual_tag_size += 10;
        }
        return actual_tag_size;
    });

    match tag_size {
        Some(actual_tag_size) => {
            if data.len() < actual_tag_size {
                result.action_type = ActionType::NeedMoreData;
            } else {
                result.action_type = ActionType::Metadata;
                result.bytes_to_skip = actual_tag_size;
            }
        }
        None => {
            result.action_type = ActionType::Error;
        }
    }
    return result;
}

fn parse_syncsafe_int(bytes: [u8; 4]) -> Option<usize> {
    let mut value = 0;
    for b in bytes {
        if (b & 0x80) != 0 {
            return None;
        }
        value = (value << 7) | (b as usize);
    }
    return Some(value);
}

fn find_next_start_code_action(data: &[u8], parse_fn: ParseFn) -> MpegAudioParserAction {
    let mut result = MpegAudioParserAction::default();
    let mut start = 0;
    while let Some(pos) = data[start..].iter().position(|&x| x == 0xff) {
        let candidate = start + pos;
        let mut current_sync = candidate;
        let mut parse_failed = false;

        for _ in 0..3 {
            match parse_fn(&data[current_sync..]) {
                Ok(info) => {
                    debug_assert!(info.frame_size > 0);
                    current_sync += info.frame_size;
                    if current_sync >= data.len() {
                        result.action_type = ActionType::NeedMoreData;
                        return result;
                    }
                }
                Err(ParserError::NeedMoreData) => {
                    result.action_type = ActionType::NeedMoreData;
                    return result;
                }
                Err(_) => {
                    parse_failed = true;
                    break;
                }
            }
        }

        if !parse_failed {
            result.action_type = ActionType::Skip;
            result.bytes_to_skip = candidate;
            return result;
        }

        start = candidate + 1;
    }

    result.action_type = ActionType::NeedMoreData;
    return result;
}
