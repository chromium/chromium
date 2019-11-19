// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_RTP_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_RTP_SOURCE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/blink/public/platform/web_rtc_rtp_source.h"
#include "third_party/webrtc/api/rtp_receiver_interface.h"

namespace blink {

class RTCRtpSource : public WebRTCRtpSource {
 public:
  explicit RTCRtpSource(const webrtc::RtpSource& source);
  ~RTCRtpSource() override;

  WebRTCRtpSource::Type SourceType() const override;
  base::TimeTicks Timestamp() const override;
  uint32_t Source() const override;
  base::Optional<double> AudioLevel() const override;
  uint32_t RtpTimestamp() const override;

 private:
  const webrtc::RtpSource source_;

  DISALLOW_COPY_AND_ASSIGN(RTCRtpSource);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_RTP_SOURCE_H_
