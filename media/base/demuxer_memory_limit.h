// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEMUXER_MEMORY_LIMIT_H_
#define MEDIA_BASE_DEMUXER_MEMORY_LIMIT_H_

#include <stddef.h>

#include "base/byte_size.h"
#include "build/build_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"

namespace media {

// The maximum amount of data (in bytes) a demuxer can keep in memory, for a
// particular type of stream.
MEDIA_EXPORT base::ByteSize GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* audio_config);
MEDIA_EXPORT base::ByteSize GetDemuxerStreamVideoMemoryLimit(
    DemuxerType demuxer_type,
    const VideoDecoderConfig* video_config);

// The maximum amount of data (in bytes) a demuxer can keep in memory overall.
MEDIA_EXPORT base::ByteSize GetDemuxerMemoryLimit(DemuxerType demuxer_type);

namespace internal {

// These values should not be used directly, they are selected by functions
// above based on platform capabilities.

// Default audio memory limit: 12MB (5 minutes of 320Kbps content).
// Medium audio memory limit: 5MB.
// Low audio memory limit: 2MB (1 minute of 256Kbps content).
inline constexpr base::ByteSize kDemuxerStreamAudioMemoryLimitDefault =
    base::MiBU(12);
inline constexpr base::ByteSize kDemuxerStreamAudioMemoryLimitMedium =
    base::MiBU(5);
inline constexpr base::ByteSize kDemuxerStreamAudioMemoryLimitLow =
    base::MiBU(2);

// Default video memory limit: 150MB (5 minutes of 4Mbps content).
// Medium video memory limit: 80MB.
// Low video memory limit: 30MB (1 minute of 4Mbps content).
inline constexpr base::ByteSize kDemuxerStreamVideoMemoryLimitDefault =
    base::MiBU(150);
inline constexpr base::ByteSize kDemuxerStreamVideoMemoryLimitMedium =
    base::MiBU(80);
inline constexpr base::ByteSize kDemuxerStreamVideoMemoryLimitLow =
    base::MiBU(30);

#if BUILDFLAG(IS_ANDROID)
// Special "very low" settings for 512MiB Android Go devices:
// * audio memory limit: 1MB (30 seconds of 256Kbps content).
// * video memory limit: 15MB (30 seconds of 4Mbps content).
inline constexpr base::ByteSize kDemuxerStreamAudioMemoryLimitVeryLow =
    base::MiBU(1);
inline constexpr base::ByteSize kDemuxerStreamVideoMemoryLimitVeryLow =
    base::MiBU(15);
#endif

}  // namespace internal

}  // namespace media

#endif  // MEDIA_BASE_DEMUXER_MEMORY_LIMIT_H_
