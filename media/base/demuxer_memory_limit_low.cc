// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer_memory_limit.h"

#include "build/build_config.h"

namespace media {

size_t GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* /*audio_config*/) {
  return internal::kDemuxerStreamAudioMemoryLimitLow;
}

size_t GetDemuxerStreamVideoMemoryLimit(
    Demuxer::DemuxerTypes /*demuxer_type*/,
    const VideoDecoderConfig* /*video_config*/) {
  return internal::kDemuxerStreamVideoMemoryLimitLow;
}

size_t GetDemuxerMemoryLimit(Demuxer::DemuxerTypes demuxer_type) {
  return GetDemuxerStreamAudioMemoryLimit(nullptr) +
         GetDemuxerStreamVideoMemoryLimit(demuxer_type, nullptr);
}

}  // namespace media
