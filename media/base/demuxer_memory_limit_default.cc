// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer_memory_limit.h"

#include "base/system/sys_info.h"

namespace media {

size_t GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* /*audio_config*/) {
  return base::SysInfo::IsLowEndDevice()
             ? internal::kDemuxerStreamAudioMemoryLimitLow
             : internal::kDemuxerStreamAudioMemoryLimitDefault;
}

size_t GetDemuxerStreamVideoMemoryLimit(
    Demuxer::DemuxerTypes /*demuxer_type*/,
    const VideoDecoderConfig* /*video_config*/) {
  return base::SysInfo::IsLowEndDevice()
             ? internal::kDemuxerStreamVideoMemoryLimitLow
             : internal::kDemuxerStreamVideoMemoryLimitDefault;
}

size_t GetDemuxerMemoryLimit(Demuxer::DemuxerTypes demuxer_type) {
  return GetDemuxerStreamAudioMemoryLimit(nullptr) +
         GetDemuxerStreamVideoMemoryLimit(demuxer_type, nullptr);
}

}  // namespace media
