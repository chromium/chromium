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
        /// Failed to unpack Xiph lacing for Vorbis extradata.
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

    /// Expected bytes per sample.
    bytes_per_sample: u8,

    /// The codec of the audio stream.
    codec: ffi::SymphoniaAudioCodec,
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

const XIPH_LACING_MAX_VALUE: u8 = 255;
const VORBIS_NUM_HEADERS: u8 = 3;
const VORBIS_NUM_LACED_HEADERS: u8 = VORBIS_NUM_HEADERS - 1;

/// Unpacks Vorbis extradata packed in the Xiph lacing format.
///
/// WebM and Matroska containers use the Xiph lacing format for Vorbis
/// extradata, where a single `CodecPrivate` buffer contains all three Vorbis
/// headers: the Identification, Comment, and Setup headers.
///
/// Symphonia's `symphonia-codec-vorbis` decoder does not understand the Xiph
/// packaging layer. It expects only the raw Identification and Setup headers
/// laid out sequentially. This function unpacks the Xiph format and returns
/// a new vector containing only those two required headers.
pub fn unpack_xiph_vorbis_extradata(extradata: &[u8]) -> Result<Vec<u8>, String> {
    // The first byte of the data block specifies the number of headers minus one.
    if extradata.is_empty() {
        return Err("extradata is empty".into());
    }
    if extradata[0] != VORBIS_NUM_LACED_HEADERS {
        return Err(format!(
            "expected {} headers but found {}",
            VORBIS_NUM_LACED_HEADERS + 1,
            extradata[0] + 1
        ));
    }

    let mut offset = 1;
    let mut lengths = Vec::new();

    // The Identification and Comment headers have their lengths laced. The length
    // of the Setup header is inferred from the remaining buffer size.
    for _ in 0..VORBIS_NUM_LACED_HEADERS {
        let mut length = 0;
        let mut reached_end = false;
        while offset < extradata.len() {
            let val = extradata[offset];
            offset += 1;
            length += val as usize;

            // Reached the final segment.
            if val < XIPH_LACING_MAX_VALUE {
                reached_end = true;
                break;
            }
        }
        if !reached_end {
            return Err("truncated length lacing".into());
        }
        lengths.push(length);
    }

    if offset >= extradata.len() {
        return Err("no data remains after reading lacing".into());
    }

    let ident_len = lengths[0];
    let comment_len = lengths[1];
    // Header contained invalid length.
    if offset + ident_len + comment_len > extradata.len() {
        return Err("header lengths exceed buffer size".into());
    }

    let setup_len = extradata.len() - offset - ident_len - comment_len;

    let ident_start = offset;
    let comment_start = ident_start + ident_len;
    let setup_start = comment_start + comment_len;

    let mut unpacked = Vec::with_capacity(ident_len + setup_len);
    unpacked.extend_from_slice(&extradata[ident_start..ident_start + ident_len]);
    unpacked.extend_from_slice(&extradata[setup_start..setup_start + setup_len]);

    Ok(unpacked)
}

/// Detailed status code indicating the result of a decoder initialization
/// attempt.
#[derive(Debug, Clone)]
pub enum SymphoniaInitError {
    UnsupportedCodec(String),
    XiphVorbisUnpackError(String),
    SymphoniaError(ffi::SymphoniaInitStatus, String),
}

impl From<SymphoniaInitError> for (ffi::SymphoniaInitStatus, String) {
    fn from(err: SymphoniaInitError) -> Self {
        match err {
            SymphoniaInitError::UnsupportedCodec(s) => {
                (ffi::SymphoniaInitStatus::UnsupportedCodec, s)
            }
            SymphoniaInitError::XiphVorbisUnpackError(s) => {
                (ffi::SymphoniaInitStatus::XiphVorbisUnpackError, s)
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

/// Converts an FFI `SymphoniaDecoderConfig` to Symphonia `CodecParameters`.
impl<'a> TryFrom<&ffi::SymphoniaDecoderConfig<'a>> for AudioCodecParameters {
    type Error = SymphoniaInitError;

    fn try_from(value: &ffi::SymphoniaDecoderConfig<'a>) -> Result<Self, Self::Error> {
        let bits_per_sample: u32 = (value.bytes_per_sample as u32) * u8::BITS;
        let mut extra_data = value.extra_data.to_vec();

        // Chromium's demuxers often pack Vorbis extradata using the Xiph format, which
        // is a byproduct of using FFmpeg.
        //
        // We unpack the Xiph format here if we detect it, dropping the comment
        // header, as Symphonia expects only the raw identification and setup headers.
        if value.codec == ffi::SymphoniaAudioCodec::Vorbis {
            match unpack_xiph_vorbis_extradata(&extra_data) {
                Ok(unpacked) => extra_data = unpacked,
                Err(err) => {
                    // It could be that this stream is not Xiph packed at all (which is fine,
                    // Symphonia might handle it natively if it's already unwrapped). We only log
                    // an error if we actually attempted to parse it as Xiph but failed.
                    if !extra_data.is_empty() && extra_data[0] == VORBIS_NUM_LACED_HEADERS {
                        return Err(SymphoniaInitError::XiphVorbisUnpackError(format!(
                            "failed to unpack xiph vorbis extradata: {}",
                            err
                        )));
                    }
                }
            }
        }

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

        if !extra_data.is_empty() {
            params.with_extra_data(extra_data.into_boxed_slice());
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

    Ok(SymphoniaDecoder {
        decoder_impl: Some(DecoderImpl {
            decoder,
            bytes_per_sample: config.bytes_per_sample,
            codec: config.codec,
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

    // If there are no frames, avoid passing the buffer to Symphonia's
    // `copy_interleaved_ref`. There is a bug in Symphonia's
    // `copy_interleaved_typed` where if `n_channels > 2` and `n_frames == 0`,
    // it will panic trying to slice a zero-length destination buffer with
    // `dst_buf[ch..]`.
    //
    // Tracked upstream in https://github.com/pdeljanov/Symphonia/issues/455.
    // When resolved upstream and a release is issued with the fix, we can
    // remove this workaround.
    if num_frames == 0 {
        return Ok(ffi::SymphoniaAudioBuffer {
            data: Vec::new(),
            sample_format: sample_buffer.sample_format(),
            sample_rate,
            num_frames,
            channel_count,
            channel_mask: channel_mask.try_into().unwrap(),
        });
    }

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
