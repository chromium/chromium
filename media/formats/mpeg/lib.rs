// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub mod bit_reader;
pub mod mpeg_audio_parser;
pub mod parse_adts;
pub mod parse_mp3;

#[derive(Debug, PartialEq)]
pub enum ParserError {
    NeedMoreData,
    InvalidHeader,
}

#[cxx::bridge(namespace = "media::formats::mpeg")]
mod ffi {
    #[derive(Default)]
    enum ActionType {
        #[default]
        NeedMoreData = 1,
        Error = 2,
        Skip = 3,
        Metadata = 4,
        AudioFrame = 5,
    }

    #[derive(Default)]
    struct MpegAudioParserAction {
        action_type: ActionType,
        bytes_to_skip: usize,
        header_info: MpegAudioHeaderInfo,
        partial_frame: bool,
    }

    // We use a combined structure for FFI simplicity even though some fields
    // are AAC or MP3 only since FFI doesn't support composed structures or
    // optional fields unfortunately.
    #[derive(Default, Clone, Debug)]
    struct MpegAudioHeaderInfo {
        frame_size: usize,
        sample_rate: u32,
        channels: u32,
        sample_count: u32,

        // MP3 only
        metadata_frame: bool,

        // AAC only
        esds: u16,
    }

    extern "Rust" {
        fn parse_mp3_header(data: &[u8]) -> MpegAudioHeaderInfo;
        fn parse_adts_header(data: &[u8]) -> MpegAudioHeaderInfo;
        fn parse_mp3_action(data: &[u8]) -> MpegAudioParserAction;
        fn parse_adts_action(data: &[u8]) -> MpegAudioParserAction;
    }
}

fn parse_mp3_header(data: &[u8]) -> ffi::MpegAudioHeaderInfo {
    return parse_mp3::parse_mp3_header_internal(data).unwrap_or_default();
}

fn parse_adts_header(data: &[u8]) -> ffi::MpegAudioHeaderInfo {
    return parse_adts::parse_adts_frame_header_internal(data).unwrap_or_default();
}

fn parse_mp3_action(data: &[u8]) -> ffi::MpegAudioParserAction {
    return mpeg_audio_parser::parse_one_action(
        data,
        mpeg_audio_parser::StartCode::Mp3,
        parse_mp3::parse_mp3_header_internal,
    );
}

fn parse_adts_action(data: &[u8]) -> ffi::MpegAudioParserAction {
    return mpeg_audio_parser::parse_one_action(
        data,
        mpeg_audio_parser::StartCode::Adts,
        parse_adts::parse_adts_frame_header_internal,
    );
}
