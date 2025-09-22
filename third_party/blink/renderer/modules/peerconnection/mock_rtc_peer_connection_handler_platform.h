// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_RTC_PEER_CONNECTION_HANDLER_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_RTC_PEER_CONNECTION_HANDLER_PLATFORM_H_

#include <memory>
#include <string>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/test/mock_peerconnectioninterface.h"

namespace blink {

// Class for creating a ParsedSessionDescription without running the parser.
// It returns an empty (but non-null) description object.
class MockParsedSessionDescription : public ParsedSessionDescription {
 public:
  MockParsedSessionDescription(const String& type, const String& sdp)
      : ParsedSessionDescription(type, sdp) {
    std::optional<webrtc::SdpType> sdp_type =
        webrtc::SdpTypeFromString(type.Utf8());
    if (!sdp_type) {
      sdp_type = webrtc::SdpType::kOffer;
    }
    description_ = webrtc::CreateSessionDescription(*sdp_type, sdp.Utf8());
  }
  // Constructor for creating an error-returning session description.
  MockParsedSessionDescription() : ParsedSessionDescription("error", "error") {}
};

// TODO(https://crbug.com/908461): This is currently implemented as NO-OPs or to
// create dummy objects whose methods return default values. Consider renaming
// the class, changing it to be GMOCK friendly or deleting it.
// TODO(https://crbug.com/787254): Remove "Platform" from the name of this
// class.
class MockRTCPeerConnectionHandlerPlatform : public RTCPeerConnectionHandler {
 public:
  MockRTCPeerConnectionHandlerPlatform();
  ~MockRTCPeerConnectionHandlerPlatform() override;

  bool Initialize(ExecutionContext* context,
                  const webrtc::PeerConnectionInterface::RTCConfiguration&,
                  WebLocalFrame*,
                  ExceptionState&) override;
  void Close() override;
  void CloseAndUnregister() override;

  Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> CreateOffer(
      RTCSessionDescriptionRequest*,
      RTCOfferOptionsPlatform*) override;
  void CreateAnswer(RTCSessionDescriptionRequest*,
                    RTCAnswerOptionsPlatform*) override;
  void SetLocalDescription(RTCVoidRequest*) override;
  void SetLocalDescription(RTCVoidRequest*, ParsedSessionDescription) override;
  void SetRemoteDescription(RTCVoidRequest*, ParsedSessionDescription) override;
  const webrtc::PeerConnectionInterface::RTCConfiguration& GetConfiguration()
      const override;
  webrtc::RTCErrorType SetConfiguration(
      const webrtc::PeerConnectionInterface::RTCConfiguration&) override;
  void AddIceCandidate(RTCVoidRequest*, RTCIceCandidatePlatform*) override;
  void RestartIce() override;
  void GetStats(RTCStatsReportCallback) override;
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
  AddTransceiverWithTrack(MediaStreamComponent*,
                          const webrtc::RtpTransceiverInit&) override;
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
  AddTransceiverWithKind(const String& kind,
                         const webrtc::RtpTransceiverInit&) override;
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>> AddTrack(
      MediaStreamComponent*,
      const MediaStreamDescriptorVector&) override;
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>> RemoveTrack(
      RTCRtpSenderPlatform*) override;
  webrtc::scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
      const String& label,
      const webrtc::DataChannelInit&) override;
  webrtc::PeerConnectionInterface* NativePeerConnection() override;
  void RunSynchronousOnceClosureOnSignalingThread(
      base::OnceClosure closure,
      const char* trace_event_name) override;
  void TrackIceConnectionStateChange(
      webrtc::PeerConnectionInterface::IceConnectionState state) override;

 private:
  class DummyRTCRtpTransceiverPlatform;

  Vector<std::unique_ptr<DummyRTCRtpTransceiverPlatform>> transceivers_;
  webrtc::scoped_refptr<webrtc::MockPeerConnectionInterface>
      native_peer_connection_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_RTC_PEER_CONNECTION_HANDLER_PLATFORM_H_
