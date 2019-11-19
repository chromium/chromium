// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_RECEIVER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_RECEIVER_H_

#include <memory>

#include "base/optional.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_rtc_stats.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/webrtc/api/dtls_transport_interface.h"
#include "third_party/webrtc/api/rtp_parameters.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"

namespace blink {

class WebMediaStreamTrack;
class WebRTCRtpSource;

// Implementations of this interface keep the corresponding WebRTC-layer
// receiver alive through reference counting. Multiple |WebRTCRtpReceiver|s
// could reference the same receiver, see |id|.
// https://w3c.github.io/webrtc-pc/#rtcrtpreceiver-interface
class BLINK_PLATFORM_EXPORT WebRTCRtpReceiver {
 public:
  virtual ~WebRTCRtpReceiver();

  virtual std::unique_ptr<WebRTCRtpReceiver> ShallowCopy() const = 0;
  // Two |WebRTCRtpReceiver|s referencing the same WebRTC-layer receiver have
  // the same |id|.
  virtual uintptr_t Id() const = 0;
  virtual rtc::scoped_refptr<webrtc::DtlsTransportInterface>
  DtlsTransport() = 0;
  // Note: For convenience, DtlsTransportInformation always returns a value.
  // The information is only interesting if DtlsTransport() is non-null.
  virtual webrtc::DtlsTransportInformation DtlsTransportInformation() = 0;
  virtual const WebMediaStreamTrack& Track() const = 0;
  virtual WebVector<WebString> StreamIds() const = 0;
  virtual WebVector<std::unique_ptr<WebRTCRtpSource>> GetSources() = 0;
  virtual void GetStats(blink::WebRTCStatsReportCallback,
                        const WebVector<webrtc::NonStandardGroupId>&) = 0;
  virtual std::unique_ptr<webrtc::RtpParameters> GetParameters() const = 0;
  virtual void SetJitterBufferMinimumDelay(
      base::Optional<double> delay_seconds) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_RECEIVER_H_
