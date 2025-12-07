// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer_memory_limit.h"

#include "base/android/android_info.h"
#include "base/system/sys_info.h"

namespace media {

namespace {

base::ByteCount SelectLimit(base::ByteCount default_limit,
                            base::ByteCount medium_limit,
                            base::ByteCount low_limit,
                            base::ByteCount very_low_limit) {
  // This is truly for only for low end devices since it will have impacts on
  // the ability to buffer and play HD+ content.
  if (!base::SysInfo::IsLowEndDevice()) {
    return base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled()
               ? medium_limit
               : default_limit;
  }
  // Use very low limit on 512MiB Android Go devices only.
  if (base::android::android_info::sdk_int() >=
          base::android::android_info::SDK_VERSION_OREO &&
      base::SysInfo::AmountOfPhysicalMemory().InMiB() <= 512) {
    return very_low_limit;
  }
  return low_limit;
}

}  // namespace

base::ByteCount GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* /*audio_config*/) {
  static const base::ByteCount limit =
      SelectLimit(internal::kDemuxerStreamAudioMemoryLimitDefault,
                  internal::kDemuxerStreamAudioMemoryLimitMedium,
                  internal::kDemuxerStreamAudioMemoryLimitLow,
                  internal::kDemuxerStreamAudioMemoryLimitVeryLow);
  return limit;
}

base::ByteCount GetDemuxerStreamVideoMemoryLimit(
    DemuxerType /*demuxer_type*/,
    const VideoDecoderConfig* /*video_config*/) {
  static const base::ByteCount limit =
      SelectLimit(internal::kDemuxerStreamVideoMemoryLimitDefault,
                  internal::kDemuxerStreamVideoMemoryLimitMedium,
                  internal::kDemuxerStreamVideoMemoryLimitLow,
                  internal::kDemuxerStreamVideoMemoryLimitVeryLow);
  return limit;
}

base::ByteCount GetDemuxerMemoryLimit(DemuxerType demuxer_type) {
  return GetDemuxerStreamAudioMemoryLimit(nullptr) +
         GetDemuxerStreamVideoMemoryLimit(demuxer_type, nullptr);
}

}  // namespace media
