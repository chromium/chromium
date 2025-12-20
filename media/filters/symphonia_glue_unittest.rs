// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//media/filters:symphonia_glue";
}

use num_traits::FromBytes;
use rust_gtest_interop::prelude::*;
use symphonia::core::audio::{AudioBuffer, AudioBufferRef, Layout, Signal, SignalSpec};
use symphonia_glue::{create_audio_buffer, ffi, SymphoniaRawSampleBuffer};

fn test_conversion<S, E, F>(
    samples: &[S],
    sample_rate: u32,
    bytes_per_sample: u32,
    expected_format: ffi::SymphoniaSampleFormat,
    expected_samples: &[E],
    to_ref: F,
) where
    S: symphonia::core::sample::Sample,
    E: FromBytes + std::fmt::Debug + std::cmp::PartialEq,
    for<'a> &'a E::Bytes: TryFrom<&'a [u8]>, // Allow Bytes that can created from a u8 slice.
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
    expect_eq!(data_u8.len(), expected_samples.len() * std::mem::size_of::<E>());

    for (i, expected) in expected_samples.iter().enumerate() {
        let start = i * std::mem::size_of::<E>();
        let end = start + std::mem::size_of::<E>();
        let bytes: &E::Bytes =
            data_u8[start..end].try_into().ok().expect("slice conversion failed");
        let val: E = FromBytes::from_ne_bytes(bytes);
        expect_eq!(val, *expected, "Mismatch at index {}", i);
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
    const BYTES_PER_SAMPLE: u32 = 2;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::S16,
        EXPECTED,
        |b| AudioBufferRef::S32(std::borrow::Cow::Borrowed(b)),
    );
}

// Verify that we do not downsample S32 when bytes_per_sample is 4.
#[gtest(SymphoniaGlueTest, NoDownsamplingS32)]
fn test_no_downsampling_s32() {
    const SAMPLES: &[i32] = &[0, 0x7FFF0000, i32::MIN, 0x12345678];
    const SAMPLE_RATE: u32 = 44100;
    const BYTES_PER_SAMPLE: u32 = 4;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::S32,
        SAMPLES,
        |b| AudioBufferRef::S32(std::borrow::Cow::Borrowed(b)),
    );
}

// Verify that we do not downsample F32 when bytes_per_sample is 4.
#[gtest(SymphoniaGlueTest, NoDownsamplingF32)]
fn test_no_downsampling_f32() {
    const SAMPLES: &[f32] = &[0.0, 0.5, -0.5, 1.0];
    const SAMPLE_RATE: u32 = 48000;
    const BYTES_PER_SAMPLE: u32 = 4;
    test_conversion(
        SAMPLES,
        SAMPLE_RATE,
        BYTES_PER_SAMPLE,
        ffi::SymphoniaSampleFormat::F32,
        SAMPLES,
        |b| AudioBufferRef::F32(std::borrow::Cow::Borrowed(b)),
    );
}
