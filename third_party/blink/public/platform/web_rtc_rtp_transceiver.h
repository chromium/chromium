// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_TRANSCEIVER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_TRANSCEIVER_H_

#include <memory>

#include "base/optional.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_rtc_rtp_receiver.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"

namespace blink {

class RTCRtpSenderPlatform;

// In Unified Plan transceivers exist and a full implementation of
// WebRTCRtpTransceiver is required.
// In Plan B, the same methods that would be used to surface transceivers only
// surface the sender or receiver component.
// To make the Plan B -> Unified Plan transition easier, WebRTCRtpTransceiver
// is used in both cases and ImplementationType indicates which methods are
// applicable for the WebRTCRtpTransceiver implementation.
enum class WebRTCRtpTransceiverImplementationType {
  // Unified Plan: All methods implemented.
  kFullTransceiver,
  // Plan B: Only Sender() is implemented.
  kPlanBSenderOnly,
  // Plan B: Only Receiver() is implemented.
  kPlanBReceiverOnly,
};

// Interface for content to implement as to allow the surfacing of transceivers.
// TODO(hbos): [Onion Soup] Remove the content layer versions of this class and
// rely on webrtc directly from blink. Requires coordination with senders and
// receivers. https://crbug.com/787254
class BLINK_PLATFORM_EXPORT WebRTCRtpTransceiver {
 public:
  virtual ~WebRTCRtpTransceiver();

  // Which methods (other than ImplementationType()) is guaranteed to be
  // implemented.
  virtual WebRTCRtpTransceiverImplementationType ImplementationType() const = 0;

  // Identifies the webrtc-layer transceiver. Multiple WebRTCRtpTransceiver can
  // exist for the same webrtc-layer transceiver.
  virtual uintptr_t Id() const = 0;
  virtual WebString Mid() const = 0;
  virtual void SetMid(base::Optional<WebString>) {}
  virtual std::unique_ptr<RTCRtpSenderPlatform> Sender() const = 0;
  virtual std::unique_ptr<WebRTCRtpReceiver> Receiver() const = 0;
  virtual bool Stopped() const = 0;
  virtual webrtc::RtpTransceiverDirection Direction() const = 0;
  virtual void SetDirection(webrtc::RtpTransceiverDirection) = 0;
  virtual base::Optional<webrtc::RtpTransceiverDirection> CurrentDirection()
      const = 0;
  virtual base::Optional<webrtc::RtpTransceiverDirection> FiredDirection()
      const = 0;
  virtual webrtc::RTCError SetCodecPreferences(
      WebVector<webrtc::RtpCodecCapability>) {
    return {};
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_TRANSCEIVER_H_
