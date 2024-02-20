// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_data_channel_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_platform.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/webrtc/api/rtp_receiver_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using testing::_;
using webrtc::AudioTrackInterface;
using webrtc::CreateSessionDescriptionObserver;
using webrtc::DtmfSenderInterface;
using webrtc::DtmfSenderObserverInterface;
using webrtc::IceCandidateInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::SetSessionDescriptionObserver;

namespace blink {

class MockStreamCollection : public webrtc::StreamCollectionInterface {
 public:
  size_t count() override { return streams_.size(); }
  MediaStreamInterface* at(size_t index) override {
    return streams_[index].get();
  }
  MediaStreamInterface* find(const std::string& id) override {
    for (size_t i = 0; i < streams_.size(); ++i) {
      if (streams_[i]->id() == id)
        return streams_[i].get();
    }
    return nullptr;
  }
  webrtc::MediaStreamTrackInterface* FindAudioTrack(
      const std::string& id) override {
    for (size_t i = 0; i < streams_.size(); ++i) {
      webrtc::MediaStreamTrackInterface* track =
          streams_.at(i)->FindAudioTrack(id).get();
      if (track)
        return track;
    }
    return nullptr;
  }
  webrtc::MediaStreamTrackInterface* FindVideoTrack(
      const std::string& id) override {
    for (size_t i = 0; i < streams_.size(); ++i) {
      webrtc::MediaStreamTrackInterface* track =
          streams_.at(i)->FindVideoTrack(id).get();
      if (track)
        return track;
    }
    return nullptr;
  }
  void AddStream(MediaStreamInterface* stream) {
    streams_.emplace_back(stream);
  }
  void RemoveStream(MediaStreamInterface* stream) {
    auto it = streams_.begin();
    for (; it != streams_.end(); ++it) {
      if (it->get() == stream) {
        streams_.erase(it);
        break;
      }
    }
  }

 protected:
  ~MockStreamCollection() override {}

 private:
  typedef std::vector<rtc::scoped_refptr<MediaStreamInterface>> StreamVector
      ALLOW_DISCOURAGED_TYPE(
          "Avoids conversion when implementing "
          "webrtc::StreamCollectionInterface");
  StreamVector streams_;
};

class MockDtmfSender : public DtmfSenderInterface {
 public:
  void RegisterObserver(DtmfSenderObserverInterface* observer) override {
    observer_ = observer;
  }
  void UnregisterObserver() override { observer_ = nullptr; }
  bool CanInsertDtmf() override { return true; }
  bool InsertDtmf(const std::string& tones,
                  int duration,
                  int inter_tone_gap) override {
    tones_ = tones;
    duration_ = duration;
    inter_tone_gap_ = inter_tone_gap;
    return true;
  }
  std::string tones() const override { return tones_; }
  int duration() const override { return duration_; }
  int inter_tone_gap() const override { return inter_tone_gap_; }

 private:
  raw_ptr<DtmfSenderObserverInterface> observer_ = nullptr;
  std::string tones_;
  int duration_ = 0;
  int inter_tone_gap_ = 0;
};

FakeRtpSender::FakeRtpSender(
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
    std::vector<std::string> stream_ids)
    : track_(std::move(track)), stream_ids_(std::move(stream_ids)) {}

FakeRtpSender::~FakeRtpSender() {}

bool FakeRtpSender::SetTrack(webrtc::MediaStreamTrackInterface* track) {
  track_ = track;
  return true;
}

rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> FakeRtpSender::track()
    const {
  return track_;
}

rtc::scoped_refptr<webrtc::DtlsTransportInterface>
FakeRtpSender::dtls_transport() const {
  return transport_;
}

uint32_t FakeRtpSender::ssrc() const {
  NOTIMPLEMENTED();
  return 0;
}

cricket::MediaType FakeRtpSender::media_type() const {
  NOTIMPLEMENTED();
  return cricket::MEDIA_TYPE_AUDIO;
}

std::string FakeRtpSender::id() const {
  NOTIMPLEMENTED();
  return "";
}

std::vector<std::string> FakeRtpSender::stream_ids() const {
  return stream_ids_;
}

void FakeRtpSender::SetStreams(const std::vector<std::string>& stream_ids) {
  stream_ids_ = stream_ids;
}

std::vector<webrtc::RtpEncodingParameters> FakeRtpSender::init_send_encodings()
    const {
  return {};
}

webrtc::RtpParameters FakeRtpSender::GetParameters() const {
  NOTIMPLEMENTED();
  return webrtc::RtpParameters();
}

webrtc::RTCError FakeRtpSender::SetParameters(
    const webrtc::RtpParameters& parameters) {
  NOTIMPLEMENTED();
  return webrtc::RTCError::OK();
}

rtc::scoped_refptr<webrtc::DtmfSenderInterface> FakeRtpSender::GetDtmfSender()
    const {
  return rtc::scoped_refptr<webrtc::DtmfSenderInterface>(
      new rtc::RefCountedObject<MockDtmfSender>());
}

FakeRtpReceiver::FakeRtpReceiver(
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
    std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>> streams)
    : track_(std::move(track)), streams_(std::move(streams)) {}

FakeRtpReceiver::~FakeRtpReceiver() {}

rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> FakeRtpReceiver::track()
    const {
  return track_;
}

rtc::scoped_refptr<webrtc::DtlsTransportInterface>
FakeRtpReceiver::dtls_transport() const {
  return transport_;
}

std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
FakeRtpReceiver::streams() const {
  return streams_;
}

std::vector<std::string> FakeRtpReceiver::stream_ids() const {
  std::vector<std::string> stream_ids;
  for (const auto& stream : streams_)
    stream_ids.push_back(stream->id());
  return stream_ids;
}

cricket::MediaType FakeRtpReceiver::media_type() const {
  NOTIMPLEMENTED();
  return cricket::MEDIA_TYPE_AUDIO;
}

std::string FakeRtpReceiver::id() const {
  NOTIMPLEMENTED();
  return "";
}

webrtc::RtpParameters FakeRtpReceiver::GetParameters() const {
  NOTIMPLEMENTED();
  return webrtc::RtpParameters();
}

bool FakeRtpReceiver::SetParameters(const webrtc::RtpParameters& parameters) {
  NOTIMPLEMENTED();
  return false;
}

void FakeRtpReceiver::SetObserver(
    webrtc::RtpReceiverObserverInterface* observer) {
  NOTIMPLEMENTED();
}

void FakeRtpReceiver::SetJitterBufferMinimumDelay(
    std::optional<double> delay_seconds) {
  NOTIMPLEMENTED();
}

std::vector<webrtc::RtpSource> FakeRtpReceiver::GetSources() const {
  NOTIMPLEMENTED();
  return std::vector<webrtc::RtpSource>();
}

FakeRtpTransceiver::FakeRtpTransceiver(
    cricket::MediaType media_type,
    rtc::scoped_refptr<FakeRtpSender> sender,
    rtc::scoped_refptr<FakeRtpReceiver> receiver,
    std::optional<std::string> mid,
    bool stopped,
    webrtc::RtpTransceiverDirection direction,
    std::optional<webrtc::RtpTransceiverDirection> current_direction)
    : media_type_(media_type),
      sender_(std::move(sender)),
      receiver_(std::move(receiver)),
      mid_(std::move(mid)),
      stopped_(stopped),
      direction_(direction),
      current_direction_(current_direction) {}

FakeRtpTransceiver::~FakeRtpTransceiver() = default;

void FakeRtpTransceiver::ReplaceWith(const FakeRtpTransceiver& other) {
  media_type_ = other.media_type_;
  sender_ = other.sender_;
  receiver_ = other.receiver_;
  mid_ = other.mid_;
  stopped_ = other.stopped_;
  direction_ = other.direction_;
  current_direction_ = other.current_direction_;
}

cricket::MediaType FakeRtpTransceiver::media_type() const {
  return media_type_;
}

std::optional<std::string> FakeRtpTransceiver::mid() const {
  return mid_;
}

rtc::scoped_refptr<webrtc::RtpSenderInterface> FakeRtpTransceiver::sender()
    const {
  return sender_;
}

rtc::scoped_refptr<webrtc::RtpReceiverInterface> FakeRtpTransceiver::receiver()
    const {
  return receiver_;
}

bool FakeRtpTransceiver::stopped() const {
  return stopped_;
}

bool FakeRtpTransceiver::stopping() const {
  NOTIMPLEMENTED();
  return false;
}

webrtc::RtpTransceiverDirection FakeRtpTransceiver::direction() const {
  return direction_;
}

std::optional<webrtc::RtpTransceiverDirection>
FakeRtpTransceiver::current_direction() const {
  return current_direction_;
}

void FakeRtpTransceiver::SetTransport(
    rtc::scoped_refptr<webrtc::DtlsTransportInterface> transport) {
  sender_->SetTransport(transport);
  receiver_->SetTransport(transport);
}

FakeDtlsTransport::FakeDtlsTransport() {}

rtc::scoped_refptr<webrtc::IceTransportInterface>
FakeDtlsTransport::ice_transport() {
  return nullptr;
}

webrtc::DtlsTransportInformation FakeDtlsTransport::Information() {
  return webrtc::DtlsTransportInformation(webrtc::DtlsTransportState::kNew);
}

const char MockPeerConnectionImpl::kDummyOffer[] = "dummy offer";
const char MockPeerConnectionImpl::kDummyAnswer[] = "dummy answer";

MockPeerConnectionImpl::MockPeerConnectionImpl(
    MockPeerConnectionDependencyFactory* factory,
    webrtc::PeerConnectionObserver* observer)
    : remote_streams_(new rtc::RefCountedObject<MockStreamCollection>),
      hint_audio_(false),
      hint_video_(false),
      getstats_result_(true),
      sdp_mline_index_(-1),
      observer_(observer) {
  // TODO(hbos): Remove once no longer mandatory to implement.
  ON_CALL(*this, SetLocalDescription(_, _))
      .WillByDefault(testing::Invoke(
          this, &MockPeerConnectionImpl::SetLocalDescriptionWorker));
  ON_CALL(*this, SetLocalDescriptionForMock(_, _))
      .WillByDefault(testing::Invoke(
          [this](
              std::unique_ptr<webrtc::SessionDescriptionInterface>* desc,
              rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface>*
                  observer) {
            SetLocalDescriptionWorker(nullptr, desc->release());
          }));
  // TODO(hbos): Remove once no longer mandatory to implement.
  ON_CALL(*this, SetRemoteDescription(_, _))
      .WillByDefault(testing::Invoke(
          this, &MockPeerConnectionImpl::SetRemoteDescriptionWorker));
  ON_CALL(*this, SetRemoteDescriptionForMock(_, _))
      .WillByDefault(testing::Invoke(
          [this](
              std::unique_ptr<webrtc::SessionDescriptionInterface>* desc,
              rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>*
                  observer) {
            SetRemoteDescriptionWorker(nullptr, desc->release());
          }));
}

MockPeerConnectionImpl::~MockPeerConnectionImpl() {}

webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>
MockPeerConnectionImpl::AddTrack(
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
  DCHECK(track);
  DCHECK_EQ(1u, stream_ids.size());
  for (const auto& sender : senders_) {
    if (sender->track() == track)
      return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER);
  }
  for (const auto& stream_id : stream_ids) {
    if (!base::Contains(local_stream_ids_, stream_id)) {
      stream_label_ = stream_id;
      local_stream_ids_.push_back(stream_id);
    }
  }
  rtc::scoped_refptr<FakeRtpSender> sender(
      new rtc::RefCountedObject<FakeRtpSender>(track, stream_ids));
  senders_.push_back(sender);
  // This mock is dumb. It creates an audio transceiver without checking the
  // kind of the sender track.
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> dummy_receiver_track(
      blink::MockWebRtcAudioTrack::Create("dummy_track").get());
  rtc::scoped_refptr<FakeRtpReceiver> dummy_receiver(
      new rtc::RefCountedObject<FakeRtpReceiver>(dummy_receiver_track));
  rtc::scoped_refptr<FakeRtpTransceiver> transceiver(
      new rtc::RefCountedObject<FakeRtpTransceiver>(
          cricket::MediaType::MEDIA_TYPE_AUDIO, sender, dummy_receiver,
          std::nullopt, false, webrtc::RtpTransceiverDirection::kSendRecv,
          std::nullopt));
  transceivers_.push_back(transceiver);
  return rtc::scoped_refptr<webrtc::RtpSenderInterface>(sender);
}

webrtc::RTCError MockPeerConnectionImpl::RemoveTrackOrError(
    rtc::scoped_refptr<webrtc::RtpSenderInterface> s) {
  rtc::scoped_refptr<FakeRtpSender> sender(
      static_cast<FakeRtpSender*>(s.get()));
  if (!base::Contains(senders_, sender)) {
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER,
                            "Mock: sender not found in senders");
  }
  sender->SetTrack(nullptr);

  for (const auto& stream_id : sender->stream_ids()) {
    auto local_stream_it = base::ranges::find(local_stream_ids_, stream_id);
    if (local_stream_it != local_stream_ids_.end())
      local_stream_ids_.erase(local_stream_it);
  }
  return webrtc::RTCError::OK();
}

std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>>
MockPeerConnectionImpl::GetSenders() const {
  std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> senders;
  for (const auto& sender : senders_)
    senders.push_back(sender);
  return senders;
}

std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>>
MockPeerConnectionImpl::GetReceivers() const {
  std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>> receivers;
  for (size_t i = 0; i < remote_streams_->count(); ++i) {
    for (const auto& audio_track : remote_streams_->at(i)->GetAudioTracks()) {
      receivers.emplace_back(
          new rtc::RefCountedObject<FakeRtpReceiver>(audio_track));
    }
    for (const auto& video_track : remote_streams_->at(i)->GetVideoTracks()) {
      receivers.emplace_back(
          new rtc::RefCountedObject<FakeRtpReceiver>(video_track));
    }
  }
  return receivers;
}

std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
MockPeerConnectionImpl::GetTransceivers() const {
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> transceivers;
  for (const auto& transceiver : transceivers_)
    transceivers.push_back(transceiver);
  return transceivers;
}

webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::DataChannelInterface>>
MockPeerConnectionImpl::CreateDataChannelOrError(
    const std::string& label,
    const webrtc::DataChannelInit* config) {
  return rtc::scoped_refptr<webrtc::DataChannelInterface>(
      new rtc::RefCountedObject<blink::MockDataChannel>(label, config));
}

bool MockPeerConnectionImpl::GetStats(webrtc::StatsObserver* observer,
                                      webrtc::MediaStreamTrackInterface* track,
                                      StatsOutputLevel level) {
  if (!getstats_result_)
    return false;

  DCHECK_EQ(kStatsOutputLevelStandard, level);
  webrtc::StatsReport report1(webrtc::StatsReport::NewTypedId(
      webrtc::StatsReport::kStatsReportTypeSsrc, "1234"));
  webrtc::StatsReport report2(webrtc::StatsReport::NewTypedId(
      webrtc::StatsReport::kStatsReportTypeSession, "nontrack"));
  report1.set_timestamp(42);
  report1.AddString(webrtc::StatsReport::kStatsValueNameFingerprint,
                    "trackvalue");

  webrtc::StatsReports reports;
  reports.push_back(&report1);

  // If selector is given, we pass back one report.
  // If selector is not given, we pass back two.
  if (!track) {
    report2.set_timestamp(44);
    report2.AddString(webrtc::StatsReport::kStatsValueNameFingerprintAlgorithm,
                      "somevalue");
    reports.push_back(&report2);
  }

  // Note that the callback is synchronous, not asynchronous; it will
  // happen before the request call completes.
  observer->OnComplete(reports);

  return true;
}

void MockPeerConnectionImpl::GetStats(
    webrtc::RTCStatsCollectorCallback* callback) {
  DCHECK(callback);
  DCHECK(stats_report_);
  callback->OnStatsDelivered(stats_report_);
}

void MockPeerConnectionImpl::GetStats(
    rtc::scoped_refptr<webrtc::RtpSenderInterface> selector,
    rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback> callback) {
  callback->OnStatsDelivered(stats_report_);
}

void MockPeerConnectionImpl::GetStats(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> selector,
    rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback> callback) {
  callback->OnStatsDelivered(stats_report_);
}

void MockPeerConnectionImpl::SetGetStatsReport(webrtc::RTCStatsReport* report) {
  stats_report_ = report;
}

const webrtc::SessionDescriptionInterface*
MockPeerConnectionImpl::local_description() const {
  return local_desc_.get();
}

const webrtc::SessionDescriptionInterface*
MockPeerConnectionImpl::remote_description() const {
  return remote_desc_.get();
}

void MockPeerConnectionImpl::AddRemoteStream(MediaStreamInterface* stream) {
  remote_streams_->AddStream(stream);
}

void MockPeerConnectionImpl::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const RTCOfferAnswerOptions& options) {
  DCHECK(observer);
  created_sessiondescription_ =
      MockParsedSessionDescription("unknown", kDummyAnswer).release();
}

void MockPeerConnectionImpl::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const RTCOfferAnswerOptions& options) {
  DCHECK(observer);
  created_sessiondescription_ =
      MockParsedSessionDescription("unknown", kDummyAnswer).release();
}

void MockPeerConnectionImpl::SetLocalDescriptionWorker(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  desc->ToString(&description_sdp_);
  local_desc_.reset(desc);
}

void MockPeerConnectionImpl::SetRemoteDescriptionWorker(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  desc->ToString(&description_sdp_);
  remote_desc_.reset(desc);
}

webrtc::RTCError MockPeerConnectionImpl::SetConfiguration(
    const RTCConfiguration& configuration) {
  return webrtc::RTCError(setconfiguration_error_type_);
}

bool MockPeerConnectionImpl::AddIceCandidate(
    const IceCandidateInterface* candidate) {
  sdp_mid_ = candidate->sdp_mid();
  sdp_mline_index_ = candidate->sdp_mline_index();
  return candidate->ToString(&ice_sdp_);
}

void MockPeerConnectionImpl::AddIceCandidate(
    std::unique_ptr<webrtc::IceCandidateInterface> candidate,
    std::function<void(webrtc::RTCError)> callback) {
  bool result = AddIceCandidate(candidate.get());
  callback(result
               ? webrtc::RTCError::OK()
               : webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION));
}

}  // namespace blink
