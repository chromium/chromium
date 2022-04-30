// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_util.h"

#include "base/metrics/histogram_macros.h"

namespace media {

namespace {

// Reported to UMA server. Do not renumber or reuse values.
enum class MediaVideoHeight {
  k360_OrLower,
  k480,
  k720,
  k1080,
  k1440,
  k2160_OrHigher,
  kMaxValue = k2160_OrHigher,
};

MediaVideoHeight GetMediaVideoHeight(int height) {
  if (height <= 400)
    return MediaVideoHeight::k360_OrLower;
  if (height <= 600)
    return MediaVideoHeight::k480;
  if (height <= 900)
    return MediaVideoHeight::k720;
  if (height <= 1260)
    return MediaVideoHeight::k1080;
  if (height <= 1800)
    return MediaVideoHeight::k1440;
  return MediaVideoHeight::k2160_OrHigher;
}

}  // namespace

std::vector<uint8_t> EmptyExtraData() {
  return std::vector<uint8_t>();
}

void ReportPepperVideoDecoderOutputPictureCountHW(int height) {
  UMA_HISTOGRAM_ENUMERATION("Media.PepperVideoDecoderOutputPictureCount.HW",
                            GetMediaVideoHeight(height));
}

void ReportPepperVideoDecoderOutputPictureCountSW(int height) {
  UMA_HISTOGRAM_ENUMERATION("Media.PepperVideoDecoderOutputPictureCount.SW",
                            GetMediaVideoHeight(height));
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
