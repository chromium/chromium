// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains limit definition constants for the media subsystem.

#ifndef MEDIA_BASE_LIMITS_H_
#define MEDIA_BASE_LIMITS_H_

#include "build/build_config.h"
#include "media/media_buildflags.h"

namespace media::limits {

// Maximum possible dimension (width or height) for any video.
inline constexpr int kMaxDimension = (1 << 15) - 1;  // 32767

// Maximum possible canvas size (width multiplied by height) for any video.
// This lets us represent up to 8 bytes per pixel without overflowing uint32_t.
//
// The actual allocation limit on 32-bit platforms is ~6 bytes per pixel since
// we're unable to allocate more than 2gb, so creating frames above this limit
// will fail (gracefully) if allocation is actually attempted.
inline constexpr int kMaxCanvas = (1 << 29) - 1;  // 23170 x 23170

// Total number of video frames which are populating in the pipeline.
inline constexpr int kMaxVideoFrames = 4;

// The following limits are used by AudioParameters::IsValid().
//
// A few notes on sample rates of common formats:
//   - AAC files are limited to 96 kHz.
//   - MP3 files are limited to 48 kHz.
//   - Vorbis used to be limited to 96 kHz, but no longer has that
//     restriction.
//   - Most PC audio hardware is limited to 192 kHz, some specialized DAC
//     devices will use 768 kHz though.
//
// kMaxSampleRate should be updated with
// blink::audio_utilities::MaxAudioBufferSampleRate()
inline constexpr int kMaxSampleRate = 768000;
inline constexpr int kMinSampleRate = 3000;
inline constexpr int kMaxChannels = 32;
inline constexpr int kMaxBytesPerSample = 4;
inline constexpr int kMaxBitsPerSample = kMaxBytesPerSample * 8;
inline constexpr int kMaxSamplesPerPacket = kMaxSampleRate;
inline constexpr int kMaxPacketSizeInBytes =
    kMaxBytesPerSample * kMaxChannels * kMaxSamplesPerPacket;

// This limit is used by ParamTraits<VideoCaptureParams>.
inline constexpr int kMaxFramesPerSecond = 1000;

// The minimum elapsed amount of time (in seconds) for a playback to be
// considered as having active engagement.
inline constexpr int kMinimumElapsedWatchTimeSecs = 7;

// Maximum lengths for various EME API parameters. These are checks to
// prevent unnecessarily large parameters from being passed around, and the
// lengths are somewhat arbitrary as the EME spec doesn't specify any limits.
inline constexpr int kMinCertificateLength = 128;
inline constexpr int kMaxCertificateLength = 16 * 1024;
inline constexpr int kMaxSessionIdLength = 512;
inline constexpr int kMinKeyIdLength = 1;
inline constexpr int kMaxKeyIdLength = 512;
inline constexpr int kMaxKeyIds = 128;
inline constexpr int kMaxInitDataLength = 64 * 1024;         // 64 KB
inline constexpr int kMaxSessionResponseLength = 64 * 1024;  // 64 KB
inline constexpr int kMaxKeySystemLength = 256;

// Minimum and maximum buffer sizes for certain audio platforms.
#if BUILDFLAG(IS_MAC)
inline constexpr int kMinAudioBufferSize = 128;
inline constexpr int kMaxAudioBufferSize = 4096;
#elif BUILDFLAG(USE_CRAS)
// Though CRAS has different per-board defaults, allow explicitly requesting
// this buffer size on any board.
inline constexpr int kMinAudioBufferSize = 256;
inline constexpr int kMaxAudioBufferSize = 8192;
#endif

// Maximum buffer size supported by Web Audio.
inline constexpr int kMaxWebAudioBufferSize = 8192;

// Bounds for the number of threads used for software video decoding.
inline constexpr int kMinVideoDecodeThreads = 2;
inline constexpr int kMaxVideoDecodeThreads =
    16;  // Matches ffmpeg's MAX_AUTO_THREADS. Higher values can result in
         // immediate out of memory errors for high resolution content. See
         // https://crbug.com/893984

}  // namespace media::limits

#endif  // MEDIA_BASE_LIMITS_H_
