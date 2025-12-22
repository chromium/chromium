// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! # Symphonia Decoder FFI Bridge
//!
//! This file provides a C++/Rust interoperability layer (FFI bridge) to
//! leverage the `symphonia` audio decoding library from within the Chromium
//! media stack. The primary goal is to expose a simple, C-compatible interface
//! that C++ code can use to decode various audio formats.
//!
//! ## General Usage
//!
//! The typical lifecycle for a C++ consumer is as follows:
//!
//! 1. **Configuration**: The C++ layer creates a `SymphoniaDecoderConfig`
//!    struct, populating it with the audio stream's parameters. This includes
//!    any "extra data" required for initialization.
//!
//! 2. **Initialization**: It calls `init_symphonia_decoder()` with the
//!    configuration struct. This function attempts to create and initialize a
//!    corresponding `symphonia` decoder. It returns a `SymphoniaInitResult`,
//!    which contains either a pointer to an opaque `SymphoniaDecoder` handle on
//!    success, or an error status and message on failure.
//!
//! 3. **Decoding**: For each encoded audio packet, the C++ layer populates a
//!    `SymphoniaPacket` struct and calls the `decode()` method on the
//!    `SymphoniaDecoder` handle.
//!
//! 4. **Handling Output**: The `decode()` method returns a
//!    `SymphoniaDecodeResult`.
//!     - On success (`Ok`), the result contains a `SymphoniaAudioBuffer` with
//!       the raw, decoded PCM audio data, split into planes.
//!     - On failure, it contains a status code and error message detailing the
//!       issue.
//!     - It can also indicate non-fatal conditions like `EndOfStream`.
//!
//! 5. **Cleanup**: The `SymphoniaDecoder` handle is owned by a `Box` and will
//!    be automatically deallocated when it goes out of scope on the Rust side,
//!    cleaning up all associated decoder resources.
//!
//! This bridge is built using the `cxx` crate, which automates the generation
//! of safe FFI bindings between the two languages.

use cxx;

use symphonia::core::audio::{AudioBufferRef, RawSampleBuffer};
use symphonia::core::codecs::CodecParameters;
use symphonia::core::errors::Error;
use symphonia::core::formats::Packet;
use symphonia::core::sample::i24;
use symphonia::core::units::TimeBase;

/// This module defines the FFI boundary using the `cxx` crate.
///
/// It contains structs and enums that are shared between C++ and Rust, as well
/// as `extern` blocks that declare the functions callable from the other
/// language. The `namespace = "media"` directive places all generated C++ code
/// within the `media` namespace.
#[cxx::bridge(namespace = "media")]
mod ffi {
    /// This identifies the compression format of the audio data.
    #[repr(i32)]
    #[derive(Debug, Clone, Copy)]
    enum SymphoniaAudioCodec {
        Flac,
    }

    /// We currently only output interleaved data, and usually in F32. However,
    /// that is not guaranteed by Symphonia.
    enum SymphoniaSampleFormat {
        Unknown,
        U8,
        S16,
        S24,
        S32,
        F32,
    }

    /// Configuration parameters required to initialize a Symphonia decoder.
    /// This struct is created and populated on the C++ side.
    struct SymphoniaDecoderConfig {
        /// The codec of the audio stream (e.g., AAC, MP3, Opus).
        codec: SymphoniaAudioCodec,
        /// Codec-specific initialization data (e.g., AAC headers).
        extra_data: Vec<u8>,
    }

    /// Represents a single, encoded audio packet to be sent to the decoder.
    /// This struct is populated on the C++ side for each call to `decode`.
    struct SymphoniaPacket {
        /// The presentation timestamp of the packet in microseconds.
        timestamp_us: u64,
        /// The duration of the packet in microseconds.
        duration_us: u64,
        /// The buffer containing the encoded packet data.
        data: Vec<u8>,
    }

    /// Represents a buffer of decoded audio data.
    /// This is the primary output of a successful decode operation.
    struct SymphoniaAudioBuffer {
        /// Interleaved audio sample planes.
        data: Vec<u8>,
        /// Sample format of the interleaved audio. May be different from the
        /// encoded data sample format.
        sample_format: SymphoniaSampleFormat,
        /// The sample rate of the decoded data.
        sample_rate: u32,
        /// The number of audio frames in the buffer.
        num_frames: usize,
    }

    /// Detailed status code indicating the result of a decoder initialization
    /// attempt.
    #[derive(Debug)]
    enum SymphoniaInitStatus {
        /// Initialization was successful.
        Ok,
        /// The `SymphoniaDecoderConfig` contained invalid or inconsistent
        /// parameters.
        InvalidConfig,
        /// Failed to construct a valid decoder instance.
        DecoderError,
    }

    /// Represents the result of a decoder initialization attempt.
    struct SymphoniaInitResult {
        /// The status of the initialization.
        status: SymphoniaInitStatus,
        /// A descriptive error message if initialization failed.
        error_str: String,
        /// On success, a `Box` containing the opaque decoder handle.
        /// It is always populated, but the internal `SymphoniaDecoder` will be
        /// invalid on failure.
        decoder: Box<SymphoniaDecoder>,
    }

    /// Represents the possible outcomes of a decode operation.
    #[derive(Debug)]
    enum SymphoniaDecodeStatus {
        /// The packet was successfully decoded.
        Ok,
        /// The audio stream ended unexpectedly.
        UnexpectedEndOfStream,
        /// The decoder is in an invalid state (e.g., initialization failed).
        InvalidDecoderState,
        /// A generic, non-specific error occurred within Symphonia.
        Error,
        /// A specific error occurred during the decoding of the packet's
        /// content. This often indicates malformed data.
        DecodeError,
        /// An I/O-related error occurred. In this context, it may signal an
        /// unexpected end of data.
        IoError,
        /// The decoder needs to be reset, e.g., due to a change in stream
        /// parameters.
        ResetRequired,
        /// An error occurred while seeking within the media.
        SeekError,
        /// The stream contains a feature or format that is not supported.
        Unsupported,
        /// The decoder returned an insufficient amount of data.
        InsufficentData,
        /// The decoder returned a sample format that is not supported by
        /// the Chromium media stack.
        InvalidDecodedBufferSampleFormat,
    }

    /// Represents the result of an attempt to decode an audio packet.
    struct SymphoniaDecodeResult {
        /// The status of the decode operation.
        status: SymphoniaDecodeStatus,
        /// A descriptive error message if decoding failed.
        error_str: String,
        /// A `Box` containing the decoded audio data. If the end of the stream
        /// has been reached, the buffer will be empty.
        buffer: Box<SymphoniaAudioBuffer>,
    }

    // This block declares the Rust functions that are exposed to C++.
    extern "Rust" {
        /// An opaque type representing the Rust decoder instance. C++ holds a
        /// pointer to this but cannot access its internal fields.
        type SymphoniaDecoder;

        /// Constructs and initializes a `SymphoniaDecoder`.
        ///
        /// # Arguments
        /// * `config` - A reference to the `SymphoniaDecoderConfig` populated
        ///   by C++.
        ///
        /// # Returns
        /// A `SymphoniaInitResult` containing either the decoder handle or an
        /// error.
        fn init_symphonia_decoder(config: &SymphoniaDecoderConfig) -> SymphoniaInitResult;

        /// Decodes a single audio packet.
        /// This is a method on the `SymphoniaDecoder` type.
        ///
        /// # Arguments
        /// * `&mut self` - The `SymphoniaDecoder` instance.
        /// * `packet` - A reference to the `SymphoniaPacket` to be decoded.
        ///
        /// # Returns
        /// A `SymphoniaDecodeResult` containing either the decoded audio buffer
        /// or an error.
        fn decode(&mut self, packet: &SymphoniaPacket) -> SymphoniaDecodeResult;
    }
}

/// Creates a default, empty `SymphoniaAudioBuffer`.
///
/// This is used as a placeholder in error-case `SymphoniaDecodeResult` objects,
/// since `cxx` requires all fields of shared structs to be populated.
fn default_audio_buffer() -> ffi::SymphoniaAudioBuffer {
    ffi::SymphoniaAudioBuffer {
        data: vec![],
        sample_format: ffi::SymphoniaSampleFormat::Unknown,
        sample_rate: 0,
        num_frames: 0,
    }
}

/// A macro to apply an expression to a `GenericRawSampleBuffer`.
/// This avoids repetitive `match` statements for immutable operations.
macro_rules! impl_generic_buffer_func {
    ($type:ident, $var:expr, $buf:ident,$expr:expr) => {
        match $var {
            $type::U8(ref $buf) => $expr,
            $type::S16(ref $buf) => $expr,
            $type::S24(ref $buf) => $expr,
            $type::S32(ref $buf) => $expr,
            $type::F32(ref $buf) => $expr,
        }
    };
}

/// A macro to apply an expression to a `GenericRawSampleBuffer`.
/// This avoids repetitive `match` statements for mutable operations.
macro_rules! impl_generic_buffer_func_mut {
    ($type:ident, $var:expr, $buf:ident,$expr:expr) => {
        match $var {
            $type::U8(ref mut $buf) => $expr,
            $type::S16(ref mut $buf) => $expr,
            $type::S24(ref mut $buf) => $expr,
            $type::S32(ref mut $buf) => $expr,
            $type::F32(ref mut $buf) => $expr,
        }
    };
}

/// An enum to wrap a `symphonia::core::audio::RawSampleBuffer` with a generic
/// sample type.
///
/// This allows for dynamically handling different sample formats at runtime
/// without needing separate code paths for each format.
enum GenericRawSampleBuffer {
    U8(RawSampleBuffer<u8>),
    S16(RawSampleBuffer<i16>),
    S24(RawSampleBuffer<i24>),
    S32(RawSampleBuffer<i32>),
    F32(RawSampleBuffer<f32>),
}

/// A byte-oriented sample buffer that wraps `GenericRawSampleBuffer`.
///
/// This struct's primary purpose is to hold decoded sample data in its
/// original, strongly-typed format (`i16`, `f32`, etc.) while providing
/// methods to access it as a raw byte slice (`&[u8]`). This is crucial
/// for passing the data across the FFI boundary. It is reused across `decode`
/// calls to reduce allocations.
struct SymphoniaRawSampleBuffer {
    /// The inner buffer, holding the type-specific sample data.
    inner: GenericRawSampleBuffer,
}

impl SymphoniaRawSampleBuffer {
    /// Creates a new, empty `SymphoniaRawSampleBuffer` with a capacity and
    /// specification derived from a decoded `AudioBufferRef`.
    fn new_buffer_for(buf: &AudioBufferRef) -> Result<SymphoniaRawSampleBuffer, String> {
        let capacity = buf.capacity() as u64;
        let spec = *buf.spec();

        let buf_result: Result<GenericRawSampleBuffer, &'static str> = match buf {
            AudioBufferRef::U8(_) => {
                Ok(GenericRawSampleBuffer::U8(RawSampleBuffer::<u8>::new(capacity, spec)))
            }
            AudioBufferRef::S16(_) => {
                Ok(GenericRawSampleBuffer::S16(RawSampleBuffer::<i16>::new(capacity, spec)))
            }
            AudioBufferRef::S24(_) => {
                Ok(GenericRawSampleBuffer::S24(RawSampleBuffer::<i24>::new(capacity, spec)))
            }
            AudioBufferRef::S32(_) => {
                Ok(GenericRawSampleBuffer::S32(RawSampleBuffer::<i32>::new(capacity, spec)))
            }
            AudioBufferRef::F32(_) => {
                Ok(GenericRawSampleBuffer::F32(RawSampleBuffer::<f32>::new(capacity, spec)))
            }
            _ => Err("Symphonia returned an unsupported buffer type"),
        };
        Ok(Self { inner: buf_result? })
    }

    /// Determines the FFI `SymphoniaSampleFormat` from the inner buffer type.
    fn sample_format(&self) -> ffi::SymphoniaSampleFormat {
        match self.inner {
            GenericRawSampleBuffer::U8(_) => ffi::SymphoniaSampleFormat::U8,
            GenericRawSampleBuffer::S16(_) => ffi::SymphoniaSampleFormat::S16,
            GenericRawSampleBuffer::S24(_) => ffi::SymphoniaSampleFormat::S24,
            GenericRawSampleBuffer::S32(_) => ffi::SymphoniaSampleFormat::S32,
            GenericRawSampleBuffer::F32(_) => ffi::SymphoniaSampleFormat::F32,
        }
    }
    /// Gets an immutable slice to the raw bytes of the samples written in the
    /// buffer.
    fn as_bytes(&self) -> &[u8] {
        impl_generic_buffer_func!(GenericRawSampleBuffer, self.inner, buf, buf.as_bytes())
    }

    /// Copies sample data from a Symphonia `AudioBufferRef` into this buffer.
    /// It correctly handles both interleaved and planar formats.
    fn copy_from_buffer(&mut self, src: AudioBufferRef) {
        impl_generic_buffer_func_mut!(
            GenericRawSampleBuffer,
            self.inner,
            buf,
            // We always copy the data as interleaved, never planar.
            buf.copy_interleaved_ref(src)
        );
    }
}

/// Internal state for a `SymphoniaDecoder`.
///
/// This holds the actual `symphonia` decoder trait object and associated state
/// that needs to persist between `decode` calls.
struct DecoderImpl {
    /// The boxed trait object for the `symphonia` decoder.
    decoder: Box<dyn symphonia::core::codecs::Decoder>,
    /// A reusable buffer for decoded samples to avoid reallocation.
    /// It is `None` until the first successful decode.
    sample_buffer: Option<SymphoniaRawSampleBuffer>,
}

/// The opaque Rust decoder type exposed to C++ through the FFI bridge.
///
/// It wraps the actual implementation in an `Option` to handle cases where
/// initialization might fail, leaving the decoder in an invalid state. This
/// `Option` serves as a "valid" flag.
pub struct SymphoniaDecoder {
    decoder_impl: Option<DecoderImpl>,
}

/// Converts an FFI `SymphoniaAudioCodec` to a Symphonia `CodecType`.
/// Returns an error string if the codec is not supported.
fn to_symphonia_codec_type(
    codec: ffi::SymphoniaAudioCodec,
) -> Result<symphonia::core::codecs::CodecType, String> {
    match codec {
        ffi::SymphoniaAudioCodec::Flac => Ok(symphonia::core::codecs::CODEC_TYPE_FLAC),

        // TODO(crbug.com/40074653): should support other formats.
        _ => Err(format!("Unsupported codec provided. codec={:?}", codec)),
    }
}

/// Converts an FFI `SymphoniaPacket` to a Symphonia `Packet`.
impl From<&ffi::SymphoniaPacket> for Packet {
    fn from(value: &ffi::SymphoniaPacket) -> Self {
        Packet::new_from_slice(0, value.timestamp_us, value.duration_us, &value.data)
    }
}

/// Converts an FFI `SymphoniaDecoderConfig` to Symphonia `CodecParameters`.
/// Note that we provide the minimum amount of configuration possible, since
/// most of the values should come directly from the bitstream and should not be
/// needed here.
impl TryFrom<&ffi::SymphoniaDecoderConfig> for CodecParameters {
    type Error = String;

    fn try_from(value: &ffi::SymphoniaDecoderConfig) -> Result<Self, Self::Error> {
        Ok(CodecParameters {
            codec: to_symphonia_codec_type(value.codec)?,
            sample_rate: None,
            time_base: Some(TimeBase::new(1_000_000, 1)), // Microsecond timebase
            n_frames: None,
            start_ts: 0,
            sample_format: None,
            bits_per_sample: None,
            bits_per_coded_sample: None,
            channels: None,
            channel_layout: None,
            delay: None,
            padding: None,
            max_frames_per_packet: None,
            packet_data_integrity: false,
            verification_check: None,
            frames_per_block: None,
            extra_data: Some(value.extra_data.clone().into_boxed_slice()),
        })
    }
}

/// Type alias for the result of a decoder initialization attempt.
type InitResult = Result<SymphoniaDecoder, (ffi::SymphoniaInitStatus, String)>;

/// Helper to convert our internal `InitResult` type to the FFI type.
impl From<InitResult> for ffi::SymphoniaInitResult {
    fn from(result: InitResult) -> Self {
        match result {
            Ok(decoder) => ffi::SymphoniaInitResult {
                status: ffi::SymphoniaInitStatus::Ok,
                decoder: Box::new(decoder),
                error_str: String::new(),
            },
            Err((status, error_str)) => ffi::SymphoniaInitResult {
                status,
                decoder: Box::new(SymphoniaDecoder { decoder_impl: None }),
                error_str,
            },
        }
    }
}

/// Internal method to initialize a decoder.
///
/// This method actually does the instantiation, and returns an `InitResult`
/// type that can get translated to the FFI boundary type using its `From`
/// trait.
fn init_symphonia_decoder_impl(config: &ffi::SymphoniaDecoderConfig) -> InitResult {
    let codec_params = CodecParameters::try_from(config)
        .map_err(|e| (ffi::SymphoniaInitStatus::InvalidConfig, e))?;

    let decoder = symphonia::default::get_codecs()
        .make(&codec_params, &Default::default())
        .map_err(|e| (ffi::SymphoniaInitStatus::DecoderError, e.to_string()))?;

    Ok(SymphoniaDecoder { decoder_impl: Some(DecoderImpl { decoder, sample_buffer: None }) })
}

/// FFI-exposed function to initialize a decoder.
///
/// This function is the entry point for creating a decoder from C++. It
/// translates the FFI configuration into Symphonia-compatible parameters and
/// attempts to instantiate a decoder from the default registry.
pub fn init_symphonia_decoder(config: &ffi::SymphoniaDecoderConfig) -> ffi::SymphoniaInitResult {
    init_symphonia_decoder_impl(config).into()
}

/// Converts a `symphonia::core::errors::Error` to an FFI
/// `SymphoniaDecodeStatus`.
impl From<&Error> for ffi::SymphoniaDecodeStatus {
    fn from(err: &Error) -> Self {
        match err {
            Error::DecodeError(_) => ffi::SymphoniaDecodeStatus::DecodeError,
            Error::IoError(io_err) if io_err.kind() == std::io::ErrorKind::UnexpectedEof => {
                ffi::SymphoniaDecodeStatus::UnexpectedEndOfStream
            }
            Error::IoError(_) => ffi::SymphoniaDecodeStatus::IoError,
            Error::ResetRequired => ffi::SymphoniaDecodeStatus::ResetRequired,
            Error::SeekError(_) => ffi::SymphoniaDecodeStatus::SeekError,
            Error::Unsupported(_) => ffi::SymphoniaDecodeStatus::Unsupported,
            _ => ffi::SymphoniaDecodeStatus::Error,
        }
    }
}

/// Creates an FFI `SymphoniaAudioBuffer` from a decoded Symphonia
/// `AudioBufferRef`.
fn create_audio_buffer(
    buffer_ref: AudioBufferRef,
    sample_buffer: &mut SymphoniaRawSampleBuffer,
) -> Result<ffi::SymphoniaAudioBuffer, String> {
    let sample_rate = buffer_ref.spec().rate;
    let num_frames = buffer_ref.frames();

    // Populate the sample byte buffer.
    sample_buffer.copy_from_buffer(buffer_ref);
    let data = sample_buffer.as_bytes().to_vec();
    let sample_format = sample_buffer.sample_format();

    // TODO(crbug.com/40074653): avoid copy here?
    Ok(ffi::SymphoniaAudioBuffer { data, sample_format, sample_rate, num_frames })
}

/// Type alias for the result of a decoding operation.
type DecodeResult = Result<Box<ffi::SymphoniaAudioBuffer>, (ffi::SymphoniaDecodeStatus, String)>;

/// Helper function to convert our internal decode result type to the FFI type.
impl From<DecodeResult> for ffi::SymphoniaDecodeResult {
    fn from(result: DecodeResult) -> Self {
        match result {
            Ok(buffer) => ffi::SymphoniaDecodeResult {
                status: ffi::SymphoniaDecodeStatus::Ok,
                error_str: String::new(),
                buffer: buffer,
            },
            Err((status, error_str)) => ffi::SymphoniaDecodeResult {
                status,
                error_str,
                buffer: Box::new(default_audio_buffer()),
            },
        }
    }
}

impl SymphoniaDecoder {
    /// Internal method to decode an audio packet.
    ///
    /// This method is responsible for calling the Symphonia decoder, and
    /// returns a `DecodeResult` that may be translated to the FFI boundary
    /// type using it's `From` trait.`
    fn decode_impl(&mut self, packet: &ffi::SymphoniaPacket) -> DecodeResult {
        // Ensure the decoder was initialized successfully.
        let decoder_impl = self.decoder_impl.as_mut().ok_or((
            ffi::SymphoniaDecodeStatus::InvalidDecoderState,
            "invalid decoder state".to_string(),
        ))?;

        let symphonia_packet = Packet::from(packet);
        let buffer = decoder_impl
            .decoder
            .decode(&symphonia_packet)
            .map_err(|e| ((&e).into(), e.to_string()))?;

        // Lazily initialize the sample buffer on the first successful decode.
        if decoder_impl.sample_buffer.is_none() {
            decoder_impl.sample_buffer =
                Some(SymphoniaRawSampleBuffer::new_buffer_for(&buffer).map_err(|e| {
                    (ffi::SymphoniaDecodeStatus::InvalidDecodedBufferSampleFormat, e.to_string())
                })?);
        }

        Ok(Box::new(
            create_audio_buffer(buffer, decoder_impl.sample_buffer.as_mut().unwrap())
                .map_err(|e| (ffi::SymphoniaDecodeStatus::InsufficentData, e.to_string()))?,
        ))
    }

    /// FFI-exposed method to decode a single audio packet.
    ///
    /// This is the main function called repeatedly by C++ to process the audio
    /// stream. It handles decoding and manages the internal state, such as
    /// creating the sample buffer on the first successful decode.
    pub fn decode(&mut self, packet: &ffi::SymphoniaPacket) -> ffi::SymphoniaDecodeResult {
        self.decode_impl(packet).into()
    }
}
