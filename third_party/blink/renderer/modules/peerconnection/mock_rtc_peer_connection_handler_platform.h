// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_RTC_PEER_CONNECTION_HANDLER_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_RTC_PEER_CONNECTION_HANDLER_PLATFORM_H_

#include <memory>
#include <string>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"

namespace blink {

class MockSessionDescription : public webrtc::SessionDescriptionInterface {
 public:
  MockSessionDescription(const std::string& type, const std::string& sdp)
      : type_(type), sdp_(sdp) {}
  ~MockSessionDescription() override = default;
  cricket::SessionDescription* description() override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  const cricket::SessionDescription* description() const override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  std::string session_id() const override {
    NOTIMPLEMENTED();
    return std::string();
  }
  std::string session_version() const override {
    NOTIMPLEMENTED();
    return std::string();
  }
  std::string type() const override { return type_; }
  bool AddCandidate(const webrtc::IceCandidateInterface* candidate) override {
    NOTIMPLEMENTED();
    return false;
  }
  size_t number_of_mediasections() const override {
    NOTIMPLEMENTED();
    return 0;
  }
  const webrtc::IceCandidateCollection* candidates(
      size_t mediasection_index) const override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  bool ToString(std::string* out) const override {
    *out = sdp_;
    return true;
  }

 private:
  std::string type_;
  std::string sdp_;
};

// Class for creating a ParsedSessionDescription without running the parser.
// It returns an empty (but non-null) description object.
class MockParsedSessionDescription : public ParsedSessionDescription {
 public:
  MockParsedSessionDescription(const String& type, const String& sdp)
      : ParsedSessionDescription(type, sdp) {
    description_ =
        std::make_unique<MockSessionDescription>(type.Utf8(), sdp.Utf8());
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

  bool Initialize(const webrtc::PeerConnectionInterface::RTCConfiguration&,
                  const MediaConstraints&,
                  WebLocalFrame*,
                  ExceptionState&) override;
  void Stop() override;
  void StopAndUnregister() override;

  Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> CreateOffer(
      RTCSessionDescriptionRequest*,
      const MediaConstraints&) override;
  Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> CreateOffer(
      RTCSessionDescriptionRequest*,
      RTCOfferOptionsPlatform*) override;
  void CreateAnswer(RTCSessionDescriptionRequest*,
                    const MediaConstraints&) override;
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
  void GetStats(RTCStatsRequest*) override;
  void GetStats(RTCStatsReportCallback,
                const Vector<webrtc::NonStandardGroupId>&) override;
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
  scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
      const String& label,
      const webrtc::DataChannelInit&) override;
  webrtc::PeerConnectionInterface* NativePeerConnection() override;
  void RunSynchronousOnceClosureOnSignalingThread(
      CrossThreadOnceClosure closure,
      const char* trace_event_name) override;
  void RunSynchronousRepeatingClosureOnSignalingThread(
      const base::RepeatingClosure& closure,
      const char* trace_event_name) override;
  void TrackIceConnectionStateChange(
      RTCPeerConnectionHandler::IceConnectionStateVersion version,
      webrtc::PeerConnectionInterface::IceConnectionState state) override;
 private:
  class DummyRTCRtpTransceiverPlatform;

  Vector<std::unique_ptr<DummyRTCRtpTransceiverPlatform>> transceivers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_RTC_PEER_CONNECTION_HANDLER_PLATFORM_H_
