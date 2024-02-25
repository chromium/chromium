// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_log_properties.h"

#include <string>

namespace media {

std::string MediaLogPropertyKeyToString(MediaLogProperty property) {
#define STRINGIFY(value)        \
  case MediaLogProperty::value: \
    return #value
  switch (property) {
    STRINGIFY(kResolution);
    STRINGIFY(kTotalBytes);
    STRINGIFY(kBitrate);
    STRINGIFY(kMaxDuration);
    STRINGIFY(kStartTime);
    STRINGIFY(kSetCdm);
    STRINGIFY(kIsCdmAttached);
    STRINGIFY(kIsStreaming);
    STRINGIFY(kFrameUrl);
    STRINGIFY(kFrameTitle);
    STRINGIFY(kIsSingleOrigin);
    STRINGIFY(kRendererName);
    STRINGIFY(kVideoDecoderName);
    STRINGIFY(kIsPlatformVideoDecoder);
    STRINGIFY(kIsRangeHeaderSupported);
    STRINGIFY(kIsVideoDecryptingDemuxerStream);
    STRINGIFY(kIsAudioDecryptingDemuxerStream);
    STRINGIFY(kVideoEncoderName);
    STRINGIFY(kIsPlatformVideoEncoder);
    STRINGIFY(kAudioDecoderName);
    STRINGIFY(kIsPlatformAudioDecoder);
    STRINGIFY(kAudioTracks);
    STRINGIFY(kVideoTracks);
    STRINGIFY(kFramerate);
    STRINGIFY(kVideoPlaybackRoughness);
    STRINGIFY(kVideoPlaybackFreezing);
  }
#undef STRINGIFY
}

}  // namespace media
