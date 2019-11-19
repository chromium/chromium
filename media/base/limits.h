// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains limit definition constants for the media subsystem.

#ifndef MEDIA_BASE_LIMITS_H_
#define MEDIA_BASE_LIMITS_H_

#include "build/build_config.h"

namespace media {

namespace limits {

enum {
  // Maximum possible dimension (width or height) for any video.
  kMaxDimension = (1 << 15) - 1,  // 32767

  // Maximum possible canvas size (width multiplied by height) for any video.
  kMaxCanvas = (1 << (14 * 2)),  // 16384 x 16384

  // Total number of video frames which are populating in the pipeline.
  kMaxVideoFrames = 4,

  // The following limits are used by AudioParameters::IsValid().
  //
  // A few notes on sample rates of common formats:
  //   - AAC files are limited to 96 kHz.
  //   - MP3 files are limited to 48 kHz.
  //   - Vorbis used to be limited to 96 kHz, but no longer has that
  //     restriction.
  //   - Most PC audio hardware is limited to 192 kHz, some specialized DAC
  //     devices will use 384 kHz though.
  kMaxSampleRate = 384000,
  kMinSampleRate = 3000,
  kMaxChannels = 32,
  kMaxBytesPerSample = 4,
  kMaxBitsPerSample = kMaxBytesPerSample * 8,
  kMaxSamplesPerPacket = kMaxSampleRate,
  kMaxPacketSizeInBytes =
      kMaxBytesPerSample * kMaxChannels * kMaxSamplesPerPacket,

  // This limit is used by ParamTraits<VideoCaptureParams>.
  kMaxFramesPerSecond = 1000,

  // The minimum elapsed amount of time (in seconds) for a playback to be
  // considered as having active engagement.
  kMinimumElapsedWatchTimeSecs = 7,

  // Maximum lengths for various EME API parameters. These are checks to
  // prevent unnecessarily large parameters from being passed around, and the
  // lengths are somewhat arbitrary as the EME spec doesn't specify any limits.
  kMinCertificateLength = 128,
  kMaxCertificateLength = 16 * 1024,
  kMaxSessionIdLength = 512,
  kMinKeyIdLength = 1,
  kMaxKeyIdLength = 512,
  kMaxKeyIds = 128,
  kMaxInitDataLength = 64 * 1024,         // 64 KB
  kMaxSessionResponseLength = 64 * 1024,  // 64 KB
  kMaxKeySystemLength = 256,

// Minimum and maximum buffer sizes for certain audio platforms.
#if defined(OS_MACOSX)
  kMinAudioBufferSize = 128,
  kMaxAudioBufferSize = 4096,
#elif defined(USE_CRAS)
  // Though CRAS has different per-board defaults, allow explicitly requesting
  // this buffer size on any board.
  kMinAudioBufferSize = 256,
  kMaxAudioBufferSize = 8192,
#endif

  // Maximum buffer size supported by Web Audio.
  kMaxWebAudioBufferSize = 8192,

  // Bounds for the number of threads used for software video decoding.
  kMinVideoDecodeThreads = 2,
  kMaxVideoDecodeThreads =
      16,  // Matches ffmpeg's MAX_AUTO_THREADS. Higher values can result in
           // immediate out of memory errors for high resolution content. See
           // https://crbug.com/893984
};

}  // namespace limits

}  // namespace media

#endif  // MEDIA_BASE_LIMITS_H_
