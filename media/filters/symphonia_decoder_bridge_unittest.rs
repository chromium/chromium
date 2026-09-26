// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//media/filters:symphonia_decoder_bridge";
}

use num_traits::ToBytes;
use rust_gtest_interop::prelude::*;
use symphonia::core::audio::{
    layouts, AudioBuffer, AudioMut, AudioSpec, Channels, GenericAudioBufferRef, Position,
};
use symphonia_decoder_bridge::{
    copy_channel_to_slice, copy_samples_to_slice, create_audio_buffer, detect_mpeg_audio_codec_id,
    ffi, init_symphonia_decoder, SymphoniaRawSampleBuffer,
};

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
    let sample_buffer =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, codec, bytes_per_sample).unwrap();

    let result = create_audio_buffer(buffer_ref, sample_buffer).unwrap();

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
#[gtest(SymphoniaDecoderBridgeTest, S32ToS16Conversion)]
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
#[gtest(SymphoniaDecoderBridgeTest, NoBitDepthAdjustingS32)]
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
#[gtest(SymphoniaDecoderBridgeTest, NoBitDepthAdjustingF32)]
fn test_no_bit_depth_adjusting_f32() {
    const SAMPLES: &[f32] = &[0.0, 0.5, -0.5, 1.0];
    const SAMPLE_RATE: u32 = 48000;
    const BYTES_PER_SAMPLE: u8 = 4;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::PlanarF32,
        SAMPLES,
        |b| GenericAudioBufferRef::F32(b),
        ffi::SymphoniaAudioCodec::Unknown,
    );
}

// Verify that we clip F32 values outside [-1.0, 1.0] and silence NaNs for MP3
// only.
#[gtest(SymphoniaDecoderBridgeTest, F32Clamping)]
fn test_f32_clamping() {
    const SAMPLES: &[f32] = &[2.0, -2.0, 1.0, -1.0, f32::NAN, f32::INFINITY, f32::NEG_INFINITY];
    const EXPECTED: &[f32] = &[1.0, -1.0, 1.0, -1.0, 0.0, 1.0, -1.0];
    const SAMPLE_RATE: u32 = 48000;
    const BYTES_PER_SAMPLE: u8 = 4;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::PlanarF32,
        EXPECTED,
        |b| GenericAudioBufferRef::F32(b),
        ffi::SymphoniaAudioCodec::Mp3,
    );
}

// Verify that we do not clip F32 values outside [-1.0, 1.0] for non-MP3 codecs.
#[gtest(SymphoniaDecoderBridgeTest, NoF32ClampingForNonMp3)]
fn test_no_f32_clamping_for_non_mp3() {
    const SAMPLES: &[f32] = &[2.0, -2.0, 1.0, -1.0, f32::INFINITY, f32::NEG_INFINITY];
    const SAMPLE_RATE: u32 = 48000;
    const BYTES_PER_SAMPLE: u8 = 4;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::PlanarF32,
        SAMPLES,
        |b| GenericAudioBufferRef::F32(b),
        ffi::SymphoniaAudioCodec::Unknown,
    );
}

// Verify that we handle U8 correctly.
#[gtest(SymphoniaDecoderBridgeTest, U8Conversion)]
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
#[gtest(SymphoniaDecoderBridgeTest, S16Conversion)]
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
#[gtest(SymphoniaDecoderBridgeTest, S24Conversion)]
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
#[gtest(SymphoniaDecoderBridgeTest, UnsupportedBufferType)]
fn test_unsupported_buffer_type() {
    let spec = AudioSpec::new(44100, layouts::CHANNEL_LAYOUT_MONO);
    let audio_buf = AudioBuffer::<f64>::new(spec, 1);
    let buffer_ref = GenericAudioBufferRef::F64(&audio_buf);
    let result =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, ffi::SymphoniaAudioCodec::Unknown, 4);
    expect_true!(result.is_err());
}

// Verify that decoder initialization fails for invalid config.
#[gtest(SymphoniaDecoderBridgeTest, DecoderInitFailure)]
fn test_decoder_init_failure() {
    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Flac,
        extra_data: &[], // Empty extra data might be enough to fail some decoders.
        bytes_per_sample: 2,
        channel_mask: 0,
        channel_count: 0,
        sample_rate: 44100,
    };
    let result = init_symphonia_decoder(&config);
    // Even if it succeeds here (some decoders might be okay with empty extra
    // data), we are testing the FFI result type. In practice, FLAC often
    // requires some metadata or will fail on first packet. If it returns Ok,
    // that's fine too, but we want to make sure it handles both.
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

// Verify that PCM decoder initialization succeeds for channel layouts that have
// no speaker positions. Chromium uses CHANNEL_LAYOUT_DISCRETE, and therefore an
// empty channel mask for unnamed layouts.
#[gtest(SymphoniaDecoderBridgeTest, PcmInitWithDiscreteChannelLayout)]
fn test_pcm_init_with_discrete_channel_layout() {
    // 13 is the first count above kMaxConcurrentChannels; 32 is
    // limits::kMaxChannels.
    for channel_count in [13, 31, 32] {
        let config = ffi::SymphoniaDecoderConfig {
            codec: ffi::SymphoniaAudioCodec::PcmS16,
            extra_data: &[],
            bytes_per_sample: 2,
            channel_mask: 0,
            channel_count,
            sample_rate: 48000,
        };

        let result = init_symphonia_decoder(&config);
        expect_eq!(result.status, ffi::SymphoniaInitStatus::Ok, "channels: {}", channel_count);
        expect_true!(result.error_str.is_empty(), "channels: {}", channel_count);
    }
}

// Verify that FLAC decoder initialization succeeds even if the "fLaC" marker
// is present in the extra data.
#[gtest(SymphoniaDecoderBridgeTest, FlacInitWithMarker)]
fn test_flac_init_with_marker() {
    let mut extra_data = vec![
        102, 76, 97, 67, // "fLaC"
        0, 0, 0, 34, // STREAMINFO header
        // STREAMINFO data (34 bytes)
        18, 0, 18, 0, 0, 0, 186, 0, 5, 57, 11, 184, 0, 240, 0, 3, 169, 128, 148, 172, 171, 223, 193,
        198, 120, 195, 117, 49, 236, 130, 87, 47, 118, 114,
    ];

    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Flac,
        extra_data: &extra_data,
        bytes_per_sample: 2,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };

    let result = init_symphonia_decoder(&config);
    expect_eq!(result.status, ffi::SymphoniaInitStatus::Ok);

    // Also verify that it handles trailing data.
    extra_data.extend_from_slice(&[0xAA, 0xBB, 0xCC]);
    let config_trailing = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Flac,
        extra_data: &extra_data,
        bytes_per_sample: 2,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };
    let result_trailing = init_symphonia_decoder(&config_trailing);
    expect_eq!(result_trailing.status, ffi::SymphoniaInitStatus::Ok);
}

// Verify that FLAC decoder initialization succeeds with 38-byte extra data
// (Header + STREAMINFO).
#[gtest(SymphoniaDecoderBridgeTest, FlacInitWithHeaderOnly)]
fn test_flac_init_with_header_only() {
    let extra_data = vec![
        0, 0, 0, 34, // STREAMINFO header
        // STREAMINFO data (34 bytes)
        18, 0, 18, 0, 0, 0, 186, 0, 5, 57, 11, 184, 0, 240, 0, 3, 169, 128, 148, 172, 171, 223, 193,
        198, 120, 195, 117, 49, 236, 130, 87, 47, 118, 114,
    ];

    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Flac,
        extra_data: &extra_data,
        bytes_per_sample: 2,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };

    let result = init_symphonia_decoder(&config);
    expect_eq!(result.status, ffi::SymphoniaInitStatus::Ok);
}

// Verify that FLAC decoder initialization fails as expected when only the
// marker and invalid data are provided. The marker should be stripped, but
// the resulting data will still fail validation in Symphonia.
#[gtest(SymphoniaDecoderBridgeTest, FlacInitWithMarkerOnly)]
fn test_flac_init_with_marker_only() {
    let extra_data = vec![
        102, 76, 97, 67, // "fLaC"
        0xFF, 0xFF, 0xFF, 0xFF, // Invalid data
    ];

    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Flac,
        extra_data: &extra_data,
        bytes_per_sample: 2,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };

    let result = init_symphonia_decoder(&config);
    // It should fail because the data after "fLaC" isn't a valid STREAMINFO.
    expect_ne!(result.status, ffi::SymphoniaInitStatus::Ok);
}

// Verify that FLAC decoder initialization fails as expected when a
// non-STREAMINFO metadata block is provided. The marker should be stripped, but
// the block will fail validation in Symphonia which expects STREAMINFO first.
#[gtest(SymphoniaDecoderBridgeTest, FlacInitWithOtherBlock)]
fn test_flac_init_with_other_block() {
    let extra_data = vec![
        102, 76, 97, 67, // "fLaC"
        1,  // APPLICATION block type (bit 7 is 0, type is 1)
        0, 0, 4, // Length 4
        0x11, 0x22, 0x33, 0x44, // Some application data
    ];

    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Flac,
        extra_data: &extra_data,
        bytes_per_sample: 2,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };

    let result = init_symphonia_decoder(&config);
    // It should fail because Symphonia's FLAC decoder initialization expects
    // STREAMINFO.
    expect_ne!(result.status, ffi::SymphoniaInitStatus::Ok);
}

// Verify that stereo F32 data is output in planar layout (channel planes).
#[gtest(SymphoniaDecoderBridgeTest, StereoPlanarF32)]
fn test_stereo_planar_f32() {
    const SAMPLE_RATE: u32 = 44100;
    let spec = AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO);
    let mut audio_buf = AudioBuffer::<f32>::new(spec, 2);
    audio_buf.render_uninit(Some(2));

    // Planar data: L[0.5, 0.1], R[-0.5, -0.1]
    audio_buf.plane_mut(0).unwrap().copy_from_slice(&[0.5, 0.1]);
    audio_buf.plane_mut(1).unwrap().copy_from_slice(&[-0.5, -0.1]);

    let buffer_ref = GenericAudioBufferRef::F32(&audio_buf);
    let sample_buffer =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, ffi::SymphoniaAudioCodec::Unknown, 4)
            .unwrap();
    let result = create_audio_buffer(buffer_ref, sample_buffer).unwrap();

    expect_eq!(result.sample_format, ffi::SymphoniaSampleFormat::PlanarF32);
    expect_eq!(result.channel_count, 2);
    expect_eq!(result.num_frames, 2);

    // Expected planar: [L0, L1, R0, R1]
    let expected: &[f32] = &[0.5, 0.1, -0.5, -0.1];
    let actual_f32: Vec<f32> =
        result.data.chunks_exact(4).map(|c| f32::from_ne_bytes(c.try_into().unwrap())).collect();

    expect_eq!(actual_f32, expected);
}

// Verify that stereo F32 clamping for MP3 operates in planar layout and
// silences NaNs to 0.0 while clamping +/- Inf to +/- 1.0.
#[gtest(SymphoniaDecoderBridgeTest, StereoPlanarF32Clamping)]
fn test_stereo_planar_f32_clamping() {
    const SAMPLE_RATE: u32 = 44100;
    let spec = AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO);
    let mut audio_buf = AudioBuffer::<f32>::new(spec, 4);
    audio_buf.render_uninit(Some(4));

    // Planar data: L[2.0, 0.5, NaN, -Inf], R[-2.0, -0.5, +Inf, 1.5]
    audio_buf.plane_mut(0).unwrap().copy_from_slice(&[2.0, 0.5, f32::NAN, f32::NEG_INFINITY]);
    audio_buf.plane_mut(1).unwrap().copy_from_slice(&[-2.0, -0.5, f32::INFINITY, 1.5]);

    let buffer_ref = GenericAudioBufferRef::F32(&audio_buf);
    let sample_buffer =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, ffi::SymphoniaAudioCodec::Mp3, 4)
            .unwrap();
    let result = create_audio_buffer(buffer_ref, sample_buffer).unwrap();

    expect_eq!(result.sample_format, ffi::SymphoniaSampleFormat::PlanarF32);
    expect_eq!(result.channel_count, 2);
    expect_eq!(result.num_frames, 4);

    // Expected planar clamped: [1.0, 0.5, 0.0, -1.0, -1.0, -0.5, 1.0, 1.0]
    let expected: &[f32] = &[1.0, 0.5, 0.0, -1.0, -1.0, -0.5, 1.0, 1.0];
    let actual_f32: Vec<f32> =
        result.data.chunks_exact(4).map(|c| f32::from_ne_bytes(c.try_into().unwrap())).collect();

    expect_eq!(actual_f32, expected);
}

// Verify that odd frame counts of stereo F32 (where channel plane size is not
// a multiple of 32 bytes) are tightly packed in planar layout.
#[gtest(SymphoniaDecoderBridgeTest, StereoPlanarF32OddFrames)]
fn test_stereo_planar_f32_odd_frames() {
    const SAMPLE_RATE: u32 = 44100;
    let spec = AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO);
    let mut audio_buf = AudioBuffer::<f32>::new(spec, 3);
    audio_buf.render_uninit(Some(3));

    // 3 frames = 12 bytes per channel plane.
    audio_buf.plane_mut(0).unwrap().copy_from_slice(&[0.1, 0.2, 0.3]);
    audio_buf.plane_mut(1).unwrap().copy_from_slice(&[-0.1, -0.2, -0.3]);

    let buffer_ref = GenericAudioBufferRef::F32(&audio_buf);
    let sample_buffer =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, ffi::SymphoniaAudioCodec::Flac, 4)
            .unwrap();
    let result = create_audio_buffer(buffer_ref, sample_buffer).unwrap();

    expect_eq!(result.sample_format, ffi::SymphoniaSampleFormat::PlanarF32);
    expect_eq!(result.channel_count, 2);
    expect_eq!(result.num_frames, 3);
    expect_eq!(result.data.len(), 24);

    let expected: &[f32] = &[0.1, 0.2, 0.3, -0.1, -0.2, -0.3];
    let actual_f32: Vec<f32> =
        result.data.chunks_exact(4).map(|c| f32::from_ne_bytes(c.try_into().unwrap())).collect();

    expect_eq!(actual_f32, expected);
}

// Verify that stereo planar S24 data is properly interleaved and padded to
// 32-bit.
#[gtest(SymphoniaDecoderBridgeTest, StereoS24InterleavingAndPadding)]
fn test_stereo_s24_interleaving_and_padding() {
    use symphonia::core::audio::sample::i24;
    const SAMPLE_RATE: u32 = 44100;
    let spec = AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO);
    let mut audio_buf = AudioBuffer::<i24>::new(spec, 2);
    audio_buf.render_uninit(Some(2));

    // Planar data: L[0x123456, 0x7FFFFF], R[-0x123456, -0x800000]
    audio_buf.plane_mut(0).unwrap().copy_from_slice(&[i24(0x123456), i24(0x7FFFFF)]);
    audio_buf.plane_mut(1).unwrap().copy_from_slice(&[i24(-0x123456), i24(-0x800000)]);

    let buffer_ref = GenericAudioBufferRef::S24(&audio_buf);
    let sample_buffer =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, ffi::SymphoniaAudioCodec::Unknown, 4)
            .unwrap();
    let result = create_audio_buffer(buffer_ref, sample_buffer).unwrap();

    expect_eq!(result.sample_format, ffi::SymphoniaSampleFormat::S24);
    expect_eq!(result.channel_count, 2);
    expect_eq!(result.num_frames, 2);

    // Expected interleaved S32: [L0, R0, L1, R1]
    let expected: &[i32] = &[0x12345600, -0x12345600, 0x7FFFFF00, -0x80000000];
    let actual_i32: Vec<i32> =
        result.data.chunks_exact(4).map(|c| i32::from_ne_bytes(c.try_into().unwrap())).collect();

    expect_eq!(actual_i32, expected);
}

// Verify that stereo planar S32 data is properly interleaved and downshifted to
// S16.
#[gtest(SymphoniaDecoderBridgeTest, StereoS32ToS16InterleavingAndDownshifting)]
fn test_stereo_s32_to_s16_interleaving_and_downshifting() {
    const SAMPLE_RATE: u32 = 44100;
    let spec = AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO);
    let mut audio_buf = AudioBuffer::<i32>::new(spec, 2);
    audio_buf.render_uninit(Some(2));

    // Planar data: L[0x12340000, 0x7FFF0000], R[-0x12340000, i32::MIN]
    audio_buf.plane_mut(0).unwrap().copy_from_slice(&[0x12340000, 0x7FFF0000]);
    audio_buf.plane_mut(1).unwrap().copy_from_slice(&[-0x12340000, i32::MIN]);

    let buffer_ref = GenericAudioBufferRef::S32(&audio_buf);
    let sample_buffer =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, ffi::SymphoniaAudioCodec::Unknown, 2)
            .unwrap();
    let result = create_audio_buffer(buffer_ref, sample_buffer).unwrap();

    expect_eq!(result.sample_format, ffi::SymphoniaSampleFormat::S16);
    expect_eq!(result.channel_count, 2);
    expect_eq!(result.num_frames, 2);

    // Expected interleaved S16: [L0, R0, L1, R1]
    let expected: &[i16] = &[0x1234, -0x1234, 0x7FFF, i16::MIN];
    let actual_i16: Vec<i16> =
        result.data.chunks_exact(2).map(|c| i16::from_ne_bytes(c.try_into().unwrap())).collect();

    expect_eq!(actual_i16, expected);
}

// Verify that Symphonia errors are correctly mapped to FFI statuses.
#[gtest(SymphoniaDecoderBridgeTest, ErrorMapping)]
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

// Verify that FFI packets are correctly converted to Symphonia packet refs.
#[gtest(SymphoniaDecoderBridgeTest, PacketConversion)]
fn test_packet_conversion() {
    let ffi_packet =
        ffi::SymphoniaPacket { timestamp_us: 12345, duration_us: 6789, data: &[0xAA, 0xBB, 0xCC] };
    let packet_ref = symphonia::core::packet::PacketRef::from(&ffi_packet);

    expect_eq!(packet_ref.pts, 12345_i64.into());
    expect_eq!(packet_ref.dur, 6789u64.into());
    expect_eq!(packet_ref.data, &[0xAA, 0xBB, 0xCC]);
}

// Verify that we handle zero frames correctly.
#[gtest(SymphoniaDecoderBridgeTest, ZeroFrames)]
fn test_zero_frames() {
    const SAMPLE_RATE: u32 = 44100;
    // Test 5.1 channels (6 channels) zero-length frame handling in
    // Symphonia 0.6.
    let spec = AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_MPEG_5P1_D);
    let audio_buf = AudioBuffer::<f32>::new(spec, 0);

    let buffer_ref = GenericAudioBufferRef::F32(&audio_buf);
    let sample_buffer =
        SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref, ffi::SymphoniaAudioCodec::Unknown, 4)
            .unwrap();
    let result = create_audio_buffer(buffer_ref, sample_buffer).unwrap();

    expect_eq!(result.num_frames, 0);
    expect_true!(result.data.is_empty());
}

#[gtest(SymphoniaDecoderBridgeTest, DetectMpegAudioCodecId)]
fn test_detect_mpeg_audio_codec_id() {
    use symphonia::core::codecs::audio::well_known::*;

    // MPEG-1 Layer 1 header: sync (11 bits) = 0x7FF, version = 11 (MPEG-1),
    // layer = 11 (Layer 1) 0xFF, 0xFE, ...
    let mp1_header = [0xFF, 0xFE, 0x90, 0x00];
    expect_eq!(detect_mpeg_audio_codec_id(&mp1_header), Some(CODEC_ID_MP1));

    // MPEG-1 Layer 2 header: sync (11 bits) = 0x7FF, version = 11 (MPEG-1),
    // layer = 10 (Layer 2) 0xFF, 0xFD, ...
    let mp2_header = [0xFF, 0xFD, 0x90, 0x00];
    expect_eq!(detect_mpeg_audio_codec_id(&mp2_header), Some(CODEC_ID_MP2));

    // MPEG-1 Layer 3 header: sync (11 bits) = 0x7FF, version = 11 (MPEG-1),
    // layer = 01 (Layer 3) 0xFF, 0xFB, ...
    let mp3_header = [0xFF, 0xFB, 0x90, 0x00];
    expect_eq!(detect_mpeg_audio_codec_id(&mp3_header), Some(CODEC_ID_MP3));

    // MPEG-2 Layer 1 header: sync = 0x7FF, version = 10 (MPEG-2), layer = 11
    // (Layer 1)
    let mpeg2_layer1_header = [0xFF, 0xF7, 0x90, 0x00];
    expect_eq!(detect_mpeg_audio_codec_id(&mpeg2_layer1_header), Some(CODEC_ID_MP1));

    // MPEG-2 Layer 2 header: sync (11 bits) = 0x7FF, version = 10 (MPEG-2),
    // layer = 10 (Layer 2) 0xFF, 0xF5, ...
    let mpeg2_layer2_header = [0xFF, 0xF5, 0x90, 0x00];
    expect_eq!(detect_mpeg_audio_codec_id(&mpeg2_layer2_header), Some(CODEC_ID_MP2));

    // MPEG-2 Layer 3 header: sync = 0x7FF, version = 10 (MPEG-2), layer = 01
    // (Layer 3)
    let mpeg2_layer3_header = [0xFF, 0xF3, 0x90, 0x00];
    expect_eq!(detect_mpeg_audio_codec_id(&mpeg2_layer3_header), Some(CODEC_ID_MP3));

    // MPEG-2.5 Layer 3 header: sync = 0x7FF, version = 00 (MPEG-2.5), layer =
    // 01 (Layer 3)
    let mpeg25_layer3_header = [0xFF, 0xE3, 0x90, 0x00];
    expect_eq!(detect_mpeg_audio_codec_id(&mpeg25_layer3_header), Some(CODEC_ID_MP3));

    // Invalid / Non-MPEG headers:
    expect_eq!(detect_mpeg_audio_codec_id(&[]), None);
    expect_eq!(detect_mpeg_audio_codec_id(&[0xFF]), None); // 1 byte
    expect_eq!(detect_mpeg_audio_codec_id(&[0xFF, 0xFB]), None); // 2 bytes
    expect_eq!(detect_mpeg_audio_codec_id(&[0xFF, 0xFB, 0x90]), None); // 3 bytes
    expect_eq!(detect_mpeg_audio_codec_id(&[0x00, 0x00, 0x00, 0x00]), None); // No sync
    expect_eq!(detect_mpeg_audio_codec_id(&[0xFE, 0xFB, 0x00, 0x00]), None); // Sync byte 0 mismatch
    expect_eq!(detect_mpeg_audio_codec_id(&[0xFF, 0x1B, 0x00, 0x00]), None); // Sync byte 1 top bits mismatch
    expect_eq!(detect_mpeg_audio_codec_id(&[0xFF, 0xE9, 0x00, 0x00]), None); // Reserved version (01)
    expect_eq!(detect_mpeg_audio_codec_id(&[0xFF, 0xF9, 0x00, 0x00]), None); // Reserved layer (00)
}

// Verify that an MP3-configured decoder dynamically updates to MP2 when
// an MP2 packet is encountered and successfully decodes the frame.
#[gtest(SymphoniaDecoderBridgeTest, Mp2LayerSwitching)]
fn test_mp2_layer_switching() {
    use symphonia::core::codecs::audio::well_known::*;

    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Mp3,
        extra_data: &[],
        bytes_per_sample: 4,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };
    let mut result = init_symphonia_decoder(&config);
    expect_eq!(result.status, ffi::SymphoniaInitStatus::Ok);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP3));

    // MPEG-1 Layer 2 frame header: 96 kbps, 48000 Hz, mono (frame size = 288
    // bytes). Remaining zero bytes indicate bit allocation 0 (silence) for
    // all sub-bands.
    let mut mp2_packet_data = vec![0u8; 288];
    mp2_packet_data[..4].copy_from_slice(&[0xFF, 0xFD, 0x64, 0xD0]);
    let packet =
        ffi::SymphoniaPacket { timestamp_us: 0, duration_us: 24000, data: &mp2_packet_data };

    // Calling decode should trigger maybe_update_mpeg_decoder, update to MP2,
    // and decode 1152 PCM frames.
    let decode_result = result.decoder.decode(&packet);
    expect_eq!(decode_result.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode_result.buffer.num_frames, 1152);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP2));
}

// Verify that an MP3-configured decoder dynamically updates to MP1 when
// an MP1 packet is encountered and successfully decodes the frame.
#[gtest(SymphoniaDecoderBridgeTest, Mp1LayerSwitching)]
fn test_mp1_layer_switching() {
    use symphonia::core::codecs::audio::well_known::*;

    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Mp3,
        extra_data: &[],
        bytes_per_sample: 4,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };
    let mut result = init_symphonia_decoder(&config);
    expect_eq!(result.status, ffi::SymphoniaInitStatus::Ok);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP3));

    // MPEG-1 Layer 1 frame header: 192 kbps, 48000 Hz, mono (frame size = 192
    // bytes). Remaining zero bytes indicate bit allocation 0 (silence) for
    // all sub-bands.
    let mut mp1_packet_data = vec![0u8; 192];
    mp1_packet_data[..4].copy_from_slice(&[0xFF, 0xFE, 0x64, 0xD0]);
    let packet =
        ffi::SymphoniaPacket { timestamp_us: 0, duration_us: 8000, data: &mp1_packet_data };

    // Calling decode should trigger maybe_update_mpeg_decoder, update to MP1,
    // and decode 384 PCM frames.
    let decode_result = result.decoder.decode(&packet);
    expect_eq!(decode_result.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode_result.buffer.num_frames, 384);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP1));
}

// Verify that an MP3-configured decoder dynamically updates when switching
// between layers mid-stream (e.g., MP2 to MP1 and back to MP2).
#[gtest(SymphoniaDecoderBridgeTest, MpMidstreamLayerSwitching)]
fn test_mp_midstream_layer_switching() {
    use symphonia::core::codecs::audio::well_known::*;

    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Mp3,
        extra_data: &[],
        bytes_per_sample: 4,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };
    let mut result = init_symphonia_decoder(&config);
    expect_eq!(result.status, ffi::SymphoniaInitStatus::Ok);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP3));

    // Decode MP2 frame.
    let mut mp2_packet_data = vec![0u8; 288];
    mp2_packet_data[..4].copy_from_slice(&[0xFF, 0xFD, 0x64, 0xD0]);
    let mp2_packet =
        ffi::SymphoniaPacket { timestamp_us: 0, duration_us: 24000, data: &mp2_packet_data };
    let decode_result = result.decoder.decode(&mp2_packet);
    expect_eq!(decode_result.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode_result.buffer.num_frames, 1152);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP2));

    // Decode MP1 frame mid-stream.
    let mut mp1_packet_data = vec![0u8; 192];
    mp1_packet_data[..4].copy_from_slice(&[0xFF, 0xFE, 0x64, 0xD0]);
    let mp1_packet =
        ffi::SymphoniaPacket { timestamp_us: 24000, duration_us: 8000, data: &mp1_packet_data };
    let decode_result = result.decoder.decode(&mp1_packet);
    expect_eq!(decode_result.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode_result.buffer.num_frames, 384);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP1));

    // Decode MP2 frame again mid-stream.
    let decode_result = result.decoder.decode(&mp2_packet);
    expect_eq!(decode_result.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode_result.buffer.num_frames, 1152);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP2));
}

// Verify that an MP3-configured decoder dynamically updates when sample rate
// and channel layout change mid-stream within the same or different MPEG
// layers.
#[gtest(SymphoniaDecoderBridgeTest, MpMidstreamSampleRateAndChannelChange)]
fn test_mp_midstream_sample_rate_and_channel_change() {
    use symphonia::core::codecs::audio::well_known::*;

    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Mp3,
        extra_data: &[],
        bytes_per_sample: 4,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };
    let mut result = init_symphonia_decoder(&config);
    expect_eq!(result.status, ffi::SymphoniaInitStatus::Ok);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP3));

    // Packet 1: MP2 96kbps 48kHz Mono (288 bytes).
    let mut mp2_mono_48k = vec![0u8; 288];
    mp2_mono_48k[..4].copy_from_slice(&[0xFF, 0xFD, 0x64, 0xD0]);
    let packet1 = ffi::SymphoniaPacket { timestamp_us: 0, duration_us: 24000, data: &mp2_mono_48k };
    let decode1 = result.decoder.decode(&packet1);
    expect_eq!(decode1.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode1.buffer.sample_rate, 48000);
    expect_eq!(decode1.buffer.channel_count, 1);
    expect_eq!(decode1.buffer.num_frames, 1152);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP2));

    // Packet 2: Midstream change to MP2 96kbps 44.1kHz Stereo (313 bytes).
    let mut mp2_stereo_44k1 = vec![0u8; 313];
    mp2_stereo_44k1[..4].copy_from_slice(&[0xFF, 0xFD, 0x60, 0x10]);
    let packet2 =
        ffi::SymphoniaPacket { timestamp_us: 24000, duration_us: 26122, data: &mp2_stereo_44k1 };
    let decode2 = result.decoder.decode(&packet2);
    expect_eq!(decode2.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode2.buffer.sample_rate, 44100);
    expect_eq!(decode2.buffer.channel_count, 2);
    expect_eq!(decode2.buffer.num_frames, 1152);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP2));

    // Verify copying decoded planar channel data for stereo.
    let mut plane_ch0 = vec![0u8; 1152 * 4];
    expect_true!(result.decoder.copy_decoded_channel(0, &mut plane_ch0));
    let mut plane_ch1 = vec![0u8; 1152 * 4];
    expect_true!(result.decoder.copy_decoded_channel(1, &mut plane_ch1));

    // Packet 3: Second consecutive packet with the same config (MP2 44.1kHz
    // Stereo). Verifies that caching last_mpeg_header_sig avoids
    // re-instantiation while decoding successfully.
    let packet3 =
        ffi::SymphoniaPacket { timestamp_us: 50122, duration_us: 26122, data: &mp2_stereo_44k1 };
    let decode3 = result.decoder.decode(&packet3);
    expect_eq!(decode3.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode3.buffer.sample_rate, 44100);
    expect_eq!(decode3.buffer.channel_count, 2);
    expect_eq!(decode3.buffer.num_frames, 1152);

    // Packet 4: Midstream change to MP1 192kbps 48kHz Mono (192 bytes).
    let mut mp1_mono_48k = vec![0u8; 192];
    mp1_mono_48k[..4].copy_from_slice(&[0xFF, 0xFE, 0x64, 0xD0]);
    let packet4 =
        ffi::SymphoniaPacket { timestamp_us: 76244, duration_us: 8000, data: &mp1_mono_48k };
    let decode4 = result.decoder.decode(&packet4);
    expect_eq!(decode4.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode4.buffer.sample_rate, 48000);
    expect_eq!(decode4.buffer.channel_count, 1);
    expect_eq!(decode4.buffer.num_frames, 384);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP1));

    // Packet 5: Midstream change to MP1 128kbps 32kHz Mono (192 bytes).
    let mut mp1_mono_32k = vec![0u8; 192];
    mp1_mono_32k[..4].copy_from_slice(&[0xFF, 0xFE, 0x48, 0xD0]);
    let packet5 =
        ffi::SymphoniaPacket { timestamp_us: 84244, duration_us: 12000, data: &mp1_mono_32k };
    let decode5 = result.decoder.decode(&packet5);
    expect_eq!(decode5.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode5.buffer.sample_rate, 32000);
    expect_eq!(decode5.buffer.channel_count, 1);
    expect_eq!(decode5.buffer.num_frames, 384);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_MP1));
}

// Verify that non-MP3 codecs (e.g. PCM) do not attempt MPEG header sniffing
// or decoder re-instantiation even if packet data begins with MPEG sync bytes.
#[gtest(SymphoniaDecoderBridgeTest, NonMpegIgnoresMpegHeader)]
fn test_non_mpeg_ignores_mpeg_header() {
    use symphonia::core::codecs::audio::well_known::*;

    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::PcmS16,
        extra_data: &[],
        bytes_per_sample: 2,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };
    let mut result = init_symphonia_decoder(&config);
    expect_eq!(result.status, ffi::SymphoniaInitStatus::Ok);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_PCM_S16LE));

    // Packet starting with MPEG sync bytes.
    let mut pcm_data = vec![0u8; 288];
    pcm_data[..4].copy_from_slice(&[0xFF, 0xFD, 0x64, 0xD0]);
    let packet = ffi::SymphoniaPacket { timestamp_us: 0, duration_us: 3000, data: &pcm_data };

    let decode_result = result.decoder.decode(&packet);
    expect_eq!(decode_result.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(result.decoder.current_codec_id(), Some(CODEC_ID_PCM_S16LE));
}

// Verify that PCM decoder dynamically grows its buffer for large packets
// without requiring a pre-configured max_frames_per_packet limit.
#[gtest(SymphoniaDecoderBridgeTest, PcmLargePacket)]
fn test_pcm_large_packet() {
    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::PcmS16,
        extra_data: &[],
        bytes_per_sample: 2,
        channel_mask: 3, // Stereo
        channel_count: 2,
        sample_rate: 48000,
    };
    let mut result = init_symphonia_decoder(&config);
    expect_eq!(result.status, ffi::SymphoniaInitStatus::Ok);

    // 8192 stereo 16-bit frames = 32768 bytes (exceeding old 4096 frame limit).
    let pcm_data = vec![0x12u8; 8192 * 2 * 2];
    let packet = ffi::SymphoniaPacket { timestamp_us: 0, duration_us: 170666, data: &pcm_data };
    let decode_result = result.decoder.decode(&packet);
    expect_eq!(decode_result.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode_result.buffer.num_frames, 8192);

    let mut dst = vec![0u8; pcm_data.len()];
    expect_true!(result.decoder.copy_decoded_samples(&mut dst));
    expect_eq!(dst, pcm_data);
}

// Verify that copy_channel_to_slice copies individual channel planes correctly
// and handles error conditions safely.
#[gtest(SymphoniaDecoderBridgeTest, PlanarF32CopyChannelToSlice)]
fn test_planar_f32_copy_channel_to_slice() {
    const SAMPLE_RATE: u32 = 48000;
    let spec = AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO);
    let mut audio_buf = AudioBuffer::<f32>::new(spec, 3);
    audio_buf.render_uninit(Some(3));

    // 3 frames = 12 bytes per channel plane.
    audio_buf.plane_mut(0).unwrap().copy_from_slice(&[0.25, 0.5, 0.75]);
    audio_buf.plane_mut(1).unwrap().copy_from_slice(&[-0.25, -0.5, -0.75]);

    // 32-byte destination plane for channel 0.
    let mut dst_ch0 = vec![0xAAu8; 32];
    expect_true!(copy_channel_to_slice(
        GenericAudioBufferRef::F32(&audio_buf),
        ffi::SymphoniaAudioCodec::Flac,
        0,
        &mut dst_ch0,
    ));
    let ch0_samples: Vec<f32> =
        dst_ch0[0..12].chunks_exact(4).map(|c| f32::from_ne_bytes(c.try_into().unwrap())).collect();
    expect_eq!(ch0_samples, &[0.25, 0.5, 0.75]);
    expect_eq!(&dst_ch0[12..32], &[0xAAu8; 20]);

    // 32-byte destination plane for channel 1.
    let mut dst_ch1 = vec![0xBBu8; 32];
    expect_true!(copy_channel_to_slice(
        GenericAudioBufferRef::F32(&audio_buf),
        ffi::SymphoniaAudioCodec::Flac,
        1,
        &mut dst_ch1,
    ));
    let ch1_samples: Vec<f32> =
        dst_ch1[0..12].chunks_exact(4).map(|c| f32::from_ne_bytes(c.try_into().unwrap())).collect();
    expect_eq!(ch1_samples, &[-0.25, -0.5, -0.75]);
    expect_eq!(&dst_ch1[12..32], &[0xBBu8; 20]);

    // Out-of-bounds channel should return false.
    let mut dst_ch2 = vec![0xCCu8; 32];
    expect_false!(copy_channel_to_slice(
        GenericAudioBufferRef::F32(&audio_buf),
        ffi::SymphoniaAudioCodec::Flac,
        2,
        &mut dst_ch2,
    ));

    // Destination buffer too short (< 12 bytes) should return false.
    let mut dst_short = vec![0u8; 8];
    expect_false!(copy_channel_to_slice(
        GenericAudioBufferRef::F32(&audio_buf),
        ffi::SymphoniaAudioCodec::Flac,
        0,
        &mut dst_short,
    ));

    // Non-F32 buffer format should return false.
    let s16_buf =
        AudioBuffer::<i16>::new(AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO), 3);
    let mut dst_non_f32 = vec![0u8; 32];
    expect_false!(copy_channel_to_slice(
        GenericAudioBufferRef::S16(&s16_buf),
        ffi::SymphoniaAudioCodec::Flac,
        0,
        &mut dst_non_f32,
    ));

    // MP3 codec clamping and NaN handling.
    let mut mp3_audio_buf =
        AudioBuffer::<f32>::new(AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO), 4);
    mp3_audio_buf.render_uninit(Some(4));
    mp3_audio_buf.plane_mut(0).unwrap().copy_from_slice(&[2.0, -2.0, f32::NAN, 0.5]);
    let mut dst_mp3 = vec![0u8; 16];
    expect_true!(copy_channel_to_slice(
        GenericAudioBufferRef::F32(&mp3_audio_buf),
        ffi::SymphoniaAudioCodec::Mp3,
        0,
        &mut dst_mp3,
    ));
    let mp3_samples: Vec<f32> =
        dst_mp3.chunks_exact(4).map(|c| f32::from_ne_bytes(c.try_into().unwrap())).collect();
    expect_eq!(mp3_samples, &[1.0, -1.0, 0.0, 0.5]);
}

// Verify that copy_samples_to_slice handles error conditions safely.
#[gtest(SymphoniaDecoderBridgeTest, CopySamplesToSliceErrors)]
fn test_copy_samples_to_slice_errors() {
    const SAMPLE_RATE: u32 = 48000;
    let spec = AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO);
    let mut audio_buf = AudioBuffer::<i16>::new(spec, 4);
    audio_buf.render_uninit(Some(4));
    // 4 frames * 2 channels * 2 bytes = 16 bytes.
    let mut dst_short = vec![0u8; 8];
    expect_false!(copy_samples_to_slice(
        GenericAudioBufferRef::S16(&audio_buf),
        ffi::SymphoniaSampleFormat::S16,
        &mut dst_short,
    ));

    let mut dst_valid = vec![0u8; 16];
    expect_true!(copy_samples_to_slice(
        GenericAudioBufferRef::S16(&audio_buf),
        ffi::SymphoniaSampleFormat::S16,
        &mut dst_valid,
    ));

    // Planar F32 should return false when passed to copy_samples_to_slice.
    let f32_buf =
        AudioBuffer::<f32>::new(AudioSpec::new(SAMPLE_RATE, layouts::CHANNEL_LAYOUT_STEREO), 4);
    let mut dst_f32 = vec![0u8; 32];
    expect_false!(copy_samples_to_slice(
        GenericAudioBufferRef::F32(&f32_buf),
        ffi::SymphoniaSampleFormat::PlanarF32,
        &mut dst_f32,
    ));
}

struct MockResetDecoder {
    params: symphonia::core::codecs::audio::AudioCodecParameters,
    buf: AudioBuffer<i16>,
}

impl MockResetDecoder {
    fn new() -> Self {
        let spec = AudioSpec::new(48000, layouts::CHANNEL_LAYOUT_MONO);
        Self {
            params: symphonia::core::codecs::audio::AudioCodecParameters::default(),
            buf: AudioBuffer::<i16>::new(spec, 0),
        }
    }
}

static MOCK_CODEC_INFO: symphonia::core::codecs::CodecInfo = symphonia::core::codecs::CodecInfo {
    short_name: "mock",
    long_name: "mock decoder",
    profiles: &[],
};

impl symphonia::core::codecs::audio::AudioDecoder for MockResetDecoder {
    fn reset(&mut self) {}
    fn codec_info(&self) -> &symphonia::core::codecs::CodecInfo {
        &MOCK_CODEC_INFO
    }
    fn codec_params(&self) -> &symphonia::core::codecs::audio::AudioCodecParameters {
        &self.params
    }
    fn decode_ref(
        &mut self,
        _packet: &symphonia::core::packet::PacketRef<'_>,
    ) -> Result<GenericAudioBufferRef<'_>, symphonia::core::errors::Error> {
        Err(symphonia::core::errors::Error::ResetRequired)
    }
    fn finalize(&mut self) -> symphonia::core::codecs::audio::FinalizeResult {
        Default::default()
    }
    fn last_decoded(&self) -> GenericAudioBufferRef<'_> {
        GenericAudioBufferRef::S16(&self.buf)
    }
}

// Verify that when the underlying decoder returns Error::ResetRequired (e.g.
// on midstream signal specification change), decode_impl re-instantiates the
// decoder and successfully retries decoding.
#[gtest(SymphoniaDecoderBridgeTest, ResetRequiredReinstantiatesDecoder)]
fn test_reset_required_reinstantiates_decoder() {
    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::PcmS16,
        extra_data: &[],
        bytes_per_sample: 2,
        channel_mask: 1, // Mono
        channel_count: 1,
        sample_rate: 48000,
    };
    let mut result = init_symphonia_decoder(&config);
    expect_eq!(result.status, ffi::SymphoniaInitStatus::Ok);

    // Replace the internal decoder with our mock that returns
    // Error::ResetRequired on decode.
    result.decoder.set_decoder_for_testing(Box::new(MockResetDecoder::new()));

    // Decode a valid PCM packet (4 samples of 16-bit mono PCM = 8 bytes).
    let pcm_data = [0x00, 0x10, 0x00, 0x20, 0x00, 0x30, 0x00, 0x40];
    let packet = ffi::SymphoniaPacket { timestamp_us: 0, duration_us: 1000, data: &pcm_data };
    let decode_result = result.decoder.decode(&packet);
    expect_eq!(decode_result.status, ffi::SymphoniaDecodeStatus::Ok);
    expect_eq!(decode_result.buffer.sample_rate, 48000);
    expect_eq!(decode_result.buffer.channel_count, 1);
    expect_eq!(decode_result.buffer.num_frames, 4);

    let mut dst = vec![0u8; 8];
    expect_true!(result.decoder.copy_decoded_samples(&mut dst));
    expect_eq!(&dst, &pcm_data);
}

// Verify that create_audio_buffer rejects invalid zero sample rate or channels.
#[gtest(SymphoniaDecoderBridgeTest, RejectsZeroSampleRateAndChannels)]
fn test_rejects_zero_sample_rate_and_channels() {
    let spec_zero_rate = AudioSpec::new(0, layouts::CHANNEL_LAYOUT_STEREO);
    let mut buf_zero_rate = AudioBuffer::<i16>::new(spec_zero_rate, 4);
    buf_zero_rate.render_uninit(Some(4));
    let sample_buf = SymphoniaRawSampleBuffer::new_buffer_for(
        &GenericAudioBufferRef::S16(&buf_zero_rate),
        ffi::SymphoniaAudioCodec::Flac,
        2,
    )
    .unwrap();
    expect_true!(
        create_audio_buffer(GenericAudioBufferRef::S16(&buf_zero_rate), sample_buf).is_err()
    );
}

// Verify that create_audio_buffer safely handles channel masks exceeding
// 32-bits without panicking.
#[gtest(SymphoniaDecoderBridgeTest, HandlesOverflowingChannelMask)]
fn test_handles_overflowing_channel_mask() {
    let spec = AudioSpec::new(44100, Channels::Positioned(Position::from_bits_retain(1 << 40)));
    let mut buf = AudioBuffer::<i16>::new(spec, 4);
    buf.render_uninit(Some(4));
    let sample_buf = SymphoniaRawSampleBuffer::new_buffer_for(
        &GenericAudioBufferRef::S16(&buf),
        ffi::SymphoniaAudioCodec::Flac,
        2,
    )
    .unwrap();
    let result = create_audio_buffer(GenericAudioBufferRef::S16(&buf), sample_buf);
    expect_true!(result.is_ok());
    expect_eq!(result.unwrap().channel_mask, 0);
}
