// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_PEER_CONNECTION_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_PEER_CONNECTION_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/dtls_transport_interface.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/sctp_transport_interface.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"
#include "third_party/webrtc/api/test/mock_peerconnectioninterface.h"

namespace blink {

class MockPeerConnectionDependencyFactory;
class MockStreamCollection;

class FakeRtpSender : public webrtc::RtpSenderInterface {
 public:
  FakeRtpSender(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
                std::vector<std::string> stream_ids);
  ~FakeRtpSender() override;

  bool SetTrack(webrtc::MediaStreamTrackInterface* track) override;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track() const override;
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> dtls_transport()
      const override;
  uint32_t ssrc() const override;
  cricket::MediaType media_type() const override;
  std::string id() const override;
  std::vector<std::string> stream_ids() const override;
  void SetStreams(const std::vector<std::string>& stream_ids) override;
  std::vector<webrtc::RtpEncodingParameters> init_send_encodings()
      const override;
  webrtc::RtpParameters GetParameters() const override;
  webrtc::RTCError SetParameters(
      const webrtc::RtpParameters& parameters) override;
  rtc::scoped_refptr<webrtc::DtmfSenderInterface> GetDtmfSender()
      const override;
  void SetTransport(
      rtc::scoped_refptr<webrtc::DtlsTransportInterface> transport) {
    transport_ = transport;
  }

  void SetFrameEncryptor(rtc::scoped_refptr<webrtc::FrameEncryptorInterface>
                             frame_encryptor) override {}
  rtc::scoped_refptr<webrtc::FrameEncryptorInterface> GetFrameEncryptor()
      const override {
    return nullptr;
  }

  void SetEncoderToPacketizerFrameTransformer(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override {}
  void SetEncoderSelector(
      std::unique_ptr<webrtc::VideoEncoderFactory::EncoderSelectorInterface>
          encoder_selector) override {}

 private:
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track_;
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> transport_;
  std::vector<std::string> stream_ids_ ALLOW_DISCOURAGED_TYPE(
      "Avoids conversion when implementing webrtc::RtpSenderInterface");
};

class FakeRtpReceiver : public webrtc::RtpReceiverInterface {
 public:
  FakeRtpReceiver(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
                  std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
                      streams = {});
  ~FakeRtpReceiver() override;

  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track() const override;
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> dtls_transport()
      const override;
  std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>> streams()
      const override;
  std::vector<std::string> stream_ids() const override;
  cricket::MediaType media_type() const override;
  std::string id() const override;
  webrtc::RtpParameters GetParameters() const override;
  bool SetParameters(const webrtc::RtpParameters& parameters) override;
  void SetObserver(webrtc::RtpReceiverObserverInterface* observer) override;
  void SetJitterBufferMinimumDelay(
      std::optional<double> delay_seconds) override;
  std::vector<webrtc::RtpSource> GetSources() const override;
  void SetTransport(
      rtc::scoped_refptr<webrtc::DtlsTransportInterface> transport) {
    transport_ = transport;
  }

 private:
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track_;
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> transport_;
  std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>> streams_
      ALLOW_DISCOURAGED_TYPE(
          "Avoids conversion when implementing webrtc::RcpReceiverInterface");
};

class FakeRtpTransceiver : public webrtc::RtpTransceiverInterface {
 public:
  FakeRtpTransceiver(
      cricket::MediaType media_type,
      rtc::scoped_refptr<FakeRtpSender> sender,
      rtc::scoped_refptr<FakeRtpReceiver> receiver,
      std::optional<std::string> mid,
      bool stopped,
      webrtc::RtpTransceiverDirection direction,
      std::optional<webrtc::RtpTransceiverDirection> current_direction);
  ~FakeRtpTransceiver() override;

  void ReplaceWith(const FakeRtpTransceiver& other);

  cricket::MediaType media_type() const override;
  std::optional<std::string> mid() const override;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender() const override;
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver() const override;
  bool stopped() const override;
  bool stopping() const override;
  webrtc::RtpTransceiverDirection direction() const override;
  std::optional<webrtc::RtpTransceiverDirection> current_direction()
      const override;
  void SetTransport(
      rtc::scoped_refptr<webrtc::DtlsTransportInterface> transport);
  std::vector<webrtc::RtpCodecCapability> codec_preferences() const override {
    return {};
  }
  webrtc::RTCError SetCodecPreferences(
      rtc::ArrayView<webrtc::RtpCodecCapability>) override {
    RTC_DCHECK_NOTREACHED() << "Not implemented";
    return {};
  }
  std::vector<webrtc::RtpHeaderExtensionCapability>
  GetHeaderExtensionsToNegotiate() const override {
    return {};
  }
  webrtc::RTCError SetHeaderExtensionsToNegotiate(
      rtc::ArrayView<const webrtc::RtpHeaderExtensionCapability>
          header_extensions) override {
    return webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION);
  }

  std::vector<webrtc::RtpHeaderExtensionCapability>
  GetNegotiatedHeaderExtensions() const override {
    return {};
  }

 private:
  cricket::MediaType media_type_;
  rtc::scoped_refptr<FakeRtpSender> sender_;
  rtc::scoped_refptr<FakeRtpReceiver> receiver_;
  std::optional<std::string> mid_;
  bool stopped_;
  webrtc::RtpTransceiverDirection direction_;
  std::optional<webrtc::RtpTransceiverDirection> current_direction_;
};

class FakeDtlsTransport : public webrtc::DtlsTransportInterface {
 public:
  FakeDtlsTransport();
  rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport() override;
  webrtc::DtlsTransportInformation Information() override;
  void RegisterObserver(
      webrtc::DtlsTransportObserverInterface* observer) override {}
  void UnregisterObserver() override {}
};

// TODO(hbos): The use of fakes and mocks is the wrong approach for testing of
// this. It introduces complexity, is error prone (not testing the right thing
// and bugs in the mocks). This class is a maintenance burden and should be
// removed. https://crbug.com/788659
class MockPeerConnectionImpl : public webrtc::MockPeerConnectionInterface {
 public:
  explicit MockPeerConnectionImpl(MockPeerConnectionDependencyFactory* factory,
                                  webrtc::PeerConnectionObserver* observer);

  MockPeerConnectionImpl(const MockPeerConnectionImpl&) = delete;
  MockPeerConnectionImpl& operator=(const MockPeerConnectionImpl&) = delete;

  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids) override;
  webrtc::RTCError RemoveTrackOrError(
      rtc::scoped_refptr<webrtc::RtpSenderInterface> sender) override;
  std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> GetSenders()
      const override;
  std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>> GetReceivers()
      const override;
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
  GetTransceivers() const override;
  MOCK_CONST_METHOD0(GetSctpTransport,
                     rtc::scoped_refptr<webrtc::SctpTransportInterface>());
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::DataChannelInterface>>
  CreateDataChannelOrError(const std::string& label,
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
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> LookupDtlsTransportByMid(
      const std::string& mid) override {
    return nullptr;
  }

  MOCK_METHOD0(Close, void());

  const webrtc::SessionDescriptionInterface* local_description() const override;
  const webrtc::SessionDescriptionInterface* remote_description()
      const override;
  const webrtc::SessionDescriptionInterface* current_local_description()
      const override {
    return nullptr;
  }
  const webrtc::SessionDescriptionInterface* current_remote_description()
      const override {
    return nullptr;
  }
  const webrtc::SessionDescriptionInterface* pending_local_description()
      const override {
    return nullptr;
  }
  const webrtc::SessionDescriptionInterface* pending_remote_description()
      const override {
    return nullptr;
  }

  // JSEP01 APIs
  void CreateOffer(webrtc::CreateSessionDescriptionObserver* observer,
                   const RTCOfferAnswerOptions& options) override;
  void CreateAnswer(webrtc::CreateSessionDescriptionObserver* observer,
                    const RTCOfferAnswerOptions& options) override;
  // TODO(hbos): Remove once no longer mandatory to implement.
  MOCK_METHOD2(SetLocalDescription,
               void(webrtc::SetSessionDescriptionObserver* observer,
                    webrtc::SessionDescriptionInterface* desc));
  void SetLocalDescription(
      std::unique_ptr<webrtc::SessionDescriptionInterface> desc,
      rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface> observer)
      override {
    SetLocalDescriptionForMock(&desc, &observer);
  }
  // Work-around due to MOCK_METHOD being unable to handle move-only arguments.
  MOCK_METHOD2(
      SetLocalDescriptionForMock,
      void(std::unique_ptr<webrtc::SessionDescriptionInterface>* desc,
           rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface>*
               observer));
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
  webrtc::RTCError SetConfiguration(
      const RTCConfiguration& configuration) override;

  bool AddIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void AddIceCandidate(std::unique_ptr<webrtc::IceCandidateInterface> candidate,
                       std::function<void(webrtc::RTCError)> callback) override;
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
  webrtc::PeerConnectionObserver* observer() { return observer_; }
  void set_setconfiguration_error_type(webrtc::RTCErrorType error_type) {
    setconfiguration_error_type_ = error_type;
  }
  static const char kDummyOffer[];
  static const char kDummyAnswer[];

  void AddAdaptationResource(
      rtc::scoped_refptr<webrtc::Resource> resource) override {
    adaptation_resources_.push_back(resource);
  }

  Vector<rtc::scoped_refptr<webrtc::Resource>> adaptation_resources() const {
    return adaptation_resources_;
  }

 protected:
  ~MockPeerConnectionImpl() override;

 private:
  std::string stream_label_;
  std::vector<std::string> local_stream_ids_ ALLOW_DISCOURAGED_TYPE(
      "Avoids conversion when implementing webrtc::PeerConnectionInterface");
  rtc::scoped_refptr<MockStreamCollection> remote_streams_;
  Vector<rtc::scoped_refptr<FakeRtpSender>> senders_;
  Vector<rtc::scoped_refptr<FakeRtpTransceiver>> transceivers_;
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
  raw_ptr<webrtc::PeerConnectionObserver> observer_;
  webrtc::RTCErrorType setconfiguration_error_type_ =
      webrtc::RTCErrorType::NONE;
  rtc::scoped_refptr<webrtc::RTCStatsReport> stats_report_;
  Vector<rtc::scoped_refptr<webrtc::Resource>> adaptation_resources_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_MOCK_PEER_CONNECTION_IMPL_H_
