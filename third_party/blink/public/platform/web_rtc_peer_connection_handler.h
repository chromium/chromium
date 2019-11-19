/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_PEER_CONNECTION_HANDLER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_PEER_CONNECTION_HANDLER_H_

#include <memory>
#include <string>

#include "third_party/blink/public/platform/web_rtc_ice_candidate.h"
#include "third_party/blink/public/platform/web_rtc_rtp_transceiver.h"
#include "third_party/blink/public/platform/web_rtc_stats.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/rtc_error.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"

namespace webrtc {
enum class RTCErrorType;
}

namespace blink {

class RTCAnswerOptionsPlatform;
class RTCOfferOptionsPlatform;
class RTCSessionDescriptionRequest;
class RTCVoidRequest;
class WebLocalFrame;
class WebMediaConstraints;
class WebMediaStream;
class WebMediaStreamTrack;
class RTCRtpSenderPlatform;
class WebRTCSessionDescription;
class WebRTCStatsRequest;
class WebString;
struct WebRTCDataChannelInit;

class WebRTCPeerConnectionHandler {
 public:
  enum class IceConnectionStateVersion {
    // Only applicable in Unified Plan when the JavaScript-exposed
    // iceConnectionState is calculated in blink. In this case, kLegacy is used
    // to report the webrtc::PeerConnectionInterface implementation which is not
    // visible in JavaScript, but still useful to track for debugging purposes.
    kLegacy,
    // The JavaScript-visible iceConnectionState. In Plan B, this is the same as
    // the webrtc::PeerConnectionInterface implementation.
    kDefault,
  };

  virtual ~WebRTCPeerConnectionHandler() = default;

  virtual bool Initialize(
      const webrtc::PeerConnectionInterface::RTCConfiguration&,
      const WebMediaConstraints&) = 0;
  virtual void AssociateWithFrame(WebLocalFrame*) {}

  // Unified Plan: The list of transceivers after the createOffer() call.
  // Because of offerToReceive[Audio/Video] it is possible for createOffer() to
  // create new transceivers or update the direction of existing transceivers.
  // https://w3c.github.io/webrtc-pc/#legacy-configuration-extensions
  // Plan B: Returns an empty list.
  virtual WebVector<std::unique_ptr<WebRTCRtpTransceiver>> CreateOffer(
      RTCSessionDescriptionRequest*,
      const WebMediaConstraints&) = 0;
  virtual WebVector<std::unique_ptr<WebRTCRtpTransceiver>> CreateOffer(
      RTCSessionDescriptionRequest*,
      RTCOfferOptionsPlatform*) = 0;
  virtual void CreateAnswer(RTCSessionDescriptionRequest*,
                            const WebMediaConstraints&) = 0;
  virtual void CreateAnswer(RTCSessionDescriptionRequest*,
                            RTCAnswerOptionsPlatform*) = 0;
  virtual void SetLocalDescription(RTCVoidRequest*) = 0;
  virtual void SetLocalDescription(RTCVoidRequest*,
                                   const WebRTCSessionDescription&) = 0;
  virtual void SetRemoteDescription(RTCVoidRequest*,
                                    const WebRTCSessionDescription&) = 0;
  virtual WebRTCSessionDescription LocalDescription() = 0;
  virtual WebRTCSessionDescription RemoteDescription() = 0;
  virtual WebRTCSessionDescription CurrentLocalDescription() = 0;
  virtual WebRTCSessionDescription CurrentRemoteDescription() = 0;
  virtual WebRTCSessionDescription PendingLocalDescription() = 0;
  virtual WebRTCSessionDescription PendingRemoteDescription() = 0;
  virtual const webrtc::PeerConnectionInterface::RTCConfiguration&
  GetConfiguration() const = 0;
  virtual webrtc::RTCErrorType SetConfiguration(
      const webrtc::PeerConnectionInterface::RTCConfiguration&) = 0;

  virtual void AddICECandidate(RTCVoidRequest*,
                               scoped_refptr<WebRTCICECandidate>) = 0;
  virtual void RestartIce() = 0;
  virtual void GetStats(const WebRTCStatsRequest&) = 0;
  // Gets stats using the new stats collection API, see
  // third_party/webrtc/api/stats/.  These will replace the old stats collection
  // API when the new API has matured enough.
  virtual void GetStats(WebRTCStatsReportCallback,
                        const WebVector<webrtc::NonStandardGroupId>&) = 0;
  virtual scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
      const WebString& label,
      const WebRTCDataChannelInit&) = 0;
  virtual webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>>
  AddTransceiverWithTrack(const WebMediaStreamTrack&,
                          const webrtc::RtpTransceiverInit&) = 0;
  virtual webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>>
  AddTransceiverWithKind(
      // webrtc::MediaStreamTrackInterface::kAudioKind or kVideoKind
      std::string kind,
      const webrtc::RtpTransceiverInit&) = 0;
  // Adds the track to the peer connection, returning the resulting transceiver
  // or error.
  virtual webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>> AddTrack(
      const WebMediaStreamTrack&,
      const WebVector<WebMediaStream>&) = 0;
  // Removes the sender.
  // In Plan B: Returns OK() with value nullptr on success. The sender's track
  // must be nulled by the caller.
  // In Unified Plan: Returns OK() with the updated transceiver state.
  virtual webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>> RemoveTrack(
      RTCRtpSenderPlatform*) = 0;
  virtual void Stop() = 0;

  // Returns a pointer to the underlying native PeerConnection object.
  virtual webrtc::PeerConnectionInterface* NativePeerConnection() = 0;

  virtual void RunSynchronousOnceClosureOnSignalingThread(
      base::OnceClosure closure,
      const char* trace_event_name) = 0;
  virtual void RunSynchronousRepeatingClosureOnSignalingThread(
      const base::RepeatingClosure& closure,
      const char* trace_event_name) = 0;

  // Inform chrome://webrtc-internals/ that the iceConnectionState has changed.
  virtual void TrackIceConnectionStateChange(
      IceConnectionStateVersion version,
      webrtc::PeerConnectionInterface::IceConnectionState state) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_PEER_CONNECTION_HANDLER_H_
