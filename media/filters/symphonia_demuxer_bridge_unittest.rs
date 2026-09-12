// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//media/filters:symphonia_demuxer_bridge";
}

use rust_gtest_interop::prelude::*;
use symphonia::core::codecs::{audio, video, CodecParameters};
use symphonia::core::formats::probe::Hint;
use symphonia::core::formats::{FormatOptions, SeekMode, SeekTo, Track};
use symphonia::core::io::{MediaSourceStream, MediaSourceStreamOptions};
use symphonia::core::meta::MetadataOptions;
use symphonia::core::units::{Duration, Time, TimeBase};
use symphonia_demuxer_bridge::{
    convert_track_info, ffi, map_codec_params, to_demuxer_init_status, SourceHandle,
};

fn create_test_wav(
    sample_rate: u32,
    channels: u16,
    bits_per_sample: u16,
    num_samples: usize,
) -> Vec<u8> {
    let mut data = Vec::new();
    let bytes_per_sample = bits_per_sample / 8;
    let block_align = channels * bytes_per_sample;
    let byte_rate = sample_rate * block_align as u32;
    let data_len = (num_samples * block_align as usize) as u32;
    let riff_len = 36 + data_len;

    // RIFF header
    data.extend_from_slice(b"RIFF");
    data.extend_from_slice(&riff_len.to_le_bytes());
    data.extend_from_slice(b"WAVE");

    // fmt subchunk
    data.extend_from_slice(b"fmt ");
    data.extend_from_slice(&16u32.to_le_bytes());
    data.extend_from_slice(&1u16.to_le_bytes()); // PCM = 1
    data.extend_from_slice(&channels.to_le_bytes());
    data.extend_from_slice(&sample_rate.to_le_bytes());
    data.extend_from_slice(&byte_rate.to_le_bytes());
    data.extend_from_slice(&block_align.to_le_bytes());
    data.extend_from_slice(&bits_per_sample.to_le_bytes());

    // data subchunk
    data.extend_from_slice(b"data");
    data.extend_from_slice(&data_len.to_le_bytes());
    data.resize(data.len() + data_len as usize, 0x12);

    data
}

#[gtest(SymphoniaDemuxerBridgeTest, MapAudioCodecParamsComprehensive)]
fn test_map_audio_codec_params_comprehensive() {
    use audio::well_known::*;

    let cases = [
        (CODEC_ID_AAC, ffi::SymphoniaCodec::Aac, "aac"),
        (CODEC_ID_FLAC, ffi::SymphoniaCodec::Flac, "flac"),
        (CODEC_ID_MP3, ffi::SymphoniaCodec::Mp3, "mp3"),
        (CODEC_ID_OPUS, ffi::SymphoniaCodec::Opus, "opus"),
        (CODEC_ID_VORBIS, ffi::SymphoniaCodec::Vorbis, "vorbis"),
        (CODEC_ID_PCM_ALAW, ffi::SymphoniaCodec::PcmAlaw, "pcm_alaw"),
        (CODEC_ID_PCM_MULAW, ffi::SymphoniaCodec::PcmMulaw, "pcm_mulaw"),
        (CODEC_ID_PCM_F32LE, ffi::SymphoniaCodec::PcmF32, "pcm_f32le"),
        (CODEC_ID_PCM_F32LE_PLANAR, ffi::SymphoniaCodec::PcmF32Planar, "pcm_f32le_planar"),
        (CODEC_ID_PCM_S16LE, ffi::SymphoniaCodec::PcmS16, "pcm_s16le"),
        (CODEC_ID_PCM_S16BE, ffi::SymphoniaCodec::PcmS16be, "pcm_s16be"),
        (CODEC_ID_PCM_S16LE_PLANAR, ffi::SymphoniaCodec::PcmS16Planar, "pcm_s16le_planar"),
        (CODEC_ID_PCM_S24LE, ffi::SymphoniaCodec::PcmS24, "pcm_s24le"),
        (CODEC_ID_PCM_S24BE, ffi::SymphoniaCodec::PcmS24be, "pcm_s24be"),
        (CODEC_ID_PCM_S32LE, ffi::SymphoniaCodec::PcmS32, "pcm_s32le"),
        (CODEC_ID_PCM_S32LE_PLANAR, ffi::SymphoniaCodec::PcmS32Planar, "pcm_s32le_planar"),
        (CODEC_ID_PCM_U8, ffi::SymphoniaCodec::PcmU8, "pcm_u8"),
        (CODEC_ID_PCM_U8_PLANAR, ffi::SymphoniaCodec::PcmU8Planar, "pcm_u8_planar"),
        (audio::CODEC_ID_NULL_AUDIO, ffi::SymphoniaCodec::Unknown, "unknown_audio"),
    ];

    for (codec_id, expected_codec, expected_name) in cases {
        let params = audio::AudioCodecParameters { codec: codec_id, ..Default::default() };
        let codec_params = CodecParameters::Audio(params);

        let (track_type, codec, name) = map_codec_params(&codec_params);
        expect_eq!(track_type, ffi::SymphoniaTrackType::Audio);
        expect_eq!(codec, expected_codec);
        expect_eq!(name, expected_name);
    }
}

#[gtest(SymphoniaDemuxerBridgeTest, MapVideoCodecParamsComprehensive)]
fn test_map_video_codec_params_comprehensive() {
    use video::well_known::*;

    let cases = [
        (CODEC_ID_AV1, ffi::SymphoniaCodec::Av1, "av1"),
        (CODEC_ID_H264, ffi::SymphoniaCodec::H264, "h264"),
        (CODEC_ID_THEORA, ffi::SymphoniaCodec::Theora, "theora"),
        (CODEC_ID_VP8, ffi::SymphoniaCodec::Vp8, "vp8"),
        (CODEC_ID_VP9, ffi::SymphoniaCodec::Vp9, "vp9"),
        (video::CODEC_ID_NULL_VIDEO, ffi::SymphoniaCodec::Unknown, "unknown_video"),
    ];

    for (codec_id, expected_codec, expected_name) in cases {
        let params = video::VideoCodecParameters { codec: codec_id, ..Default::default() };
        let codec_params = CodecParameters::Video(params);

        let (track_type, codec, name) = map_codec_params(&codec_params);
        expect_eq!(track_type, ffi::SymphoniaTrackType::Video);
        expect_eq!(codec, expected_codec);
        expect_eq!(name, expected_name);
    }
}

#[gtest(SymphoniaDemuxerBridgeTest, ConvertTrackInfoAudio)]
fn test_convert_track_info_audio() {
    use symphonia::core::audio::{Channels, Position};

    let mut track = Track::new(1);
    let channels = Channels::Positioned(Position::FRONT_LEFT | Position::FRONT_RIGHT);
    let channel_mask = match channels {
        Channels::Positioned(pos) => pos.bits() as u32,
        _ => 0,
    };

    let params = audio::AudioCodecParameters {
        codec: audio::well_known::CODEC_ID_VORBIS,
        sample_rate: Some(48000),
        channels: Some(channels),
        bits_per_sample: Some(16),
        extra_data: Some(Box::new([1, 2, 3, 4])),
        ..Default::default()
    };
    track.with_codec_params(CodecParameters::Audio(params));
    track.time_base = TimeBase::try_from_recip(48000);
    track.duration = Some(Duration::from(48000u32));
    track.delay = Some(100);
    track.padding = Some(200);

    let info = convert_track_info(&track);
    expect_eq!(info.track_id, 1);
    expect_eq!(info.track_type, ffi::SymphoniaTrackType::Audio);
    expect_eq!(info.codec, ffi::SymphoniaCodec::Vorbis);
    expect_eq!(info.codec_name, "vorbis");
    expect_eq!(info.sample_rate, 48000);
    expect_eq!(info.channels, 2);
    expect_eq!(info.channel_mask, channel_mask);
    expect_eq!(info.bits_per_sample, 16);
    expect_eq!(info.extra_data, vec![1, 2, 3, 4]);
    expect_eq!(info.delay_frames, 100);
    expect_eq!(info.padding_frames, 200);
    expect_eq!(info.duration_us, 1_000_000);
}

#[gtest(SymphoniaDemuxerBridgeTest, ConvertTrackInfoVideo)]
fn test_convert_track_info_video() {
    let mut track = Track::new(2);
    let extra_data = vec![video::VideoExtraData {
        id: video::VIDEO_EXTRA_DATA_ID_NULL,
        data: Box::new([0xDE, 0xAD, 0xBE, 0xEF]),
    }];
    let params = video::VideoCodecParameters {
        codec: video::well_known::CODEC_ID_AV1,
        width: Some(1920),
        height: Some(1080),
        extra_data,
        ..Default::default()
    };
    track.with_codec_params(CodecParameters::Video(params));
    track.time_base = TimeBase::try_from_recip(60);
    track.duration = Some(Duration::from(300u32));

    let info = convert_track_info(&track);
    expect_eq!(info.track_id, 2);
    expect_eq!(info.track_type, ffi::SymphoniaTrackType::Video);
    expect_eq!(info.codec, ffi::SymphoniaCodec::Av1);
    expect_eq!(info.codec_name, "av1");
    expect_eq!(info.width, 1920);
    expect_eq!(info.height, 1080);
    expect_eq!(info.extra_data, vec![0xDE, 0xAD, 0xBE, 0xEF]);
    expect_eq!(info.duration_us, 5_000_000);
}

#[gtest(SymphoniaDemuxerBridgeTest, ConvertTrackInfoEmpty)]
fn test_convert_track_info_empty() {
    let track = Track::new(3);
    let info = convert_track_info(&track);
    expect_eq!(info.track_id, 3);
    expect_eq!(info.track_type, ffi::SymphoniaTrackType::Unknown);
    expect_eq!(info.codec, ffi::SymphoniaCodec::Unknown);
    expect_eq!(info.codec_name, "unknown");
    expect_eq!(info.duration_us, 0);
    expect_eq!(info.start_time_us, 0);
    expect_eq!(info.sample_rate, 0);
    expect_eq!(info.channels, 0);
}

#[gtest(SymphoniaDemuxerBridgeTest, TimeCalculationHelpers)]
fn test_time_calculation_helpers() {
    use symphonia::core::units::Timestamp;
    use symphonia_demuxer_bridge::{calc_duration_us, calc_timestamp_us};

    let tb = TimeBase::try_from_recip(44100);
    expect_eq!(calc_timestamp_us(tb, Timestamp::new(44100)), 1_000_000);
    expect_eq!(calc_timestamp_us(tb, Timestamp::new(0)), 0);
    expect_eq!(calc_duration_us(tb, 44100), 1_000_000);
    expect_eq!(calc_duration_us(tb, 22050), 500_000);

    // None timebase defaults safely to 0
    expect_eq!(calc_timestamp_us(None, Timestamp::new(12345)), 0);
    expect_eq!(calc_duration_us(None, 12345), 0);
}

#[gtest(SymphoniaDemuxerBridgeTest, ProbeWavContainer)]
fn test_probe_wav_container() {
    let wav_data = create_test_wav(44100, 2, 16, 4410);
    let cursor = std::io::Cursor::new(wav_data);
    let mss = MediaSourceStream::new(Box::new(cursor), MediaSourceStreamOptions::default());

    let mut hint = Hint::new();
    hint.with_extension("wav");

    let probe = symphonia::default::get_probe();
    let format_opts = FormatOptions::default();
    let meta_opts = MetadataOptions::default();

    let probed = probe.probe(&hint, mss, format_opts, meta_opts);
    expect_true!(probed.is_ok());

    let mut reader = probed.unwrap();
    expect_eq!(reader.format_info().short_name, "wave");

    let track_info = {
        let tracks = reader.tracks();
        expect_eq!(tracks.len(), 1);
        convert_track_info(&tracks[0])
    };
    let target_track_id = track_info.track_id;

    expect_eq!(track_info.track_type, ffi::SymphoniaTrackType::Audio);
    expect_eq!(track_info.codec, ffi::SymphoniaCodec::PcmS16);
    expect_eq!(track_info.sample_rate, 44100);
    expect_eq!(track_info.channels, 2);

    let packet = reader.next_packet();
    expect_true!(packet.is_ok());
    let packet_opt = packet.unwrap();
    expect_true!(packet_opt.is_some());
    let packet = packet_opt.unwrap();
    expect_eq!(packet.track_id, target_track_id);
    expect_false!(packet.data.is_empty());

    let seek_result = reader.seek(
        SeekMode::Accurate,
        SeekTo::Time { time: Time::from_micros(0), track_id: Some(target_track_id) },
    );
    expect_true!(seek_result.is_ok());
}

#[gtest(SymphoniaDemuxerBridgeTest, ProbeEmptyStream)]
fn test_probe_empty_stream() {
    let cursor = std::io::Cursor::new(Vec::<u8>::new());
    let mss = MediaSourceStream::new(Box::new(cursor), MediaSourceStreamOptions::default());

    let hint = Hint::new();
    let probe = symphonia::default::get_probe();
    let format_opts = FormatOptions::default();
    let meta_opts = MetadataOptions::default();

    let probed = probe.probe(&hint, mss, format_opts, meta_opts);
    expect_false!(probed.is_ok());
}

#[gtest(SymphoniaDemuxerBridgeTest, ErrorMapping)]
fn test_error_mapping() {
    use symphonia::core::errors::Error;

    let err = Error::Unsupported("unsupported");
    let (status, msg) = to_demuxer_init_status(&err);
    expect_eq!(status, ffi::SymphoniaDemuxerInitStatus::UnsupportedFormat);
    expect_true!(msg.contains("unsupported"));

    let err = Error::DecodeError("corrupt");
    let (status, _) = to_demuxer_init_status(&err);
    expect_eq!(status, ffi::SymphoniaDemuxerInitStatus::DecodeError);

    let err = Error::IoError(std::io::Error::from(std::io::ErrorKind::UnexpectedEof));
    let (status, _) = to_demuxer_init_status(&err);
    expect_eq!(status, ffi::SymphoniaDemuxerInitStatus::ReadError);

    let err = Error::LimitError("too large");
    let (status, _) = to_demuxer_init_status(&err);
    expect_eq!(status, ffi::SymphoniaDemuxerInitStatus::LimitError);

    let err = Error::ResetRequired;
    let (status, _) = to_demuxer_init_status(&err);
    expect_eq!(status, ffi::SymphoniaDemuxerInitStatus::GenericError);

    let err = Error::SeekError(symphonia::core::errors::SeekErrorKind::OutOfRange);
    let (status, _) = to_demuxer_init_status(&err);
    expect_eq!(status, ffi::SymphoniaDemuxerInitStatus::GenericError);
}

#[gtest(SymphoniaDemuxerBridgeTest, ProbeWavMultiPacketAndSeek)]
fn test_probe_wav_multi_packet_and_seek() {
    // 2 seconds of 44.1kHz 16-bit stereo PCM audio (88,200 samples)
    let wav_data = create_test_wav(44100, 2, 16, 88200);
    let cursor = std::io::Cursor::new(wav_data);
    let mss = MediaSourceStream::new(Box::new(cursor), MediaSourceStreamOptions::default());

    let mut hint = Hint::new();
    hint.with_extension("wav");

    let probe = symphonia::default::get_probe();
    let probed = probe.probe(&hint, mss, FormatOptions::default(), MetadataOptions::default());
    expect_true!(probed.is_ok());

    let mut reader = probed.unwrap();
    let track_id = reader.tracks()[0].id;
    let timebase = reader.tracks()[0].time_base.unwrap();

    // Read the first packet
    let packet1 = reader.next_packet().expect("Read packet 1 failed").expect("Expected packet 1");
    let packet1_micros = timebase.calc_time_saturating(packet1.pts).as_micros();
    expect_eq!(packet1_micros, 0);

    // Seek to 1 second (1,000,000 microseconds)
    let seek_result = reader.seek(
        SeekMode::Accurate,
        SeekTo::Time { time: Time::from_micros(1_000_000), track_id: Some(track_id) },
    );
    expect_true!(seek_result.is_ok());
    let seeked_to = seek_result.unwrap();
    let seeked_micros = timebase.calc_time_saturating(seeked_to.actual_ts).as_micros();
    // Seek lands on the nearest preceding packet boundary <= 1,000,000 us.
    expect_le!(seeked_micros, 1_000_000);
    expect_gt!(seeked_micros, 950_000);

    // Read packet after seek and verify it begins at the seeked timestamp
    let packet_after_seek =
        reader.next_packet().expect("Read post-seek failed").expect("Expected packet");
    let post_seek_micros = timebase.calc_time_saturating(packet_after_seek.pts).as_micros();
    expect_eq!(post_seek_micros, seeked_micros);
}

#[gtest(SymphoniaDemuxerBridgeTest, SourceHandleUnconnectedAndReset)]
fn test_source_handle_unconnected_and_reset() {
    let handle = SourceHandle::new();
    let res = handle.with_bridge(|_| Ok(()));
    expect_true!(res.is_err());
    expect_eq!(res.unwrap_err().kind(), std::io::ErrorKind::NotConnected);

    handle.reset();
    let res2 = handle.with_bridge(|_| Ok(()));
    expect_true!(res2.is_err());
    expect_eq!(res2.unwrap_err().kind(), std::io::ErrorKind::NotConnected);
}
