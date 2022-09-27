// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_fallback_recorder.h"

#include "base/metrics/histogram_functions.h"

namespace blink {

void RecordRTCVideoDecoderFallbackReason(
    media::VideoCodec codec,
    RTCVideoDecoderFallbackReason fallback_reason) {
  switch (codec) {
    case media::VideoCodec::kH264:
      base::UmaHistogramEnumeration("Media.RTCVideoDecoderFallbackReason.H264",
                                    fallback_reason);
      break;
    case media::VideoCodec::kVP8:
      base::UmaHistogramEnumeration("Media.RTCVideoDecoderFallbackReason.Vp8",
                                    fallback_reason);
      break;
    case media::VideoCodec::kVP9:
      base::UmaHistogramEnumeration("Media.RTCVideoDecoderFallbackReason.Vp9",
                                    fallback_reason);
      break;
    default:
      base::UmaHistogramEnumeration("Media.RTCVideoDecoderFallbackReason.Other",
                                    fallback_reason);
  }
}

}  // namespace blink
