// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_RECEIVER_H_

#include "base/optional.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_rtc_rtp_receiver.h"
#include "third_party/blink/public/platform/web_rtc_rtp_source.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_contributing_source.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receive_parameters.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_synchronization_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {
class RTCDtlsTransport;
class RTCPeerConnection;
class RTCRtpCapabilities;
class RTCRtpTransceiver;

// https://w3c.github.io/webrtc-pc/#rtcrtpreceiver-interface
class RTCRtpReceiver final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Takes ownership of the receiver.
  RTCRtpReceiver(RTCPeerConnection*,
                 std::unique_ptr<WebRTCRtpReceiver>,
                 MediaStreamTrack*,
                 MediaStreamVector);

  static RTCRtpCapabilities* getCapabilities(const String& kind);

  MediaStreamTrack* track() const;
  RTCDtlsTransport* transport();
  RTCDtlsTransport* rtcpTransport();
  double playoutDelayHint(bool&, ExceptionState&);
  void setPlayoutDelayHint(double, bool, ExceptionState&);
  RTCRtpReceiveParameters* getParameters();
  HeapVector<Member<RTCRtpSynchronizationSource>> getSynchronizationSources();
  HeapVector<Member<RTCRtpContributingSource>> getContributingSources();
  ScriptPromise getStats(ScriptState*);

  WebRTCRtpReceiver* web_receiver();
  MediaStreamVector streams() const;
  void set_streams(MediaStreamVector streams);
  void set_transceiver(RTCRtpTransceiver*);
  void set_transport(RTCDtlsTransport*);
  void UpdateSourcesIfNeeded();

  void Trace(blink::Visitor*) override;

 private:
  Member<RTCPeerConnection> pc_;
  void SetContributingSourcesNeedsUpdating();

  std::unique_ptr<WebRTCRtpReceiver> receiver_;
  Member<MediaStreamTrack> track_;
  Member<RTCDtlsTransport> transport_;
  MediaStreamVector streams_;

  // The current SSRCs and CSRCs. getSynchronizationSources() returns the SSRCs
  // and getContributingSources() returns the CSRCs.
  WebVector<std::unique_ptr<WebRTCRtpSource>> web_sources_;
  bool web_sources_needs_updating_ = true;
  Member<RTCRtpTransceiver> transceiver_;

  // Hint to the WebRTC Jitter Buffer about desired playout delay. Actual
  // observed delay may differ depending on the congestion control. |nullopt|
  // means default value must be used.
  base::Optional<double> playout_delay_hint_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_RECEIVER_H_
