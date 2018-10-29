// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_MOCK_PEER_CONNECTION_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_MOCK_PEER_CONNECTION_IMPL_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/api/stats/rtcstatsreport.h"

namespace content {

class MockPeerConnectionDependencyFactory;
class MockStreamCollection;

class FakeRtpSender : public webrtc::RtpSenderInterface {
 public:
  FakeRtpSender(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
                std::vector<std::string> stream_ids);
  ~FakeRtpSender() override;

  bool SetTrack(webrtc::MediaStreamTrackInterface* track) override;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track() const override;
  uint32_t ssrc() const override;
  cricket::MediaType media_type() const override;
  std::string id() const override;
  std::vector<std::string> stream_ids() const override;
  std::vector<webrtc::RtpEncodingParameters> init_send_encodings()
      const override;
  webrtc::RtpParameters GetParameters() override;
  webrtc::RTCError SetParameters(
      const webrtc::RtpParameters& parameters) override;
  rtc::scoped_refptr<webrtc::DtmfSenderInterface> GetDtmfSender()
      const override;

 private:
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track_;
  std::vector<std::string> stream_ids_;
};

class FakeRtpReceiver : public webrtc::RtpReceiverInterface {
 public:
  FakeRtpReceiver(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
                  std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
                      streams = {});
  ~FakeRtpReceiver() override;

  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track() const override;
  std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>> streams()
      const override;
  cricket::MediaType media_type() const override;
  std::string id() const override;
  webrtc::RtpParameters GetParameters() const override;
  bool SetParameters(const webrtc::RtpParameters& parameters) override;
  void SetObserver(webrtc::RtpReceiverObserverInterface* observer) override;
  std::vector<webrtc::RtpSource> GetSources() const override;

 private:
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track_;
  std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>> streams_;
};

class FakeRtpTransceiver : public webrtc::RtpTransceiverInterface {
 public:
  FakeRtpTransceiver(
      cricket::MediaType media_type,
      rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      base::Optional<std::string> mid,
      bool stopped,
      webrtc::RtpTransceiverDirection direction,
      base::Optional<webrtc::RtpTransceiverDirection> current_direction);
  ~FakeRtpTransceiver() override;

  FakeRtpTransceiver& operator=(const FakeRtpTransceiver& other) = default;

  cricket::MediaType media_type() const override;
  absl::optional<std::string> mid() const override;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender() const override;
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver() const override;
  bool stopped() const override;
  webrtc::RtpTransceiverDirection direction() const override;
  void SetDirection(webrtc::RtpTransceiverDirection new_direction) override;
  absl::optional<webrtc::RtpTransceiverDirection> current_direction()
      const override;
  void Stop() override;

 private:
  cricket::MediaType media_type_;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender_;
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver_;
  absl::optional<std::string> mid_;
  bool stopped_;
  webrtc::RtpTransceiverDirection direction_;
  absl::optional<webrtc::RtpTransceiverDirection> current_direction_;
};

// TODO(hbos): The use of fakes and mocks is the wrong approach for testing of
// this. It introduces complexity, is error prone (not testing the right thing
// and bugs in the mocks). This class is a maintenance burden and should be
// removed. https://crbug.com/788659
class MockPeerConnectionImpl : public webrtc::PeerConnectionInterface {
 public:
  explicit MockPeerConnectionImpl(MockPeerConnectionDependencyFactory* factory,
                                  webrtc::PeerConnectionObserver* observer);

  // PeerConnectionInterface implementation.
  rtc::scoped_refptr<webrtc::StreamCollectionInterface> local_streams()
      override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  rtc::scoped_refptr<webrtc::StreamCollectionInterface> remote_streams()
      override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  bool AddStream(webrtc::MediaStreamInterface* local_stream) override {
    NOTIMPLEMENTED();
    return false;
  }
  void RemoveStream(webrtc::MediaStreamInterface* local_stream) override {
    NOTIMPLEMENTED();
  }
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids) override;
  bool RemoveTrack(webrtc::RtpSenderInterface* sender) override;
  std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> GetSenders()
      const override;
  std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>> GetReceivers()
      const override;
  rtc::scoped_refptr<webrtc::DataChannelInterface>
      CreateDataChannel(const std::string& label,
                        const webrtc::DataChannelInit* config) override;
  bool GetStats(webrtc::StatsObserver* observer,
                webrtc::MediaStreamTrackInterface* track,
                StatsOutputLevel level) override;
  void GetStats(webrtc::RTCStatsCollectorCallback* callback) override;
  void GetStats(
      rtc::scoped_refptr<webrtc::RtpSenderInterface> selector,
      rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback> callback) override;
  void GetStats(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> selector,
      rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback> callback) override;

  // Call this function to make sure next call to legacy GetStats fail.
  void SetGetStatsResult(bool result) { getstats_result_ = result; }
  // Set the report that |GetStats(RTCStatsCollectorCallback*)| returns.
  void SetGetStatsReport(webrtc::RTCStatsReport* report);

  SignalingState signaling_state() override {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kStable;
  }
  IceConnectionState ice_connection_state() override {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kIceConnectionNew;
  }
  IceGatheringState ice_gathering_state() override {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kIceGatheringNew;
  }

  bool StartRtcEventLog(rtc::PlatformFile file,
                        int64_t max_size_bytes) override {
    NOTIMPLEMENTED();
    return false;
  }
  void StopRtcEventLog() override { NOTIMPLEMENTED(); }

  MOCK_METHOD0(Close, void());

  const webrtc::SessionDescriptionInterface* local_description() const override;
  const webrtc::SessionDescriptionInterface* remote_description()
      const override;

  // JSEP01 APIs
  void CreateOffer(webrtc::CreateSessionDescriptionObserver* observer,
                   const RTCOfferAnswerOptions& options) override;
  void CreateAnswer(webrtc::CreateSessionDescriptionObserver* observer,
                    const RTCOfferAnswerOptions& options) override;
  MOCK_METHOD2(SetLocalDescription,
               void(webrtc::SetSessionDescriptionObserver* observer,
                    webrtc::SessionDescriptionInterface* desc));
  void SetLocalDescriptionWorker(
      webrtc::SetSessionDescriptionObserver* observer,
      webrtc::SessionDescriptionInterface* desc);
  // TODO(hbos): Remove once no longer mandatory to implement.
  MOCK_METHOD2(SetRemoteDescription,
               void(webrtc::SetSessionDescriptionObserver* observer,
                    webrtc::SessionDescriptionInterface* desc));
  void SetRemoteDescription(
      std::unique_ptr<webrtc::SessionDescriptionInterface> desc,
      rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>
          observer) override {
    SetRemoteDescriptionForMock(&desc, &observer);
  }
  // Work-around due to MOCK_METHOD being unable to handle move-only arguments.
  MOCK_METHOD2(
      SetRemoteDescriptionForMock,
      void(std::unique_ptr<webrtc::SessionDescriptionInterface>* desc,
           rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>*
               observer));
  void SetRemoteDescriptionWorker(
      webrtc::SetSessionDescriptionObserver* observer,
      webrtc::SessionDescriptionInterface* desc);
  bool SetConfiguration(const RTCConfiguration& configuration,
                        webrtc::RTCError* error) override;
  bool AddIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

  webrtc::RTCError SetBitrate(const webrtc::BitrateSettings& bitrate) override;

  void AddRemoteStream(webrtc::MediaStreamInterface* stream);

  const std::string& stream_label() const { return stream_label_; }
  bool hint_audio() const { return hint_audio_; }
  bool hint_video() const { return hint_video_; }
  const std::string& description_sdp() const { return description_sdp_; }
  const std::string& sdp_mid() const { return sdp_mid_; }
  int sdp_mline_index() const { return sdp_mline_index_; }
  const std::string& ice_sdp() const { return ice_sdp_; }
  webrtc::SessionDescriptionInterface* created_session_description() const {
    return created_sessiondescription_.get();
  }
  webrtc::PeerConnectionObserver* observer() {
    return observer_;
  }
  void set_setconfiguration_error_type(webrtc::RTCErrorType error_type) {
    setconfiguration_error_type_ = error_type;
  }
  static const char kDummyOffer[];
  static const char kDummyAnswer[];

 protected:
  ~MockPeerConnectionImpl() override;

 private:
  // Used for creating MockSessionDescription.
  MockPeerConnectionDependencyFactory* dependency_factory_;

  std::string stream_label_;
  std::vector<std::string> local_stream_ids_;
  rtc::scoped_refptr<MockStreamCollection> remote_streams_;
  std::vector<rtc::scoped_refptr<FakeRtpSender>> senders_;
  std::unique_ptr<webrtc::SessionDescriptionInterface> local_desc_;
  std::unique_ptr<webrtc::SessionDescriptionInterface> remote_desc_;
  std::unique_ptr<webrtc::SessionDescriptionInterface>
      created_sessiondescription_;
  bool hint_audio_;
  bool hint_video_;
  bool getstats_result_;
  std::string description_sdp_;
  std::string sdp_mid_;
  int sdp_mline_index_;
  std::string ice_sdp_;
  webrtc::PeerConnectionObserver* observer_;
  webrtc::RTCErrorType setconfiguration_error_type_ =
      webrtc::RTCErrorType::NONE;
  rtc::scoped_refptr<webrtc::RTCStatsReport> stats_report_;

  DISALLOW_COPY_AND_ASSIGN(MockPeerConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_MOCK_PEER_CONNECTION_IMPL_H_
