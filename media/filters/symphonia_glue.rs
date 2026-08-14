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

use symphonia::core::audio::sample::i24;
use symphonia::core::audio::{Audio, Channels, GenericAudioBufferRef, Position};
use symphonia::core::codecs::audio::{AudioCodecParameters, AudioDecoder};
use symphonia::core::errors::Error;
use symphonia::core::packet::Packet;

/// This module defines the FFI boundary using the `cxx` crate.
///
/// It contains structs and enums that are shared between C++ and Rust, as well
/// as `extern` blocks that declare the functions callable from the other
/// language. The `namespace = "media"` directive places all generated C++ code
/// within the `media` namespace.
#[cxx::bridge(namespace = "media")]
pub mod ffi {
    /// This identifies the compression format of the audio data.
    #[repr(i32)]
    #[derive(Debug, Clone, Copy)]
    enum SymphoniaAudioCodec {
        Unknown,
        Flac,
        Mp3,
        PcmAlaw,
        PcmF32,
        PcmF32Planar,
        PcmMulaw,
        PcmS16,
        PcmS16be,
        PcmS16Planar,
        PcmS24,
        PcmS24be,
        PcmS32,
        PcmS32Planar,
        PcmU8,
        PcmU8Planar,
        Vorbis,
    }

    /// We currently only output interleaved data, and usually in F32. However,
    /// that is not guaranteed by Symphonia.
    #[derive(Debug)]
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
    struct SymphoniaDecoderConfig<'a> {
        /// The codec of the audio stream (e.g., AAC, MP3, Opus).
        codec: SymphoniaAudioCodec,
        /// Codec-specific initialization data (e.g., AAC headers).
        extra_data: &'a [u8],
        /// Expected bytes per sample from the container/config.
        bytes_per_sample: u8,

        /// fields necessary for PCM decoders.
        /// Maximum number of frames per packet.
        max_frames_per_packet: u64,
        /// Sample rate of the audio stream.
        sample_rate: u32,
        /// Channel mask of the audio stream.
        channel_mask: u32,
    }

    /// Represents a single, encoded audio packet to be sent to the decoder.
    /// This struct is populated on the C++ side for each call to `decode`.
    struct SymphoniaPacket<'a> {
        /// The presentation timestamp of the packet in microseconds.
        timestamp_us: u64,
        /// The duration of the packet in microseconds.
        duration_us: u64,
        /// The buffer containing the encoded packet data.
        data: &'a [u8],
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
        /// The number of channels.
        channel_count: usize,
        /// The channels, represented as a bit mask.
        channel_mask: u32,
    }

    /// Detailed status code indicating the result of a decoder initialization
    /// attempt.
    /// NOTE: these values are persisted to UMA histograms, and should not be
    /// reordered or deleted.
    #[derive(Debug)]
    enum SymphoniaInitStatus {
        /// Initialization was successful.
        Ok,
        /// The `SymphoniaDecoderConfig` contained invalid or inconsistent
        /// parameters.
        InvalidConfig,
        /// Failed to construct a valid decoder instance.
        DecoderError,
        /// The requested codec is not supported by the bridge.
        UnsupportedCodec,
        /// Deprecated: Vorbis extradata unpacking is now handled internally by
        /// Symphonia.
        XiphVorbisUnpackError,
        /// Symphonia returned an 'Unsupported' error during initialization.
        SymphoniaUnsupported,
        /// Symphonia returned a 'DecodeError' during initialization (likely
        /// bad extradata).
        SymphoniaDecodeError,
        /// Symphonia returned an 'IoError' during initialization.
        SymphoniaIoError,
        /// Symphonia returned a 'LimitError' during initialization.
        SymphoniaLimitError,

        /// Boundary for UMA histograms.
        kMaxValue,
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
    /// NOTE: these values are persisted to UMA histograms, and should not be
    /// reordered or deleted.
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
        /// An error occurred while seeking.
        SeekError,
        /// The stream contains a feature or format that is not supported.
        Unsupported,
        /// The decoder returned an insufficient amount of data.
        InsufficentData,
        /// The decoder returned a sample format that is not supported by
        /// the Chromium media stack.
        InvalidDecodedBufferSampleFormat,

        /// Boundary for UMA histograms.
        kMaxValue,
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
        channel_count: 0,
        channel_mask: 0,
    }
}

/// A byte-oriented sample buffer that holds decoded sample data in its
/// original, strongly-typed format (`i16`, `f32`, etc.) while providing
/// methods to access it as a raw byte slice (`&[u8]`). This is crucial
/// for passing the data across the FFI boundary.
pub struct SymphoniaRawSampleBuffer {
    /// Interleaved audio sample data as bytes.
    data: Vec<u8>,
    /// The sample format of the data.
    sample_format: ffi::SymphoniaSampleFormat,
    /// The codec of the audio stream.
    codec: ffi::SymphoniaAudioCodec,
}

impl SymphoniaRawSampleBuffer {
    /// Creates a new, empty `SymphoniaRawSampleBuffer` with a capacity and
    /// specification derived from a decoded `GenericAudioBufferRef`.
    pub fn new_buffer_for(
        buf: &GenericAudioBufferRef,
        codec: ffi::SymphoniaAudioCodec,
    ) -> Result<SymphoniaRawSampleBuffer, String> {
        let sample_format = match buf {
            GenericAudioBufferRef::U8(_) => ffi::SymphoniaSampleFormat::U8,
            GenericAudioBufferRef::S16(_) => ffi::SymphoniaSampleFormat::S16,
            GenericAudioBufferRef::S24(_) => ffi::SymphoniaSampleFormat::S24,
            GenericAudioBufferRef::S32(_) => ffi::SymphoniaSampleFormat::S32,
            GenericAudioBufferRef::F32(_) => ffi::SymphoniaSampleFormat::F32,
            _ => return Err("unsupported format".to_string()),
        };
        Ok(Self { data: Vec::new(), sample_format, codec })
    }

    /// Determines the FFI `SymphoniaSampleFormat` from the inner buffer type.
    fn sample_format(&self) -> ffi::SymphoniaSampleFormat {
        self.sample_format
    }

    /// Gets an immutable slice to the raw bytes of the samples written in the
    /// buffer.
    fn as_bytes(&self) -> &[u8] {
        &self.data
    }

    /// Copies sample data from a Symphonia `GenericAudioBufferRef` into this
    /// buffer. It correctly handles both interleaved and planar formats.
    fn copy_from_buffer(&mut self, src: GenericAudioBufferRef) {
        self.data.clear();
        match src {
            GenericAudioBufferRef::U8(_) => {
                src.copy_bytes_to_vec_interleaved_as::<u8>(&mut self.data)
            }
            GenericAudioBufferRef::S16(_) => {
                src.copy_bytes_to_vec_interleaved_as::<i16>(&mut self.data)
            }
            GenericAudioBufferRef::S24(_) => {
                src.copy_bytes_to_vec_interleaved_as::<i24>(&mut self.data)
            }
            GenericAudioBufferRef::S32(_) => {
                src.copy_bytes_to_vec_interleaved_as::<i32>(&mut self.data)
            }
            GenericAudioBufferRef::F32(_) => {
                if matches!(self.codec, ffi::SymphoniaAudioCodec::Mp3) {
                    let buf = match src {
                        GenericAudioBufferRef::F32(buf) => buf,
                        _ => unreachable!(),
                    };
                    let num_frames = buf.frames();
                    let num_channels = buf.spec().channels().count();

                    self.data.reserve(num_frames * num_channels * std::mem::size_of::<f32>());

                    let planes: Vec<&[f32]> =
                        (0..num_channels).map(|ch| buf.plane(ch).unwrap()).collect();

                    for i in 0..num_frames {
                        for plane in &planes {
                            // Symphonia v0.6+ does not clamp float samples to a valid range.
                            // While some codecs like Opus and Vorbis can legitimately exceed
                            // [-1.0, 1.0], Symphonia's MP3 decoder can produce extreme values
                            // on corrupted streams. We clamp MP3 only to maintain parity with
                            // the FFmpegAudioDecoder's handling of corrupt files.
                            let sample = plane[i].clamp(-1.0, 1.0);
                            self.data.extend_from_slice(&sample.to_le_bytes());
                        }
                    }
                } else {
                    src.copy_bytes_to_vec_interleaved_as::<f32>(&mut self.data)
                }
            }
            _ => {
                unreachable!("Unsupported buffer format should have been caught in new_buffer_for")
            }
        }
    }
}

/// Internal state for a `SymphoniaDecoder`.
///
/// This holds the actual `symphonia` decoder trait object and associated state
/// that needs to persist between `decode` calls.
struct DecoderImpl {
    /// The boxed trait object for the `symphonia` decoder.
    decoder: Box<dyn AudioDecoder>,

    /// The parameters used to configure the decoder.
    codec_params: AudioCodecParameters,

    /// The current codec ID of the active decoder instance.
    current_codec_id: symphonia::core::codecs::audio::AudioCodecId,

    /// Expected bytes per sample.
    bytes_per_sample: u8,

    /// The codec of the audio stream.
    codec: ffi::SymphoniaAudioCodec,

    /// Tracks whether the MPEG layer has already been detected and configured.
    has_detected_layer: bool,
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
fn to_symphonia_codec_id(
    codec: ffi::SymphoniaAudioCodec,
) -> Result<symphonia::core::codecs::audio::AudioCodecId, String> {
    use symphonia::core::codecs::audio::well_known::*;
    match codec {
        ffi::SymphoniaAudioCodec::Unknown => Err("Unknown codec provided".to_string()),
        ffi::SymphoniaAudioCodec::Flac => Ok(CODEC_ID_FLAC),
        ffi::SymphoniaAudioCodec::Mp3 => Ok(CODEC_ID_MP3),
        ffi::SymphoniaAudioCodec::PcmAlaw => Ok(CODEC_ID_PCM_ALAW),
        ffi::SymphoniaAudioCodec::PcmF32 => Ok(CODEC_ID_PCM_F32LE),
        ffi::SymphoniaAudioCodec::PcmF32Planar => Ok(CODEC_ID_PCM_F32LE_PLANAR),
        ffi::SymphoniaAudioCodec::PcmMulaw => Ok(CODEC_ID_PCM_MULAW),
        ffi::SymphoniaAudioCodec::PcmS16 => Ok(CODEC_ID_PCM_S16LE),
        ffi::SymphoniaAudioCodec::PcmS16be => Ok(CODEC_ID_PCM_S16BE),
        ffi::SymphoniaAudioCodec::PcmS16Planar => Ok(CODEC_ID_PCM_S16LE_PLANAR),
        ffi::SymphoniaAudioCodec::PcmS24 => Ok(CODEC_ID_PCM_S24LE),
        ffi::SymphoniaAudioCodec::PcmS24be => Ok(CODEC_ID_PCM_S24BE),
        ffi::SymphoniaAudioCodec::PcmS32 => Ok(CODEC_ID_PCM_S32LE),
        ffi::SymphoniaAudioCodec::PcmS32Planar => Ok(CODEC_ID_PCM_S32LE_PLANAR),
        ffi::SymphoniaAudioCodec::PcmU8 => Ok(CODEC_ID_PCM_U8),
        ffi::SymphoniaAudioCodec::PcmU8Planar => Ok(CODEC_ID_PCM_U8_PLANAR),
        ffi::SymphoniaAudioCodec::Vorbis => Ok(CODEC_ID_VORBIS),
        _ => Err(format!("Unsupported codec value {:?} provided", codec)),
    }
}

/// Converts an FFI `SymphoniaPacket` to a Symphonia `Packet`.
impl<'a> From<&ffi::SymphoniaPacket<'a>> for Packet {
    fn from(value: &ffi::SymphoniaPacket<'a>) -> Self {
        Packet::new(
            0,
            (value.timestamp_us as i64).into(),
            value.duration_us.into(),
            value.data.to_vec(),
        )
    }
}

/// Trims FLAC extradata that may contain a "fLaC" marker and/or a metadata
/// block header for the STREAMINFO block. Symphonia's FLAC decoder expects only
/// the raw 34-byte STREAMINFO block for initialization.
///
/// This function takes a given `extradata`, and:
/// 1. Always strips the "fLaC" container marker if present.
/// 2. If the remaining data identifies a valid FLAC STREAMINFO block at the
///    start, it strips the metadata header and returns just the 34-byte
///    payload.
/// 3. Otherwise, it returns the marker-stripped data as-is.
///
/// See https://www.ietf.org/archive/id/draft-ietf-cellar-flac-12.html#name-file-level-metadata
pub fn get_streaminfo_payload(extradata: &[u8]) -> &[u8] {
    const MARKER: &[u8; 4] = b"fLaC";
    const HEADER_SIZE: usize = 4;
    const STREAMINFO_SIZE: usize = 34;

    // Always skip the "fLaC" marker if it's there. This is container-level framing
    // and is never part of a metadata block. Note that a valid STREAMINFO block
    // can never start with these bytes because it would violate the requirement
    // that max_block_size >= min_block_size.
    let stripped = extradata.strip_prefix(MARKER).unwrap_or(extradata);

    // If skipping the marker revealed the raw payload, return it.
    if stripped.len() == STREAMINFO_SIZE {
        return stripped;
    }

    // If the data (after optional marker) starts with a STREAMINFO metadata block
    // header, extract its payload.
    if stripped.len() >= HEADER_SIZE + STREAMINFO_SIZE {
        // The first bit indicates if this is the last block, the next 7 indicate block
        // type. https://www.ietf.org/archive/id/draft-ietf-cellar-flac-12.html#section-8.1
        let block_type = stripped[0] & 0x7f;
        if block_type == 0 {
            let block_len = u32::from_be_bytes([0, stripped[1], stripped[2], stripped[3]]) as usize;
            if block_len == STREAMINFO_SIZE {
                return &stripped[HEADER_SIZE..HEADER_SIZE + STREAMINFO_SIZE];
            }
        }
    }

    // We didn't find a definitive STREAMINFO payload to extract, but we've
    // already peeled off any container-level markers.
    stripped
}

/// Detailed status code indicating the result of a decoder initialization
/// attempt.
#[derive(Debug, Clone)]
pub enum SymphoniaInitError {
    UnsupportedCodec(String),
    SymphoniaError(ffi::SymphoniaInitStatus, String),
}

impl From<SymphoniaInitError> for (ffi::SymphoniaInitStatus, String) {
    fn from(err: SymphoniaInitError) -> Self {
        match err {
            SymphoniaInitError::UnsupportedCodec(s) => {
                (ffi::SymphoniaInitStatus::UnsupportedCodec, s)
            }
            SymphoniaInitError::SymphoniaError(status, s) => (status, s),
        }
    }
}

fn to_symphonia_init_status(err: &Error) -> ffi::SymphoniaInitStatus {
    match err {
        Error::Unsupported(_) => ffi::SymphoniaInitStatus::SymphoniaUnsupported,
        Error::DecodeError(_) => ffi::SymphoniaInitStatus::SymphoniaDecodeError,
        Error::IoError(_) => ffi::SymphoniaInitStatus::SymphoniaIoError,
        Error::LimitError(_) => ffi::SymphoniaInitStatus::SymphoniaLimitError,
        _ => ffi::SymphoniaInitStatus::DecoderError,
    }
}

/// Processes raw extradata into a codec-specific boxed slice.
///
/// This function acts as a bridge between Chromium's demuxers (or WebCodecs)
/// and Symphonia's decoders. Different codecs have different requirements for
/// initialization:
///
/// * **FLAC**: Extracts the verified STREAMINFO payload from potentially
///   wrapped or marker-prefixed data.
/// * **Others**: Returns a simple copy of the non-empty raw data.
///
/// Returns `Ok(Some(Box<[u8]>))` if valid initialization data was found or
/// extracted, `Ok(None)` if the input was empty, or an error if extraction
/// failed for a recognized format.
fn get_extra_data(
    codec: ffi::SymphoniaAudioCodec,
    raw_extra_data: &[u8],
) -> Result<Option<Box<[u8]>>, SymphoniaInitError> {
    match codec {
        // Depending on what demuxer implementation was used (and in the case of WebCodecs we may
        // have no idea), the extra data may need to be stripped of the FLAC magic marker
        // and / or the metadata block header.
        ffi::SymphoniaAudioCodec::Flac => {
            let payload = get_streaminfo_payload(raw_extra_data);
            Ok((!payload.is_empty()).then(|| Box::from(payload)))
        }
        _ => Ok((!raw_extra_data.is_empty()).then(|| Box::from(raw_extra_data))),
    }
}

/// Converts an FFI `SymphoniaDecoderConfig` to Symphonia `CodecParameters`.
impl<'a> TryFrom<&ffi::SymphoniaDecoderConfig<'a>> for AudioCodecParameters {
    type Error = SymphoniaInitError;

    fn try_from(value: &ffi::SymphoniaDecoderConfig<'a>) -> Result<Self, Self::Error> {
        let bits_per_sample: u32 = (value.bytes_per_sample as u32) * u8::BITS;

        let extra_data = get_extra_data(value.codec, value.extra_data)?;

        let mut params = AudioCodecParameters::new();
        params
            .for_codec(
                to_symphonia_codec_id(value.codec).map_err(SymphoniaInitError::UnsupportedCodec)?,
            )
            .with_bits_per_sample(bits_per_sample)
            .with_channels(Channels::Positioned(Position::from_bits_truncate(
                value.channel_mask.into(),
            )))
            .with_sample_rate(value.sample_rate);

        if let Some(extra_data) = extra_data {
            params.with_extra_data(extra_data);
        }

        if value.max_frames_per_packet > 0 {
            params.with_max_frames_per_packet(value.max_frames_per_packet);
        }

        Ok(params)
    }
}

/// Type alias for the result of a decoder initialization attempt.
type InitResult = Result<SymphoniaDecoder, SymphoniaInitError>;

/// Helper to convert our internal `InitResult` type to the FFI type.
impl From<InitResult> for ffi::SymphoniaInitResult {
    fn from(result: InitResult) -> Self {
        match result {
            Ok(decoder) => ffi::SymphoniaInitResult {
                status: ffi::SymphoniaInitStatus::Ok,
                decoder: Box::new(decoder),
                error_str: String::new(),
            },
            Err(err) => {
                let (status, error_str) = err.into();
                ffi::SymphoniaInitResult {
                    status,
                    decoder: Box::new(SymphoniaDecoder { decoder_impl: None }),
                    error_str,
                }
            }
        }
    }
}

/// Internal method to initialize a decoder.
///
/// This method actually does the instantiation, and returns an `InitResult`
/// type that can get translated to the FFI boundary type using its `From`
/// trait.
fn init_symphonia_decoder_impl(config: &ffi::SymphoniaDecoderConfig) -> InitResult {
    let codec_params = AudioCodecParameters::try_from(config)?;

    let decoder = symphonia::default::get_codecs()
        .make_audio_decoder(&codec_params, &Default::default())
        .map_err(|e| {
            SymphoniaInitError::SymphoniaError(to_symphonia_init_status(&e), e.to_string())
        })?;

    let current_codec_id = codec_params.codec;

    Ok(SymphoniaDecoder {
        decoder_impl: Some(DecoderImpl {
            decoder,
            codec_params,
            current_codec_id,
            bytes_per_sample: config.bytes_per_sample,
            codec: config.codec,
            has_detected_layer: false,
        }),
    })
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
            Error::IoError(io_err)
                if io_err.kind() == std::io::ErrorKind::UnexpectedEof
                    || (io_err.kind() == std::io::ErrorKind::Other
                        && io_err.to_string() == "unexpected end of bitstream") =>
            {
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
pub fn create_audio_buffer(
    buffer_ref: GenericAudioBufferRef,
    mut sample_buffer: SymphoniaRawSampleBuffer,
    bytes_per_sample: u8,
) -> Result<ffi::SymphoniaAudioBuffer, String> {
    let sample_rate = buffer_ref.spec().rate();
    let num_frames = buffer_ref.frames();
    let channel_count = buffer_ref.spec().channels().count();
    let channel_mask = match buffer_ref.spec().channels() {
        Channels::Positioned(pos) => pos.bits(),
        _ => 0,
    };

    // Populate the sample byte buffer.
    sample_buffer.copy_from_buffer(buffer_ref);
    let mut sample_format = sample_buffer.sample_format();

    // Ensure we output S16 if requested (Symphonia outputs it as S32 regardless).
    let should_shift_down =
        sample_format == ffi::SymphoniaSampleFormat::S32 && bytes_per_sample == 2;
    let should_shift_up = sample_format == ffi::SymphoniaSampleFormat::S24;
    let data = if should_shift_down {
        sample_format = ffi::SymphoniaSampleFormat::S16;
        sample_buffer
            .as_bytes()
            .chunks_exact(4)
            .flat_map(|chunk| {
                let sample = i32::from_ne_bytes(chunk.try_into().unwrap());
                // Shift right by 16 to get back the original 16 bits.
                ((sample >> 16) as i16).to_ne_bytes()
            })
            .collect()
    } else if should_shift_up {
        // Chromium's AudioBuffer expects 24-bit samples to be padded to 32 bits
        // and shifted left by 8 bits to use the full 32-bit range.
        // Chromium is always little-endian.
        sample_buffer
            .as_bytes()
            .chunks_exact(3)
            .flat_map(|chunk| [0, chunk[0], chunk[1], chunk[2]])
            .collect()
    } else {
        sample_buffer.data
    };

    Ok(ffi::SymphoniaAudioBuffer {
        data,
        sample_format,
        sample_rate,
        num_frames,
        channel_count,
        channel_mask: channel_mask.try_into().unwrap(),
    })
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
                buffer,
            },
            Err((status, error_str)) => ffi::SymphoniaDecodeResult {
                status,
                error_str,
                buffer: Box::new(default_audio_buffer()),
            },
        }
    }
}

/// Detects the MPEG audio layer (Layer 1, Layer 2, or Layer 3) from the
/// packet's 4-byte frame header, and returns the corresponding Symphonia
/// AudioCodecId.
pub fn detect_mpeg_audio_codec_id(
    data: &[u8],
) -> Option<symphonia::core::codecs::audio::AudioCodecId> {
    use symphonia::core::codecs::audio::well_known::*;
    if data.len() < 4 {
        return None;
    }
    // MPEG audio sync word: 11 consecutive 1 bits (0xFFE0 mask across first 2
    // bytes).
    if data[0] != 0xFF || (data[1] & 0xE0) != 0xE0 {
        return None;
    }
    let version_bits = (data[1] >> 3) & 0x03;
    let layer_bits = (data[1] >> 1) & 0x03;
    // Version 01 is reserved; Layer 00 is reserved.
    if version_bits == 0x01 || layer_bits == 0x00 {
        return None;
    }
    match layer_bits {
        3 => Some(CODEC_ID_MP1),
        2 => Some(CODEC_ID_MP2),
        1 => Some(CODEC_ID_MP3),
        _ => None,
    }
}

impl DecoderImpl {
    /// In Chromium, all MPEG audio layers (MP1, MP2, MP3) are mapped to
    /// AudioCodec::kMP3. Symphonia's own native demuxer (`MpaReader`) reads
    /// the first frame header during probe and configures track params with
    /// CODEC_ID_MP1, CODEC_ID_MP2, or CODEC_ID_MP3. However, Chromium's
    /// demuxers (such as `EsParserMpeg1Audio` for HLS/MPEG2-TS) only know
    /// AudioCodec::kMP3, so `SymphoniaAudioDecoder` is always initialized with
    /// CODEC_ID_MP3.
    ///
    /// Symphonia's `MpaDecoder` creates a static layer-specific state based on
    /// the initial codec ID. If incoming packets contain Layer 1 or Layer 2
    /// (MP2) audio, `MpaDecoder` returns an invalid layer error. Ideally,
    /// Symphonia's `MpaDecoder` could support dynamic layer switching
    /// internally; until then, we detect the MPEG layer header and
    /// re-instantiate the decoder for that layer to achieve full parity
    /// with FFmpeg. TODO(crbug.com/544919881): Consider contributing
    /// dynamic layer switching upstream to Symphonia's MpaDecoder.
    fn maybe_update_mpeg_decoder(&mut self, packet_data: &[u8]) {
        if !matches!(self.codec, ffi::SymphoniaAudioCodec::Mp3) || self.has_detected_layer {
            return;
        }
        let Some(target_codec_id) = detect_mpeg_audio_codec_id(packet_data) else {
            return;
        };
        self.has_detected_layer = true;
        if target_codec_id == self.current_codec_id {
            return;
        }
        let mut new_params = self.codec_params.clone();
        new_params.for_codec(target_codec_id);
        if let Ok(new_decoder) =
            symphonia::default::get_codecs().make_audio_decoder(&new_params, &Default::default())
        {
            self.decoder = new_decoder;
            self.codec_params = new_params;
            self.current_codec_id = target_codec_id;
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

        decoder_impl.maybe_update_mpeg_decoder(packet.data);

        let symphonia_packet = Packet::from(packet);
        let buffer = decoder_impl
            .decoder
            .decode(&symphonia_packet)
            .map_err(|e| ((&e).into(), e.to_string()))?;

        let sample_buffer = SymphoniaRawSampleBuffer::new_buffer_for(&buffer, decoder_impl.codec)
            .map_err(|e| {
            (ffi::SymphoniaDecodeStatus::InvalidDecodedBufferSampleFormat, e.to_string())
        })?;

        Ok(Box::new(
            create_audio_buffer(buffer, sample_buffer, decoder_impl.bytes_per_sample)
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
