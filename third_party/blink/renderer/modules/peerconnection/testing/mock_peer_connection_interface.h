// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_PEER_CONNECTION_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_PEER_CONNECTION_INTERFACE_H_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/rtc_base/ref_count.h"

namespace blink {

class MockPeerConnectionInterface
    : public rtc::RefCountedObject<webrtc::PeerConnectionInterface> {
 public:
  // PeerConnectionInterface
  MOCK_METHOD(rtc::scoped_refptr<webrtc::StreamCollectionInterface>,
              local_streams,
              (),
              (override));
  MOCK_METHOD(rtc::scoped_refptr<webrtc::StreamCollectionInterface>,
              remote_streams,
              (),
              (override));
  MOCK_METHOD(bool, AddStream, (webrtc::MediaStreamInterface*), (override));
  MOCK_METHOD(void, RemoveStream, (webrtc::MediaStreamInterface*), (override));
  MOCK_METHOD(
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>,
      AddTrack,
      (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>,
       const std::vector<std::string>&),
      (override));
  MOCK_METHOD(bool, RemoveTrack, (webrtc::RtpSenderInterface*), (override));
  MOCK_METHOD(webrtc::RTCError,
              RemoveTrackNew,
              (rtc::scoped_refptr<webrtc::RtpSenderInterface>),
              (override));
  MOCK_METHOD(
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>,
      AddTransceiver,
      (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>),
      (override));
  MOCK_METHOD(
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>,
      AddTransceiver,
      (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>,
       const webrtc::RtpTransceiverInit&),
      (override));
  MOCK_METHOD(
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>,
      AddTransceiver,
      (cricket::MediaType),
      (override));
  MOCK_METHOD(
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>,
      AddTransceiver,
      (cricket::MediaType, const webrtc::RtpTransceiverInit&),
      (override));
  MOCK_METHOD(rtc::scoped_refptr<webrtc::RtpSenderInterface>,
              CreateSender,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>>,
              GetSenders,
              (),
              (const override));
  MOCK_METHOD(std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>>,
              GetReceivers,
              (),
              (const override));
  MOCK_METHOD(std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>,
              GetTransceivers,
              (),
              (const override));
  MOCK_METHOD(bool,
              GetStats,
              (webrtc::StatsObserver*,
               webrtc::MediaStreamTrackInterface*,
               StatsOutputLevel),
              (override));
  MOCK_METHOD(void, GetStats, (webrtc::RTCStatsCollectorCallback*), (override));
  MOCK_METHOD(void,
              GetStats,
              (rtc::scoped_refptr<webrtc::RtpSenderInterface>,
               rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>),
              (override));
  MOCK_METHOD(void,
              GetStats,
              (rtc::scoped_refptr<webrtc::RtpReceiverInterface>,
               rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>),
              (override));
  MOCK_METHOD(void, ClearStatsCache, (), (override));
  MOCK_METHOD(rtc::scoped_refptr<webrtc::SctpTransportInterface>,
              GetSctpTransport,
              (),
              (const override));
  MOCK_METHOD(rtc::scoped_refptr<webrtc::DataChannelInterface>,
              CreateDataChannel,
              (const std::string&, const webrtc::DataChannelInit*),
              (override));
  MOCK_METHOD(const webrtc::SessionDescriptionInterface*,
              local_description,
              (),
              (const override));
  MOCK_METHOD(const webrtc::SessionDescriptionInterface*,
              remote_description,
              (),
              (const override));
  MOCK_METHOD(const webrtc::SessionDescriptionInterface*,
              current_local_description,
              (),
              (const override));
  MOCK_METHOD(const webrtc::SessionDescriptionInterface*,
              current_remote_description,
              (),
              (const override));
  MOCK_METHOD(const webrtc::SessionDescriptionInterface*,
              pending_local_description,
              (),
              (const override));
  MOCK_METHOD(const webrtc::SessionDescriptionInterface*,
              pending_remote_description,
              (),
              (const override));
  MOCK_METHOD(void, RestartIce, (), (override));
  MOCK_METHOD(void,
              CreateOffer,
              (webrtc::CreateSessionDescriptionObserver*,
               const RTCOfferAnswerOptions&),
              (override));
  MOCK_METHOD(void,
              CreateAnswer,
              (webrtc::CreateSessionDescriptionObserver*,
               const RTCOfferAnswerOptions&),
              (override));
  MOCK_METHOD(void,
              SetLocalDescription,
              (webrtc::SetSessionDescriptionObserver*,
               webrtc::SessionDescriptionInterface*),
              (override));
  MOCK_METHOD(void,
              SetRemoteDescription,
              (webrtc::SetSessionDescriptionObserver*,
               webrtc::SessionDescriptionInterface*),
              (override));
  MOCK_METHOD(
      void,
      SetRemoteDescription,
      (std::unique_ptr<webrtc::SessionDescriptionInterface>,
       rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>),
      (override));
  MOCK_METHOD(PeerConnectionInterface::RTCConfiguration,
              GetConfiguration,
              (),
              (override));
  MOCK_METHOD(webrtc::RTCError,
              SetConfiguration,
              (const PeerConnectionInterface::RTCConfiguration&),
              (override));
  MOCK_METHOD(bool,
              AddIceCandidate,
              (const webrtc::IceCandidateInterface*),
              (override));
  MOCK_METHOD(bool,
              RemoveIceCandidates,
              (const std::vector<cricket::Candidate>&),
              (override));
  MOCK_METHOD(webrtc::RTCError,
              SetBitrate,
              (const webrtc::BitrateSettings&),
              (override));
  MOCK_METHOD(void, SetAudioPlayout, (bool), (override));
  MOCK_METHOD(void, SetAudioRecording, (bool), (override));
  MOCK_METHOD(rtc::scoped_refptr<webrtc::DtlsTransportInterface>,
              LookupDtlsTransportByMid,
              (const std::string&),
              (override));
  MOCK_METHOD(SignalingState, signaling_state, (), (override));
  MOCK_METHOD(IceConnectionState, ice_connection_state, (), (override));
  MOCK_METHOD(IceConnectionState,
              standardized_ice_connection_state,
              (),
              (override));
  MOCK_METHOD(PeerConnectionState, peer_connection_state, (), (override));
  MOCK_METHOD(IceGatheringState, ice_gathering_state, (), (override));
  MOCK_METHOD(absl::optional<bool>, can_trickle_ice_candidates, (), (override));
  MOCK_METHOD(bool,
              StartRtcEventLog,
              (std::unique_ptr<webrtc::RtcEventLogOutput>, int64_t),
              (override));
  MOCK_METHOD(bool,
              StartRtcEventLog,
              (std::unique_ptr<webrtc::RtcEventLogOutput>),
              (override));
  MOCK_METHOD(void, StopRtcEventLog, (), (override));
  MOCK_METHOD(void, Close, (), (override));
};

static_assert(!std::is_abstract<MockPeerConnectionInterface>::value, "");

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_PEER_CONNECTION_INTERFACE_H_
