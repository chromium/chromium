// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_util.h"

namespace media {

std::vector<uint8_t> EmptyExtraData() {
  return std::vector<uint8_t>();
}

AudioParameters::Format ConvertAudioCodecToBitstreamFormat(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kAC3:
      return AudioParameters::Format::AUDIO_BITSTREAM_AC3;
    case AudioCodec::kEAC3:
      return AudioParameters::Format::AUDIO_BITSTREAM_EAC3;
    case AudioCodec::kDTS:
      return AudioParameters::Format::AUDIO_BITSTREAM_DTS;
      // No support for DTS_HD yet as this section is related to the incoming
      // stream type. DTS_HD support is only added for audio track output to
      // support audiosink reporting DTS_HD support.
    default:
      return AudioParameters::Format::AUDIO_FAKE;
  }
}

}  // namespace media
