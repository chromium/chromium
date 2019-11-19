// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_source.h"

#include <cmath>

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/webrtc/api/scoped_refptr.h"

namespace blink {

std::unique_ptr<WebRTCRtpSource> CreateRTCRtpSource(
    const webrtc::RtpSource& source) {
  return std::make_unique<RTCRtpSource>(source);
}

RTCRtpSource::RTCRtpSource(const webrtc::RtpSource& source) : source_(source) {}

RTCRtpSource::~RTCRtpSource() {}

WebRTCRtpSource::Type RTCRtpSource::SourceType() const {
  switch (source_.source_type()) {
    case webrtc::RtpSourceType::SSRC:
      return WebRTCRtpSource::Type::kSSRC;
    case webrtc::RtpSourceType::CSRC:
      return WebRTCRtpSource::Type::kCSRC;
    default:
      NOTREACHED();
      return WebRTCRtpSource::Type::kSSRC;
  }
}

base::TimeTicks RTCRtpSource::Timestamp() const {
  return base::TimeTicks() +
         base::TimeDelta::FromMilliseconds(source_.timestamp_ms());
}

uint32_t RTCRtpSource::Source() const {
  return source_.source_id();
}

base::Optional<double> RTCRtpSource::AudioLevel() const {
  if (!source_.audio_level())
    return base::nullopt;
  // Converted according to equation defined here:
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtpcontributingsource-audiolevel
  uint8_t rfc_level = *source_.audio_level();
  if (rfc_level > 127u)
    rfc_level = 127u;
  if (rfc_level == 127u)
    return 0.0;
  return std::pow(10.0, -(double)rfc_level / 20.0);
}

uint32_t RTCRtpSource::RtpTimestamp() const {
  return source_.rtp_timestamp();
}

}  // namespace blink
