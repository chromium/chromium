// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_RTP_TRANSCEIVER_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_RTP_TRANSCEIVER_PLATFORM_H_

#include <memory>

#include "base/optional.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"

namespace blink {

class RTCRtpReceiverPlatform;
class RTCRtpSenderPlatform;

// In Unified Plan transceivers exist and a full implementation of
// RTCRtpTransceiverPlatform is required.
// In Plan B, the same methods that would be used to surface transceivers only
// surface the sender or receiver component.
// To make the Plan B -> Unified Plan transition easier,
// RTCRtpTransceiverPlatform is used in both cases and ImplementationType
// indicates which methods are applicable for the RTCRtpTransceiverPlatform
// implementation.
enum class RTCRtpTransceiverPlatformImplementationType {
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
class PLATFORM_EXPORT RTCRtpTransceiverPlatform {
 public:
  virtual ~RTCRtpTransceiverPlatform();

  // Which methods (other than ImplementationType()) is guaranteed to be
  // implemented.
  virtual RTCRtpTransceiverPlatformImplementationType ImplementationType()
      const = 0;

  // Identifies the webrtc-layer transceiver. Multiple RTCRtpTransceiverPlatform
  // can exist for the same webrtc-layer transceiver.
  virtual uintptr_t Id() const = 0;
  virtual String Mid() const = 0;
  virtual void SetMid(base::Optional<String>) {}
  virtual std::unique_ptr<RTCRtpSenderPlatform> Sender() const = 0;
  virtual std::unique_ptr<RTCRtpReceiverPlatform> Receiver() const = 0;
  virtual bool Stopped() const = 0;
  virtual webrtc::RtpTransceiverDirection Direction() const = 0;
  virtual webrtc::RTCError SetDirection(webrtc::RtpTransceiverDirection) = 0;
  virtual base::Optional<webrtc::RtpTransceiverDirection> CurrentDirection()
      const = 0;
  virtual base::Optional<webrtc::RtpTransceiverDirection> FiredDirection()
      const = 0;
  virtual webrtc::RTCError Stop() {
    NOTREACHED();
    return webrtc::RTCError::OK();
  }
  virtual webrtc::RTCError SetCodecPreferences(
      Vector<webrtc::RtpCodecCapability>) {
    return {};
  }
  virtual webrtc::RTCError SetOfferedRtpHeaderExtensions(
      Vector<webrtc::RtpHeaderExtensionCapability> header_extensions) = 0;
  virtual Vector<webrtc::RtpHeaderExtensionCapability>
  HeaderExtensionsNegotiated() const = 0;
  virtual Vector<webrtc::RtpHeaderExtensionCapability> HeaderExtensionsToOffer()
      const = 0;
};

// This class contains dummy implementations for functions that are not
// supported in Plan B mode.
class RTCRtpPlanBTransceiverPlatform : public RTCRtpTransceiverPlatform {
 public:
  webrtc::RTCError SetOfferedRtpHeaderExtensions(
      Vector<webrtc::RtpHeaderExtensionCapability> header_extensions) override {
    return webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION);
  }
  Vector<webrtc::RtpHeaderExtensionCapability> HeaderExtensionsNegotiated()
      const override {
    return {};
  }
  Vector<webrtc::RtpHeaderExtensionCapability> HeaderExtensionsToOffer()
      const override {
    return {};
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_RTP_TRANSCEIVER_PLATFORM_H_
