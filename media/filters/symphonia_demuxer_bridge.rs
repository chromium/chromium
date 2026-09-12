// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! # Symphonia Demuxer FFI Bridge
//!
//! This file provides a C++/Rust interoperability layer (FFI bridge) to
//! leverage the `symphonia` media demuxing library from within the Chromium
//! media stack. The primary goal is to expose a safe, C-compatible interface
//! that C++ `Demuxer` implementations can use to probe, demux, and seek
//! various media containers.
//!
//! ## General Usage
//!
//! 1. **Probing & Initialization**: The C++ layer calls
//!    `init_symphonia_demuxer()` passing a `SymphoniaSourceBridge` reference
//!    and optional format/MIME hints. Symphonia probes the source stream,
//!    extracts track metadata, and returns a `SymphoniaDemuxerInitResult`
//!    containing track descriptions and an opaque `SymphoniaDemuxerBridge`
//!    handle.
//!
//! 2. **Packet Reading**: The C++ layer calls `read_packet()` to obtain the
//!    next `SymphoniaDemuxerPacket` from the container. Packets provide
//!    timestamps (PTS/DTS), duration, track ID, keyframe status, and discard
//!    padding.
//!
//! 3. **Seeking**: The C++ layer calls `seek_to()` with a target timestamp in
//!    microseconds. Symphonia performs an accurate seek to the nearest
//!    preceding keyframe and returns the actual seeked timestamp.
//!
//! 4. **Cleanup**: The `SymphoniaDemuxerBridge` handle is owned via `Box` on
//!    the C++ side and cleans up all format reader state upon destruction.

use std::collections::HashMap;
use std::io::{self, Read, Seek, SeekFrom};
use std::pin::Pin;
use std::sync::atomic::{AtomicPtr, Ordering};
use std::sync::Arc;

use symphonia::core::audio::Channels;
use symphonia::core::errors::Error;
use symphonia::core::formats::probe::Hint;
use symphonia::core::formats::{FormatOptions, FormatReader, SeekMode, SeekTo, Track};
use symphonia::core::io::{MediaSource, MediaSourceStream, MediaSourceStreamOptions};
use symphonia::core::meta::MetadataOptions;
use symphonia::core::units::{Time, TimeBase, Timestamp};

#[cxx::bridge(namespace = "media")]
pub mod ffi {
    /// Identifies the media track type.
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    enum SymphoniaTrackType {
        Unknown,
        Audio,
        Video,
        Subtitle,
    }

    /// Identifies the compression format of the media stream.
    ///
    /// NOTE: This enum covers both audio and video codecs demuxed from
    /// containers. TODO(crbug.com/550619039): Unify with
    /// `SymphoniaAudioCodec` in `media/filters/symphonia_decoder_bridge.rs`
    /// via a shared `symphonia_codec.h` extern enum once both bridges are
    /// landed.
    #[repr(i32)]
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    enum SymphoniaCodec {
        Unknown,
        // Audio codecs
        Aac,
        Flac,
        Mp3,
        Opus,
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
        // Video codecs
        Av1,
        H264,
        Theora,
        Vp8,
        Vp9,
    }

    /// Parameters describing an individual media track within a container.
    #[derive(Debug, Clone)]
    struct SymphoniaTrackInfo {
        track_id: u32,
        track_type: SymphoniaTrackType,
        codec: SymphoniaCodec,
        codec_name: String,
        sample_rate: u32,
        channels: u32,
        channel_mask: u32,
        bits_per_sample: u32,
        extra_data: Vec<u8>,
        time_base_num: u32,
        time_base_den: u32,
        duration_us: i64,
        start_time_us: i64,
        delay_frames: u32,
        padding_frames: u32,
        language: String,
        width: u32,
        height: u32,
    }

    /// Status of demuxer initialization.
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    enum SymphoniaDemuxerInitStatus {
        Ok,
        UnsupportedFormat,
        ReadError,
        LimitError,
        DecodeError,
        GenericError,
    }

    /// Result returned by `init_symphonia_demuxer`.
    struct SymphoniaDemuxerInitResult {
        status: SymphoniaDemuxerInitStatus,
        error_str: String,
        container_name: String,
        duration_us: i64,
        tracks: Vec<SymphoniaTrackInfo>,
        demuxer: Box<SymphoniaDemuxerBridge>,
    }

    /// Represents an encoded packet demuxed from the media container.
    #[derive(Debug, Clone)]
    struct SymphoniaDemuxerPacket {
        track_id: u32,
        pts_us: i64,
        dts_us: i64,
        duration_us: i64,
        is_keyframe: bool,
        trim_start_us: i64,
        trim_end_us: i64,
        data: Vec<u8>,
    }

    /// Status of a packet read operation.
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    enum SymphoniaReadPacketStatus {
        Ok,
        EndOfStream,
        ResetRequired,
        Error,
    }

    /// Result returned by `read_packet`.
    struct SymphoniaReadPacketResult {
        status: SymphoniaReadPacketStatus,
        error_str: String,
        packet: SymphoniaDemuxerPacket,
    }

    /// Status of a seek operation.
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    enum SymphoniaSeekStatus {
        Ok,
        Error,
        OutOfRange,
    }

    /// Result returned by `seek_to`.
    struct SymphoniaSeekResult {
        status: SymphoniaSeekStatus,
        actual_time_us: i64,
        error_str: String,
    }

    unsafe extern "C++" {
        include!("media/filters/symphonia_source_bridge.h");
        type SymphoniaSourceBridge;

        #[cxx_name = "Read"]
        fn read(self: Pin<&mut SymphoniaSourceBridge>, buf: &mut [u8]) -> i64;
        #[cxx_name = "Seek"]
        fn seek(self: Pin<&mut SymphoniaSourceBridge>, pos: u64) -> bool;
        #[cxx_name = "GetPosition"]
        fn get_position(self: &SymphoniaSourceBridge) -> u64;
        #[cxx_name = "GetLength"]
        fn get_length(self: &SymphoniaSourceBridge) -> i64;
        #[cxx_name = "IsSeekable"]
        fn is_seekable(self: &SymphoniaSourceBridge) -> bool;
    }

    extern "Rust" {
        type SymphoniaDemuxerBridge;

        fn init_symphonia_demuxer(
            source: Pin<&mut SymphoniaSourceBridge>,
            hint_extension: &str,
            hint_mime: &str,
        ) -> SymphoniaDemuxerInitResult;

        fn read_packet(
            self: &mut SymphoniaDemuxerBridge,
            source: Pin<&mut SymphoniaSourceBridge>,
        ) -> SymphoniaReadPacketResult;

        fn seek_to(
            self: &mut SymphoniaDemuxerBridge,
            source: Pin<&mut SymphoniaSourceBridge>,
            time_us: u64,
            track_id: i32,
        ) -> SymphoniaSeekResult;

        fn container_name(&self) -> &str;
        fn duration_us(&self) -> i64;
        fn tracks(&self) -> Vec<SymphoniaTrackInfo>;
    }
}

/// RAII guard that temporarily binds a C++ `SymphoniaSourceBridge` reference to
/// a `SourceHandle` for the duration of a call, resetting the handle's pointer
/// to null on drop to avoid dangling pointers and UAF.
pub struct SourceScope {
    handle: SourceHandle,
}

impl Drop for SourceScope {
    fn drop(&mut self) {
        self.handle.reset();
    }
}

/// Shared source pointer wrapper connecting Symphonia's `MediaSource` to
/// `SymphoniaSourceBridge`.
#[derive(Clone)]
pub struct SourceHandle {
    ptr: Arc<AtomicPtr<ffi::SymphoniaSourceBridge>>,
}

impl SourceHandle {
    pub fn new() -> Self {
        Self { ptr: Arc::new(AtomicPtr::new(std::ptr::null_mut())) }
    }

    pub fn reset(&self) {
        self.ptr.store(std::ptr::null_mut(), Ordering::Release);
    }

    /// Temporarily attaches the C++ source bridge to this handle.
    /// The returned `SourceScope` resets the pointer to null when dropped.
    pub fn bind(&self, bridge: Pin<&mut ffi::SymphoniaSourceBridge>) -> SourceScope {
        // SAFETY: The Pin guarantees the reference target exists. Storing the
        // raw pointer allows the MediaSource implementation to access
        // the C++ bridge during the active FFI call. The returned
        // SourceScope RAII guard ensures the pointer is reset to null
        // immediately upon exiting the call scope.
        let raw = unsafe { bridge.get_unchecked_mut() as *mut ffi::SymphoniaSourceBridge };
        self.ptr.store(raw, Ordering::Release);
        SourceScope { handle: self.clone() }
    }

    pub fn with_bridge<F, R>(&self, f: F) -> io::Result<R>
    where
        F: FnOnce(Pin<&mut ffi::SymphoniaSourceBridge>) -> io::Result<R>,
    {
        let raw = self.ptr.load(Ordering::Acquire);
        if raw.is_null() {
            return Err(io::Error::new(io::ErrorKind::NotConnected, "Source bridge not available"));
        }
        // SAFETY: The pointer was initialized from a valid Pin<&mut> during the
        // active call scope and remains valid and exclusively accessed
        // on this thread sequence for the duration of `with_bridge`.
        let pinned = unsafe { Pin::new_unchecked(&mut *raw) };
        f(pinned)
    }
}

impl Default for SourceHandle {
    fn default() -> Self {
        Self::new()
    }
}

/// Implements Symphonia's `MediaSource` over `SymphoniaSourceBridge`.
struct SymphoniaSourceAdapter {
    handle: SourceHandle,
}

impl Read for SymphoniaSourceAdapter {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.handle.with_bridge(|mut bridge| {
            let bytes_read = bridge.as_mut().read(buf);
            if bytes_read < 0 {
                return Err(io::Error::other("Failed to read from C++ source bridge"));
            }
            Ok(bytes_read as usize)
        })
    }
}

impl Seek for SymphoniaSourceAdapter {
    fn seek(&mut self, pos: SeekFrom) -> io::Result<u64> {
        self.handle.with_bridge(|mut bridge| {
            let target_pos = match pos {
                SeekFrom::Start(offset) => offset,
                SeekFrom::Current(offset) => {
                    bridge.get_position().checked_add_signed(offset).ok_or_else(|| {
                        io::Error::new(io::ErrorKind::InvalidInput, "Seek out of bounds")
                    })?
                }
                SeekFrom::End(offset) => {
                    let total_len = bridge.get_length();
                    if total_len < 0 {
                        return Err(io::Error::new(
                            io::ErrorKind::Unsupported,
                            "Cannot seek from end on stream with unknown length",
                        ));
                    }
                    (total_len as u64).checked_add_signed(offset).ok_or_else(|| {
                        io::Error::new(io::ErrorKind::InvalidInput, "Seek out of bounds")
                    })?
                }
            };

            if !bridge.as_mut().seek(target_pos) {
                return Err(io::Error::other("Seek failed on C++ source bridge"));
            }
            Ok(target_pos)
        })
    }
}

impl MediaSource for SymphoniaSourceAdapter {
    fn is_seekable(&self) -> bool {
        self.handle.with_bridge(|bridge| Ok(bridge.is_seekable())).unwrap_or(false)
    }

    fn byte_len(&self) -> Option<u64> {
        self.handle
            .with_bridge(|bridge| Ok(u64::try_from(bridge.get_length()).ok()))
            .unwrap_or(None)
    }
}

/// Maps Symphonia codec parameters to bridge track type, codec type, and codec
/// name.
pub fn map_codec_params(
    params: &symphonia::core::codecs::CodecParameters,
) -> (ffi::SymphoniaTrackType, ffi::SymphoniaCodec, &'static str) {
    use symphonia::core::codecs::audio::well_known::*;
    use symphonia::core::codecs::video::well_known::*;

    match params {
        symphonia::core::codecs::CodecParameters::Audio(audio_params) => {
            let (codec, name) = match audio_params.codec {
                CODEC_ID_AAC => (ffi::SymphoniaCodec::Aac, "aac"),
                CODEC_ID_FLAC => (ffi::SymphoniaCodec::Flac, "flac"),
                CODEC_ID_MP3 => (ffi::SymphoniaCodec::Mp3, "mp3"),
                CODEC_ID_OPUS => (ffi::SymphoniaCodec::Opus, "opus"),
                CODEC_ID_VORBIS => (ffi::SymphoniaCodec::Vorbis, "vorbis"),
                CODEC_ID_PCM_ALAW => (ffi::SymphoniaCodec::PcmAlaw, "pcm_alaw"),
                CODEC_ID_PCM_MULAW => (ffi::SymphoniaCodec::PcmMulaw, "pcm_mulaw"),
                CODEC_ID_PCM_F32LE => (ffi::SymphoniaCodec::PcmF32, "pcm_f32le"),
                CODEC_ID_PCM_F32LE_PLANAR => {
                    (ffi::SymphoniaCodec::PcmF32Planar, "pcm_f32le_planar")
                }
                CODEC_ID_PCM_S16LE => (ffi::SymphoniaCodec::PcmS16, "pcm_s16le"),
                CODEC_ID_PCM_S16BE => (ffi::SymphoniaCodec::PcmS16be, "pcm_s16be"),
                CODEC_ID_PCM_S16LE_PLANAR => {
                    (ffi::SymphoniaCodec::PcmS16Planar, "pcm_s16le_planar")
                }
                CODEC_ID_PCM_S24LE => (ffi::SymphoniaCodec::PcmS24, "pcm_s24le"),
                CODEC_ID_PCM_S24BE => (ffi::SymphoniaCodec::PcmS24be, "pcm_s24be"),
                CODEC_ID_PCM_S32LE => (ffi::SymphoniaCodec::PcmS32, "pcm_s32le"),
                CODEC_ID_PCM_S32LE_PLANAR => {
                    (ffi::SymphoniaCodec::PcmS32Planar, "pcm_s32le_planar")
                }
                CODEC_ID_PCM_U8 => (ffi::SymphoniaCodec::PcmU8, "pcm_u8"),
                CODEC_ID_PCM_U8_PLANAR => (ffi::SymphoniaCodec::PcmU8Planar, "pcm_u8_planar"),
                _ => (ffi::SymphoniaCodec::Unknown, "unknown_audio"),
            };
            (ffi::SymphoniaTrackType::Audio, codec, name)
        }
        symphonia::core::codecs::CodecParameters::Video(video_params) => {
            let (codec, name) = match video_params.codec {
                CODEC_ID_AV1 => (ffi::SymphoniaCodec::Av1, "av1"),
                CODEC_ID_H264 => (ffi::SymphoniaCodec::H264, "h264"),
                CODEC_ID_THEORA => (ffi::SymphoniaCodec::Theora, "theora"),
                CODEC_ID_VP8 => (ffi::SymphoniaCodec::Vp8, "vp8"),
                CODEC_ID_VP9 => (ffi::SymphoniaCodec::Vp9, "vp9"),
                _ => (ffi::SymphoniaCodec::Unknown, "unknown_video"),
            };
            (ffi::SymphoniaTrackType::Video, codec, name)
        }
        _ => (ffi::SymphoniaTrackType::Unknown, ffi::SymphoniaCodec::Unknown, "unknown"),
    }
}

impl Default for ffi::SymphoniaTrackInfo {
    fn default() -> Self {
        Self {
            track_id: 0,
            track_type: ffi::SymphoniaTrackType::Unknown,
            codec: ffi::SymphoniaCodec::Unknown,
            codec_name: String::new(),
            sample_rate: 0,
            channels: 0,
            channel_mask: 0,
            bits_per_sample: 0,
            extra_data: Vec::new(),
            time_base_num: 0,
            time_base_den: 0,
            duration_us: 0,
            start_time_us: 0,
            delay_frames: 0,
            padding_frames: 0,
            language: String::new(),
            width: 0,
            height: 0,
        }
    }
}

/// Calculates a timestamp in microseconds for a given optional timebase.
#[inline]
pub fn calc_timestamp_us(timebase: Option<TimeBase>, ts: Timestamp) -> i64 {
    timebase.map(|tb| tb.calc_time_saturating(ts).as_micros() as i64).unwrap_or(0)
}

/// Calculates duration in microseconds for a given frame count and optional
/// timebase.
#[inline]
pub fn calc_duration_us(timebase: Option<TimeBase>, frames: u64) -> i64 {
    calc_timestamp_us(timebase, Timestamp::new(frames as i64))
}

/// Converts a Symphonia `Track` into `ffi::SymphoniaTrackInfo`.
pub fn convert_track_info(track: &Track) -> ffi::SymphoniaTrackInfo {
    let (time_base_num, time_base_den) =
        track.time_base.map_or((0, 0), |tb| (tb.numer.get(), tb.denom.get()));

    let duration_us = track.duration.map_or(0, |dur| calc_duration_us(track.time_base, dur.get()));

    let start_time_us = calc_timestamp_us(track.time_base, track.start_ts);

    let (track_type, codec, name) = track.codec_params.as_ref().map_or(
        (ffi::SymphoniaTrackType::Unknown, ffi::SymphoniaCodec::Unknown, "unknown"),
        map_codec_params,
    );

    let mut track_info = ffi::SymphoniaTrackInfo {
        track_id: track.id,
        track_type,
        codec,
        codec_name: name.to_string(),
        time_base_num,
        time_base_den,
        duration_us,
        start_time_us,
        delay_frames: track.delay.unwrap_or(0),
        padding_frames: track.padding.unwrap_or(0),
        language: track.language.clone().unwrap_or_default(),
        ..Default::default()
    };

    if let Some(ref codec_params) = track.codec_params {
        match codec_params {
            symphonia::core::codecs::CodecParameters::Audio(audio_params) => {
                track_info.sample_rate = audio_params.sample_rate.unwrap_or(0);
                if let Some(ref ch) = audio_params.channels {
                    track_info.channels = ch.count() as u32;
                    track_info.channel_mask = match ch {
                        Channels::Positioned(pos) => pos.bits() as u32,
                        _ => 0,
                    };
                }
                track_info.bits_per_sample = audio_params.bits_per_sample.unwrap_or(0);
                if let Some(ref extra) = audio_params.extra_data {
                    track_info.extra_data = extra.to_vec();
                }
            }
            symphonia::core::codecs::CodecParameters::Video(video_params) => {
                track_info.width = video_params.width.unwrap_or(0) as u32;
                track_info.height = video_params.height.unwrap_or(0) as u32;
                if let Some(first_extra) = video_params.extra_data.first() {
                    track_info.extra_data = first_extra.data.to_vec();
                }
            }
            _ => {}
        }
    }

    track_info
}

/// The main demuxer bridge holding the underlying Symphonia `FormatReader`.
pub struct SymphoniaDemuxerBridge {
    reader: Option<Box<dyn FormatReader>>,
    source_handle: Option<SourceHandle>,
    container_name: String,
    duration_us: i64,
    tracks: Vec<ffi::SymphoniaTrackInfo>,
    timebases: HashMap<u32, TimeBase>,
}

impl ffi::SymphoniaDemuxerPacket {
    fn empty() -> Self {
        Self {
            track_id: 0,
            pts_us: 0,
            dts_us: 0,
            duration_us: 0,
            is_keyframe: false,
            trim_start_us: 0,
            trim_end_us: 0,
            data: Vec::new(),
        }
    }
}

impl ffi::SymphoniaReadPacketResult {
    fn ok(packet: ffi::SymphoniaDemuxerPacket) -> Self {
        Self { status: ffi::SymphoniaReadPacketStatus::Ok, error_str: String::new(), packet }
    }

    fn err(status: ffi::SymphoniaReadPacketStatus, error_str: String) -> Self {
        Self { status, error_str, packet: ffi::SymphoniaDemuxerPacket::empty() }
    }
}

impl ffi::SymphoniaSeekResult {
    fn ok(actual_time_us: i64) -> Self {
        Self { status: ffi::SymphoniaSeekStatus::Ok, actual_time_us, error_str: String::new() }
    }

    fn err(status: ffi::SymphoniaSeekStatus, error_str: String) -> Self {
        Self { status, actual_time_us: 0, error_str }
    }
}

/// Maps a Symphonia probing `Error` to its FFI status and human-readable
/// message.
pub fn to_demuxer_init_status(err: &Error) -> (ffi::SymphoniaDemuxerInitStatus, String) {
    match err {
        Error::Unsupported(s) => (
            ffi::SymphoniaDemuxerInitStatus::UnsupportedFormat,
            format!("Unsupported container format: {s}"),
        ),
        Error::IoError(err) => (
            ffi::SymphoniaDemuxerInitStatus::ReadError,
            format!("IO error during probing: {err:?}"),
        ),
        Error::LimitError(s) => (
            ffi::SymphoniaDemuxerInitStatus::LimitError,
            format!("Limit error during probing: {s}"),
        ),
        Error::DecodeError(s) => (
            ffi::SymphoniaDemuxerInitStatus::DecodeError,
            format!("Decode error during probing: {s}"),
        ),
        err => (ffi::SymphoniaDemuxerInitStatus::GenericError, format!("Probing failed: {err:?}")),
    }
}

impl ffi::SymphoniaDemuxerInitResult {
    fn ok(
        container_name: String,
        duration_us: i64,
        tracks: Vec<ffi::SymphoniaTrackInfo>,
        demuxer: Box<SymphoniaDemuxerBridge>,
    ) -> Self {
        Self {
            status: ffi::SymphoniaDemuxerInitStatus::Ok,
            error_str: String::new(),
            container_name,
            duration_us,
            tracks,
            demuxer,
        }
    }

    fn err(status: ffi::SymphoniaDemuxerInitStatus, error_str: String) -> Self {
        Self {
            status,
            error_str,
            container_name: String::new(),
            duration_us: 0,
            tracks: Vec::new(),
            demuxer: Box::new(SymphoniaDemuxerBridge::empty()),
        }
    }
}

impl From<&Error> for ffi::SymphoniaDemuxerInitResult {
    fn from(err: &Error) -> Self {
        let (status, error_str) = to_demuxer_init_status(err);
        Self::err(status, error_str)
    }
}

impl From<Error> for ffi::SymphoniaDemuxerInitResult {
    fn from(err: Error) -> Self {
        Self::from(&err)
    }
}

impl SymphoniaDemuxerBridge {
    fn empty() -> Self {
        Self {
            reader: None,
            source_handle: None,
            container_name: String::new(),
            duration_us: 0,
            tracks: Vec::new(),
            timebases: HashMap::new(),
        }
    }

    pub fn container_name(&self) -> &str {
        &self.container_name
    }

    pub fn duration_us(&self) -> i64 {
        self.duration_us
    }

    pub fn tracks(&self) -> Vec<ffi::SymphoniaTrackInfo> {
        self.tracks.clone()
    }

    pub fn read_packet(
        &mut self,
        source: Pin<&mut ffi::SymphoniaSourceBridge>,
    ) -> ffi::SymphoniaReadPacketResult {
        let _source_guard = self.source_handle.as_ref().map(|h| h.bind(source));

        let reader = match self.reader.as_mut() {
            Some(r) => r,
            None => {
                return ffi::SymphoniaReadPacketResult::err(
                    ffi::SymphoniaReadPacketStatus::Error,
                    "Demuxer is not initialized".to_string(),
                );
            }
        };

        match reader.next_packet() {
            Ok(Some(packet)) => {
                let track_id = packet.track_id;
                let timebase = self.timebases.get(&track_id).copied();

                let pts_us = calc_timestamp_us(timebase, packet.pts);
                let dts_us = if packet.dts == packet.pts {
                    pts_us
                } else {
                    calc_timestamp_us(timebase, packet.dts)
                };
                let duration_us = calc_duration_us(timebase, packet.dur.get());
                let trim_start_us = calc_duration_us(timebase, packet.trim_start.get());
                let trim_end_us = calc_duration_us(timebase, packet.trim_end.get());

                // Audio frames are always random access points (keyframe =
                // true). TODO(crbug.com/550619039): Inspect
                // container packet flags for video tracks
                // (MKV/MP4).
                let is_keyframe = true;

                ffi::SymphoniaReadPacketResult::ok(ffi::SymphoniaDemuxerPacket {
                    track_id,
                    pts_us,
                    dts_us,
                    duration_us,
                    is_keyframe,
                    trim_start_us,
                    trim_end_us,
                    data: packet.data.into_vec(),
                })
            }
            Ok(None) => ffi::SymphoniaReadPacketResult::err(
                ffi::SymphoniaReadPacketStatus::EndOfStream,
                String::new(),
            ),
            Err(Error::ResetRequired) => ffi::SymphoniaReadPacketResult::err(
                ffi::SymphoniaReadPacketStatus::ResetRequired,
                "Format reader reset required".to_string(),
            ),
            Err(err) => ffi::SymphoniaReadPacketResult::err(
                ffi::SymphoniaReadPacketStatus::Error,
                format!("Failed to read packet: {err:?}"),
            ),
        }
    }

    pub fn seek_to(
        &mut self,
        source: Pin<&mut ffi::SymphoniaSourceBridge>,
        time_us: u64,
        track_id: i32,
    ) -> ffi::SymphoniaSeekResult {
        let _source_guard = self.source_handle.as_ref().map(|h| h.bind(source));

        let reader = match self.reader.as_mut() {
            Some(r) => r,
            None => {
                return ffi::SymphoniaSeekResult::err(
                    ffi::SymphoniaSeekStatus::Error,
                    "Demuxer is not initialized".to_string(),
                );
            }
        };

        // If track_id >= 0, seek specifically taking into account that track's
        // index / keyframes. A negative track_id (e.g. -1) seeks across any
        // track.
        let seek_to = SeekTo::Time {
            time: Time::from_micros_u64(time_us),
            track_id: if track_id >= 0 { Some(track_id as u32) } else { None },
        };

        match reader.seek(SeekMode::Accurate, seek_to) {
            Ok(seeked_to) => {
                let actual_time_us = self
                    .timebases
                    .get(&seeked_to.track_id)
                    .map(|&tb| calc_timestamp_us(Some(tb), seeked_to.actual_ts))
                    .unwrap_or(time_us as i64);

                ffi::SymphoniaSeekResult::ok(actual_time_us)
            }
            Err(Error::SeekError(seek_err)) => ffi::SymphoniaSeekResult::err(
                ffi::SymphoniaSeekStatus::OutOfRange,
                format!("Seek out of range: {seek_err:?}"),
            ),
            Err(err) => ffi::SymphoniaSeekResult::err(
                ffi::SymphoniaSeekStatus::Error,
                format!("Seek failed: {err:?}"),
            ),
        }
    }
}

impl Drop for SymphoniaDemuxerBridge {
    fn drop(&mut self) {
        if let Some(ref handle) = self.source_handle {
            handle.reset();
        }
    }
}

/// Initializes a Symphonia demuxer from a `SymphoniaSourceBridge`.
pub fn init_symphonia_demuxer(
    source: Pin<&mut ffi::SymphoniaSourceBridge>,
    hint_extension: &str,
    hint_mime: &str,
) -> ffi::SymphoniaDemuxerInitResult {
    let source_handle = SourceHandle::new();
    let _source_guard = source_handle.bind(source);
    let adapter = SymphoniaSourceAdapter { handle: source_handle.clone() };

    let mss = MediaSourceStream::new(Box::new(adapter), MediaSourceStreamOptions::default());

    let mut hint = Hint::new();
    if !hint_extension.is_empty() {
        hint.with_extension(hint_extension);
    }
    if !hint_mime.is_empty() {
        hint.mime_type(hint_mime);
    }

    let probe = symphonia::default::get_probe();
    let format_opts = FormatOptions::default();
    let meta_opts = MetadataOptions::default();

    match probe.probe(&hint, mss, format_opts, meta_opts) {
        Ok(reader) => {
            let container_name = reader.format_info().short_name.to_string();

            let mut timebases = HashMap::new();
            let mut tracks = Vec::new();

            for track in reader.tracks() {
                if let Some(tb) = track.time_base {
                    timebases.insert(track.id, tb);
                }
                tracks.push(convert_track_info(track));
            }

            let media_info = reader.media_info();
            let duration_us = media_info
                .duration
                .map(|dur| calc_duration_us(media_info.time_base, dur.get()))
                .unwrap_or_else(|| tracks.iter().map(|t| t.duration_us).max().unwrap_or(0));

            let bridge = SymphoniaDemuxerBridge {
                reader: Some(reader),
                source_handle: Some(source_handle),
                container_name: container_name.clone(),
                duration_us,
                tracks: tracks.clone(),
                timebases,
            };

            ffi::SymphoniaDemuxerInitResult::ok(
                container_name,
                duration_us,
                tracks,
                Box::new(bridge),
            )
        }
        Err(err) => ffi::SymphoniaDemuxerInitResult::from(err),
    }
}
