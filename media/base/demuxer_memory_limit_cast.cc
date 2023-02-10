// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer_memory_limit.h"

#include "base/check.h"
#include "media/base/channel_layout.h"

namespace media {

size_t GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* audio_config) {
  if (!audio_config) {
    return internal::kDemuxerStreamAudioMemoryLimitLow;
  }

  DCHECK(audio_config->IsValidConfig());
  switch (audio_config->codec()) {
    case AudioCodec::kEAC3:
    case AudioCodec::kAC3:
    case AudioCodec::kDTS:
    case AudioCodec::kDTSXP2:
    case AudioCodec::kDTSE:
    case AudioCodec::kMpegHAudio:
      return internal::kDemuxerStreamAudioMemoryLimitMedium;
    case AudioCodec::kAAC:
      if (ChannelLayoutToChannelCount(audio_config->channel_layout()) >= 5) {
        return internal::kDemuxerStreamAudioMemoryLimitMedium;
      }
      return internal::kDemuxerStreamAudioMemoryLimitLow;
    default:
      return internal::kDemuxerStreamAudioMemoryLimitLow;
  }
}

size_t GetDemuxerStreamVideoMemoryLimit(
    Demuxer::DemuxerTypes demuxer_type,
    const VideoDecoderConfig* video_config) {
  switch (demuxer_type) {
    case Demuxer::DemuxerTypes::kFFmpegDemuxer:
      return internal::kDemuxerStreamVideoMemoryLimitDefault;
    case Demuxer::DemuxerTypes::kChunkDemuxer:
      if (!video_config) {
        return internal::kDemuxerStreamVideoMemoryLimitLow;
      }
      DCHECK(video_config->IsValidConfig());
      switch (video_config->codec()) {
        case VideoCodec::kVP9:
        case VideoCodec::kHEVC:
        case VideoCodec::kDolbyVision:
          return internal::kDemuxerStreamVideoMemoryLimitMedium;
        default:
          return internal::kDemuxerStreamVideoMemoryLimitLow;
      }
    case Demuxer::DemuxerTypes::kMediaUrlDemuxer:
      return internal::kDemuxerStreamVideoMemoryLimitLow;
  }
}

size_t GetDemuxerMemoryLimit(Demuxer::DemuxerTypes demuxer_type) {
  return GetDemuxerStreamAudioMemoryLimit(nullptr) +
         GetDemuxerStreamVideoMemoryLimit(demuxer_type, nullptr);
}

}  // namespace media
