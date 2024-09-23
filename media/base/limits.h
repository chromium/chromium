// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains limit definition constants for the media subsystem.

#ifndef MEDIA_BASE_LIMITS_H_
#define MEDIA_BASE_LIMITS_H_

#include "build/build_config.h"
#include "media/media_buildflags.h"

namespace media {

namespace limits {

// Maximum possible dimension (width or height) for any video.
constexpr int kMaxDimension = (1 << 15) - 1;  // 32767

// Maximum possible canvas size (width multiplied by height) for any video.
constexpr int kMaxCanvas = (1 << (14 * 2));  // 16384 x 16384

// Total number of video frames which are populating in the pipeline.
constexpr int kMaxVideoFrames = 4;

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
constexpr int kMaxSampleRate = 768000;
constexpr int kMinSampleRate = 3000;
constexpr int kMaxChannels = 32;
constexpr int kMaxBytesPerSample = 4;
constexpr int kMaxBitsPerSample = kMaxBytesPerSample * 8;
constexpr int kMaxSamplesPerPacket = kMaxSampleRate;
constexpr int kMaxPacketSizeInBytes =
    kMaxBytesPerSample * kMaxChannels * kMaxSamplesPerPacket;

// This limit is used by ParamTraits<VideoCaptureParams>.
constexpr int kMaxFramesPerSecond = 1000;

// The minimum elapsed amount of time (in seconds) for a playback to be
// considered as having active engagement.
constexpr int kMinimumElapsedWatchTimeSecs = 7;

// Maximum lengths for various EME API parameters. These are checks to
// prevent unnecessarily large parameters from being passed around, and the
// lengths are somewhat arbitrary as the EME spec doesn't specify any limits.
constexpr int kMinCertificateLength = 128;
constexpr int kMaxCertificateLength = 16 * 1024;
constexpr int kMaxSessionIdLength = 512;
constexpr int kMinKeyIdLength = 1;
constexpr int kMaxKeyIdLength = 512;
constexpr int kMaxKeyIds = 128;
constexpr int kMaxInitDataLength = 64 * 1024;         // 64 KB
constexpr int kMaxSessionResponseLength = 64 * 1024;  // 64 KB
constexpr int kMaxKeySystemLength = 256;

// Minimum and maximum buffer sizes for certain audio platforms.
#if BUILDFLAG(IS_MAC)
constexpr int kMinAudioBufferSize = 128;
constexpr int kMaxAudioBufferSize = 4096;
#elif BUILDFLAG(USE_CRAS)
// Though CRAS has different per-board defaults, allow explicitly requesting
// this buffer size on any board.
constexpr int kMinAudioBufferSize = 256;
constexpr int kMaxAudioBufferSize = 8192;
#endif

// Maximum buffer size supported by Web Audio.
constexpr int kMaxWebAudioBufferSize = 8192;

// Bounds for the number of threads used for software video decoding.
constexpr int kMinVideoDecodeThreads = 2;
constexpr int kMaxVideoDecodeThreads =
    16;  // Matches ffmpeg's MAX_AUTO_THREADS. Higher values can result in
         // immediate out of memory errors for high resolution content. See
         // https://crbug.com/893984

}  // namespace limits

}  // namespace media

#endif  // MEDIA_BASE_LIMITS_H_
