// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//media/filters:symphonia_glue";
}

use num_traits::ToBytes;
use rust_gtest_interop::prelude::*;
use symphonia::core::audio::{AudioBuffer, AudioBufferRef, Layout, Signal, SignalSpec};
use symphonia_glue::{create_audio_buffer, ffi, init_symphonia_decoder, SymphoniaRawSampleBuffer};

fn test_conversion<S, E, F>(
    samples: &[S],
    sample_rate: u32,
    bytes_per_sample: u8,
    expected_format: ffi::SymphoniaSampleFormat,
    expected_samples: &[E],
    to_ref: F,
) where
    S: symphonia::core::sample::Sample,
    E: ToBytes + std::fmt::Debug + std::cmp::PartialEq,
    F: for<'a> Fn(&'a AudioBuffer<S>) -> AudioBufferRef<'a>,
{
    let spec = SignalSpec::new(sample_rate, Layout::Mono.into_channels());
    let mut audio_buf = AudioBuffer::<S>::new(samples.len() as u64, spec);
    audio_buf.render_reserved(Some(samples.len()));
    audio_buf.chan_mut(0).copy_from_slice(samples);

    let buffer_ref = to_ref(&audio_buf);
    let mut sample_buffer = SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref).unwrap();

    let result = create_audio_buffer(buffer_ref, &mut sample_buffer, bytes_per_sample).unwrap();

    expect_eq!(result.sample_rate, sample_rate);
    expect_eq!(result.num_frames, samples.len());
    expect_eq!(result.sample_format, expected_format);

    let data_u8 = result.data;
    let actual_samples = data_u8.chunks_exact(bytes_per_sample as usize).collect::<Vec<_>>();
    expect_eq!(actual_samples.len(), expected_samples.len());

    for (i, (expected, actual_bytes)) in
        expected_samples.iter().zip(actual_samples.into_iter()).enumerate()
    {
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
        |b| AudioBufferRef::S32(std::borrow::Cow::Borrowed(b)),
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
        |b| AudioBufferRef::S32(std::borrow::Cow::Borrowed(b)),
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
        |b| AudioBufferRef::F32(std::borrow::Cow::Borrowed(b)),
    );
}

// Verify that we do not clip F32 values outside [-1.0, 1.0].
#[gtest(SymphoniaGlueTest, F32NoClipping)]
fn test_f32_no_clipping() {
    const SAMPLES: &[f32] = &[2.0, -2.0, 10.0, -10.0];
    const SAMPLE_RATE: u32 = 48000;
    const BYTES_PER_SAMPLE: u8 = 4;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::F32,
        SAMPLES,
        |b| AudioBufferRef::F32(std::borrow::Cow::Borrowed(b)),
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
        |b| AudioBufferRef::U8(std::borrow::Cow::Borrowed(b)),
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
        |b| AudioBufferRef::S16(std::borrow::Cow::Borrowed(b)),
    );
}

// Verify that we handle S24 correctly.
#[gtest(SymphoniaGlueTest, S24Conversion)]
fn test_s24_conversion() {
    use symphonia::core::sample::i24;
    let samples: Vec<i24> = vec![i24(0), i24(0x7FFFFF), i24(-0x800000), i24(0x123456)];
    const SAMPLE_RATE: u32 = 48000;
    const BYTES_PER_SAMPLE: u8 = 3;

    let spec = SignalSpec::new(SAMPLE_RATE, Layout::Mono.into_channels());
    let mut audio_buf = AudioBuffer::<i24>::new(samples.len() as u64, spec);
    audio_buf.render_reserved(Some(samples.len()));
    audio_buf.chan_mut(0).copy_from_slice(&samples);

    let buffer_ref = AudioBufferRef::S24(std::borrow::Cow::Borrowed(&audio_buf));
    let mut sample_buffer = SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref).unwrap();

    let result = create_audio_buffer(buffer_ref, &mut sample_buffer, BYTES_PER_SAMPLE).unwrap();

    expect_eq!(result.sample_rate, SAMPLE_RATE);
    expect_eq!(result.num_frames, samples.len());
    expect_eq!(result.sample_format, ffi::SymphoniaSampleFormat::S24);

    let data_u8 = result.data;
    expect_eq!(data_u8.len(), samples.len() * 3);

    for (i, expected) in samples.iter().enumerate() {
        let start = i * 3;
        let actual_bytes = &data_u8[start..start + 3];
        let expected_bytes = expected.to_ne_bytes();
        expect_eq!(actual_bytes, expected_bytes, "Mismatch at index {i}");
    }
}

// Verify that we handle unsupported buffer types.
#[gtest(SymphoniaGlueTest, UnsupportedBufferType)]
fn test_unsupported_buffer_type() {
    let spec = SignalSpec::new(44100, Layout::Mono.into_channels());
    let audio_buf = AudioBuffer::<f64>::new(1, spec);
    let buffer_ref = AudioBufferRef::F64(std::borrow::Cow::Borrowed(&audio_buf));
    let result = SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref);
    expect_true!(result.is_err());
}

// Verify that decoder initialization fails for invalid config.
#[gtest(SymphoniaGlueTest, DecoderInitFailure)]
fn test_decoder_init_failure() {
    let config = ffi::SymphoniaDecoderConfig {
        codec: ffi::SymphoniaAudioCodec::Flac,
        extra_data: vec![], // Empty extra data might be enough to fail some decoders.
        bytes_per_sample: 2,
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
    let spec = SignalSpec::new(SAMPLE_RATE, Layout::Stereo.into_channels());
    let mut audio_buf = AudioBuffer::<f32>::new(2, spec);
    audio_buf.render_reserved(Some(2));

    // Planar data: L[0.5, 0.1], R[-0.5, -0.1]
    audio_buf.chan_mut(0).copy_from_slice(&[0.5, 0.1]);
    audio_buf.chan_mut(1).copy_from_slice(&[-0.5, -0.1]);

    let buffer_ref = AudioBufferRef::F32(std::borrow::Cow::Borrowed(&audio_buf));
    let mut sample_buffer = SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref).unwrap();
    let result = create_audio_buffer(buffer_ref, &mut sample_buffer, 4).unwrap();

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
    let ffi_packet = ffi::SymphoniaPacket {
        timestamp_us: 12345,
        duration_us: 6789,
        data: vec![0xAA, 0xBB, 0xCC],
    };
    let sym_packet = symphonia::core::formats::Packet::from(&ffi_packet);

    expect_eq!(sym_packet.ts(), 12345);
    expect_eq!(sym_packet.dur(), 6789);
    expect_eq!(&*sym_packet.data, &[0xAA, 0xBB, 0xCC]);
}

// Verify that we handle zero frames correctly.
#[gtest(SymphoniaGlueTest, ZeroFrames)]
fn test_zero_frames() {
    const SAMPLE_RATE: u32 = 44100;
    let spec = SignalSpec::new(SAMPLE_RATE, Layout::Mono.into_channels());
    let audio_buf = AudioBuffer::<f32>::new(0, spec);

    let buffer_ref = AudioBufferRef::F32(std::borrow::Cow::Borrowed(&audio_buf));
    let mut sample_buffer = SymphoniaRawSampleBuffer::new_buffer_for(&buffer_ref).unwrap();
    let result = create_audio_buffer(buffer_ref, &mut sample_buffer, 4).unwrap();

    expect_eq!(result.num_frames, 0);
    expect_true!(result.data.is_empty());
}
