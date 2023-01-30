// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_util.h"

#include "base/trace_event/trace_event.h"

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
    case AudioCodec::kDTSXP2:
      return AudioParameters::Format::AUDIO_BITSTREAM_DTSX_P2;
      // No support for DTS_HD yet as this section is related to the incoming
      // stream type. DTS_HD support is only added for audio track output to
      // support audiosink reporting DTS_HD support.
    default:
      return AudioParameters::Format::AUDIO_FAKE;
  }
}

bool MediaTraceIsEnabled() {
  bool enable_decode_traces = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("media", &enable_decode_traces);
  return enable_decode_traces;
}
}  // namespace media
