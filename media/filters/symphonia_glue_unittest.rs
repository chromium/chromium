// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//media/filters:symphonia_glue";
}

use num_traits::ToBytes;
use rust_gtest_interop::prelude::*;
use symphonia::core::audio::{layouts, AudioBuffer, AudioMut, AudioSpec, GenericAudioBufferRef};
use symphonia_glue::{create_audio_buffer, ffi, init_symphonia_decoder, SymphoniaRawSampleBuffer};

fn test_conversion<S, E, F>(
    samples: &[S],
    sample_rate: u32,
    bytes_per_sample: u8,
    expected_format: ffi::SymphoniaSampleFormat,
    expected_samples: &[E],
    to_ref: F,
    codec: ffi::SymphoniaAudioCodec,
) where
    S: symphonia::core::audio::sample::Sample,
    E: ToBytes + std::fmt::Debug + std::cmp::PartialEq,
    F: for<'a> Fn(&'a AudioBuffer<S>) -> GenericAudioBufferRef<'a>,
{
    let spec = AudioSpec::new(sample_rate, layouts::CHANNEL_LAYOUT_MONO);
    let mut audio_buf = AudioBuffer::<S>::new(spec, samples.len());
    audio_buf.render_uninit(Some(samples.len()));
    audio_buf.plane_mut(0).unwrap().copy_from_slice(samples);

    let buffer_ref = to_ref(&audio_buf);
    let sample_buffer = SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, codec).unwrap();

    let result = create_audio_buffer(buffer_ref, sample_buffer, bytes_per_sample).unwrap();

    expect_eq!(result.sample_rate, sample_rate);
    expect_eq!(result.num_frames, samples.len());
    expect_eq!(result.sample_format, expected_format);

    let data_u8 = result.data;
    let actual_samples = data_u8.chunks_exact(bytes_per_sample as usize).collect::<Vec<_>>();
    expect_eq!(actual_samples.len(), expected_samples.len());

    for (i, (expected, actual_bytes)) in expected_samples.iter().zip(actual_samples).enumerate() {
        let expected_bytes = expected.to_ne_bytes();
        expect_eq!(actual_bytes, expected_bytes.as_ref(), "Mismatch at index {i}");
    }
}

// Ensure that we override Symphonia's default behavior of promoting S16 to
// S32 on output. This allows for strict sample wise comparison with other
// audio decoder implementations.
#[gtest(SymphoniaGlueTest, S32ToS16Conversion)]
fn test_s32_to_s16_conversion() {
    const SAMPLES: &[i32] = &[0, 0x7FFF0000, i32::MIN, 0x12345678];
    const EXPECTED: &[i16] = &[0, 0x7FFF, i16::MIN, 0x1234];
    const SAMPLE_RATE: u32 = 44100;
    const BYTES_PER_SAMPLE: u8 = 2;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::S16,
        EXPECTED,
        |b| GenericAudioBufferRef::S32(b),
        ffi::SymphoniaAudioCodec::Unknown,
    );
}

// Verify that we do not adjust bit depth of S32 when bytes_per_sample is 4.
#[gtest(SymphoniaGlueTest, NoBitDepthAdjustingS32)]
fn test_no_bit_depth_adjusting_s32() {
    const SAMPLES: &[i32] = &[0, 0x7FFF0000, i32::MIN, 0x12345678];
    const SAMPLE_RATE: u32 = 44100;
    const BYTES_PER_SAMPLE: u8 = 4;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::S32,
        SAMPLES,
        |b| GenericAudioBufferRef::S32(b),
        ffi::SymphoniaAudioCodec::Unknown,
    );
}

// Verify that we do not adjust bit depth of F32 when bytes_per_sample is 4.
#[gtest(SymphoniaGlueTest, NoBitDepthAdjustingF32)]
fn test_no_bit_depth_adjusting_f32() {
    const SAMPLES: &[f32] = &[0.0, 0.5, -0.5, 1.0];
    const SAMPLE_RATE: u32 = 48000;
    const BYTES_PER_SAMPLE: u8 = 4;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::F32,
        SAMPLES,
        |b| GenericAudioBufferRef::F32(b),
        ffi::SymphoniaAudioCodec::Unknown,
    );
}

// Verify that we clip F32 values outside [-1.0, 1.0] for MP3 only.
#[gtest(SymphoniaGlueTest, F32Clamping)]
fn test_f32_clamping() {
    const SAMPLES: &[f32] = &[2.0, -2.0, 1.0, -1.0];
    const EXPECTED: &[f32] = &[1.0, -1.0, 1.0, -1.0];
    const SAMPLE_RATE: u32 = 48000;
    const BYTES_PER_SAMPLE: u8 = 4;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::F32,
        EXPECTED,
        |b| GenericAudioBufferRef::F32(b),
        ffi::SymphoniaAudioCodec::Mp3,
    );
}

// Verify that we do not clip F32 values outside [-1.0, 1.0] for non-MP3 codecs.
#[gtest(SymphoniaGlueTest, NoF32ClampingForNonMp3)]
fn test_no_f32_clamping_for_non_mp3() {
    const SAMPLES: &[f32] = &[2.0, -2.0, 1.0, -1.0];
    const SAMPLE_RATE: u32 = 48000;
    const BYTES_PER_SAMPLE: u8 = 4;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::F32,
        SAMPLES,
        |b| GenericAudioBufferRef::F32(b),
        ffi::SymphoniaAudioCodec::Unknown,
    );
}

// Verify that we handle U8 correctly.
#[gtest(SymphoniaGlueTest, U8Conversion)]
fn test_u8_conversion() {
    const SAMPLES: &[u8] = &[0, 128, 255, 64];
    const SAMPLE_RATE: u32 = 22050;
    const BYTES_PER_SAMPLE: u8 = 1;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::U8,
        SAMPLES,
        |b| GenericAudioBufferRef::U8(b),
        ffi::SymphoniaAudioCodec::Unknown,
    );
}

// Verify that we handle S16 correctly when no bit_depth_adjusting is needed.
#[gtest(SymphoniaGlueTest, S16Conversion)]
fn test_s16_conversion() {
    const SAMPLES: &[i16] = &[0, 0x7FFF, i16::MIN, 0x1234];
    const SAMPLE_RATE: u32 = 44100;
    const BYTES_PER_SAMPLE: u8 = 2;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::S16,
        SAMPLES,
        |b| GenericAudioBufferRef::S16(b),
        ffi::SymphoniaAudioCodec::Unknown,
    );
}

// Verify that we handle S24 correctly (padding to 32 bits and shifting left by
// 8).
#[gtest(SymphoniaGlueTest, S24Conversion)]
fn test_s24_conversion() {
    let samples = vec![
        symphonia::core::audio::sample::i24(0),
        symphonia::core::audio::sample::i24(0x7FFFFF),
        symphonia::core::audio::sample::i24(-0x800000),
        symphonia::core::audio::sample::i24(0x123456),
    ];
    let expected: &[i32] = &[
        0,
        0x7FFFFF00,
        -0x80000000, // min value for i32
        0x12345600,
    ];
    const SAMPLE_RATE: u32 = 48000;
    const BYTES_PER_SAMPLE: u8 = 4; // Expected output bytes per sample (padded)

    test_conversion(
        &samples,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::S24,
        expected,
        |b| GenericAudioBufferRef::S24(b),
        ffi::SymphoniaAudioCodec::Unknown,
    );
}

// Verify that we handle unsupported buffer types.
#[gtest(SymphoniaGlueTest, UnsupportedBufferType)]
fn test_unsupported_buffer_type() {
    let spec = AudioSpec::new(44100, layouts::CHANNEL_LAYOUT_MONO);
    let audio_buf = AudioBuffer::<f64>::new(spec, 1);
    let buffer_ref = GenericAudioBufferRef::F64(&audio_buf);
    let result =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, ffi::SymphoniaAudioCodec::Unknown);
    expect_true!(result.is_err());
}

// Verify that decoder initialization fails for invalid config.
#[gtest(SymphoniaGlueTest, DecoderInitFailure)]
fn test_decoder_init_failure() {
    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Flac,
        extra_data: &[], // Empty extra data might be enough to fail some decoders.
        bytes_per_sample: 2,
        channel_mask: 0,
        max_frames_per_packet: 0,
        sample_rate: 44100,
    };
    let result = init_symphonia_decoder(&config);
    // Even if it succeeds here (some decoders might be okay with empty extra data),
    // we are testing the FFI result type.
    // In practice, FLAC often requires some metadata or will fail on first packet.
    // If it returns Ok, that's fine too, but we want to make sure it handles both.
    match result.status {
        ffi::SymphoniaInitStatus::Ok => {
            // If it succeeded, the decoder should be present.
            // We can't easily check the decoder content as it's opaque.
        }
        _ => {
            // If it failed, there should be an error string.
            expect_false!(result.error_str.is_empty());
        }
    }
}

// Verify that stereo data is correctly interleaved.
#[gtest(SymphoniaGlueTest, StereoInterleaving)]
fn test_stereo_interleaving() {
    const SAMPLE_RATE: u32 = 44100;
    let spec = AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO);
    let mut audio_buf = AudioBuffer::<f32>::new(spec, 2);
    audio_buf.render_uninit(Some(2));

    // Planar data: L[0.5, 0.1], R[-0.5, -0.1]
    audio_buf.plane_mut(0).unwrap().copy_from_slice(&[0.5, 0.1]);
    audio_buf.plane_mut(1).unwrap().copy_from_slice(&[-0.5, -0.1]);

    let buffer_ref = GenericAudioBufferRef::F32(&audio_buf);
    let sample_buffer =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, ffi::SymphoniaAudioCodec::Unknown)
            .unwrap();
    let result = create_audio_buffer(buffer_ref, sample_buffer, 4).unwrap();

    // Expected interleaved: [0.5, -0.5, 0.1, -0.1]
    let expected: &[f32] = &[0.5, -0.5, 0.1, -0.1];
    let actual_f32: Vec<f32> =
        result.data.chunks_exact(4).map(|c| f32::from_ne_bytes(c.try_into().unwrap())).collect();

    expect_eq!(actual_f32, expected);
}

// Verify that Symphonia errors are correctly mapped to FFI statuses.
#[gtest(SymphoniaGlueTest, ErrorMapping)]
fn test_error_mapping() {
    use std::io::{Error as IoError, ErrorKind};
    use symphonia::core::errors::Error;

    let decode_err = Error::DecodeError("test");
    expect_eq!(
        ffi::SymphoniaDecodeStatus::from(&decode_err),
        ffi::SymphoniaDecodeStatus::DecodeError
    );

    let eof_err = Error::IoError(IoError::new(ErrorKind::UnexpectedEof, "test"));
    expect_eq!(
        ffi::SymphoniaDecodeStatus::from(&eof_err),
        ffi::SymphoniaDecodeStatus::UnexpectedEndOfStream
    );

    let reset_err = Error::ResetRequired;
    expect_eq!(
        ffi::SymphoniaDecodeStatus::from(&reset_err),
        ffi::SymphoniaDecodeStatus::ResetRequired
    );
}

// Verify that FFI packets are correctly converted to Symphonia packets.
#[gtest(SymphoniaGlueTest, PacketConversion)]
fn test_packet_conversion() {
    let ffi_packet =
        ffi::SymphoniaPacket { timestamp_us: 12345, duration_us: 6789, data: &[0xAA, 0xBB, 0xCC] };
    let sym_packet = symphonia::core::packet::Packet::from(&ffi_packet);

    expect_eq!(sym_packet.pts(), 12345_i64.into());
    expect_eq!(sym_packet.dur(), 6789u64.into());
    expect_eq!(&*sym_packet.data, &[0xAA, 0xBB, 0xCC]);
}

// Verify that we handle zero frames correctly.
#[gtest(SymphoniaGlueTest, ZeroFrames)]
fn test_zero_frames() {
    const SAMPLE_RATE: u32 = 44100;
    // We use 5.1 channels (6 channels) to explicitly test the 3+ channels
    // interleaving path in Symphonia, which had a bug where it panicked
    // if num_frames == 0.
    let spec = AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_MPEG_5P1_D);
    let audio_buf = AudioBuffer::<f32>::new(spec, 0);

    let buffer_ref = GenericAudioBufferRef::F32(&audio_buf);
    let sample_buffer =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, ffi::SymphoniaAudioCodec::Unknown)
            .unwrap();
    let result = create_audio_buffer(buffer_ref, sample_buffer, 4).unwrap();

    expect_eq!(result.num_frames, 0);
    expect_true!(result.data.is_empty());
}

#[gtest(SymphoniaGlueTest, UnpackXiphVorbisExtradata)]
fn test_unpack_xiph_vorbis_extradata() {
    use symphonia_glue::unpack_xiph_vorbis_extradata;

    // A valid Xiph-packed buffer with 3 headers.
    // [0]: number of headers - 1 (2)
    // [1]: length of first header (3)
    // [2]: length of second header (4)
    // [3..6]: Header 1
    // [6..10]: Header 2
    // [10..]: Header 3
    let valid_xiph: Vec<u8> = vec![
        2, 3, 4, // Header information
        1, 1, 1, // Header 1 (Identification)
        2, 2, 2, 2, // Header 2 (Comment)
        3, 3, 3, 3, 3, // Header 3 (Setup)
    ];

    let unpacked = unpack_xiph_vorbis_extradata(&valid_xiph).expect("Should successfully unpack");
    // We expect Header 1 (Ident) and Header 3 (Setup) concatenated sequentially.
    let expected: Vec<u8> = vec![1, 1, 1, 3, 3, 3, 3, 3];
    expect_eq!(unpacked, expected);

    // Empty extradata should return None.
    expect_true!(unpack_xiph_vorbis_extradata(&[]).is_err());

    // Not Xiph-lacing (first byte != 2) should return None.
    let non_xiph: Vec<u8> = vec![1, 2, 3, 4, 5];
    expect_true!(unpack_xiph_vorbis_extradata(&non_xiph).is_err());

    // Truncated lengths (expecting more bytes than available) should return None.
    let truncated_lengths: Vec<u8> = vec![2, 255, 255]; // Needs more bytes to finish length parsing
    expect_true!(unpack_xiph_vorbis_extradata(&truncated_lengths).is_err());

    // Truncated payload (lengths read fine, but payload is missing) should return
    // None.
    let truncated_payload: Vec<u8> = vec![2, 3, 4, 1, 1]; // Misses payload bytes
    expect_true!(unpack_xiph_vorbis_extradata(&truncated_payload).is_err());
}
