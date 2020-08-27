// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_platform.h"

#include <utility>

#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_dtmf_sender_handler.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_source.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_transceiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"

namespace blink {

namespace {

// Having a refcounted helper class allows multiple DummyRTCRtpSenderPlatform to
// share the same internal states.
class DummyRtpSenderInternal
    : public WTF::ThreadSafeRefCounted<DummyRtpSenderInternal> {
 private:
  static uintptr_t last_id_;

 public:
  explicit DummyRtpSenderInternal(MediaStreamComponent* component)
      : id_(++last_id_), component_(component) {}

  uintptr_t id() const { return id_; }
  MediaStreamComponent* track() const { return component_; }
  void set_track(MediaStreamComponent* component) { component_ = component; }

 private:
  const uintptr_t id_;
  Persistent<MediaStreamComponent> component_;
};

uintptr_t DummyRtpSenderInternal::last_id_ = 0;

class DummyRTCRtpSenderPlatform : public RTCRtpSenderPlatform {
 public:
  explicit DummyRTCRtpSenderPlatform(MediaStreamComponent* component)
      : internal_(base::MakeRefCounted<DummyRtpSenderInternal>(component)) {}
  DummyRTCRtpSenderPlatform(const DummyRTCRtpSenderPlatform& other)
      : internal_(other.internal_) {}
  ~DummyRTCRtpSenderPlatform() override {}

  scoped_refptr<DummyRtpSenderInternal> internal() const { return internal_; }

  std::unique_ptr<RTCRtpSenderPlatform> ShallowCopy() const override {
    return nullptr;
  }
  uintptr_t Id() const override { return internal_->id(); }
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> DtlsTransport() override {
    return nullptr;
  }
  webrtc::DtlsTransportInformation DtlsTransportInformation() override {
    static const webrtc::DtlsTransportInformation dummy(
        webrtc::DtlsTransportState::kNew);
    return dummy;
  }
  MediaStreamComponent* Track() const override { return internal_->track(); }
  Vector<String> StreamIds() const override {
    return Vector<String>({String::FromUTF8("DummyStringId")});
  }
  void ReplaceTrack(MediaStreamComponent*, RTCVoidRequest*) override {}
  std::unique_ptr<RtcDtmfSenderHandler> GetDtmfSender() const override {
    return nullptr;
  }
  std::unique_ptr<webrtc::RtpParameters> GetParameters() const override {
    return std::unique_ptr<webrtc::RtpParameters>();
  }
  void SetParameters(Vector<webrtc::RtpEncodingParameters>,
                     absl::optional<webrtc::DegradationPreference>,
                     RTCVoidRequest*) override {}
  void GetStats(RTCStatsReportCallback,
                const Vector<webrtc::NonStandardGroupId>&) override {}
  void SetStreams(const Vector<String>& stream_ids) override {}

 private:
  scoped_refptr<DummyRtpSenderInternal> internal_;
};

class DummyRTCRtpReceiverPlatform : public RTCRtpReceiverPlatform {
 private:
  static uintptr_t last_id_;

 public:
  explicit DummyRTCRtpReceiverPlatform(MediaStreamSource::StreamType type)
      : id_(++last_id_) {
    if (type == MediaStreamSource::StreamType::kTypeAudio) {
      auto* source = MakeGarbageCollected<MediaStreamSource>(
          String::FromUTF8("remoteAudioId"),
          MediaStreamSource::StreamType::kTypeAudio,
          String::FromUTF8("remoteAudioName"), true /* remote */);
      component_ =
          MakeGarbageCollected<MediaStreamComponent>(source->Id(), source);
    } else {
      DCHECK_EQ(type, MediaStreamSource::StreamType::kTypeVideo);
      auto* source = MakeGarbageCollected<MediaStreamSource>(
          String::FromUTF8("remoteVideoId"),
          MediaStreamSource::StreamType::kTypeVideo,
          String::FromUTF8("remoteVideoName"), true /* remote */);
      component_ =
          MakeGarbageCollected<MediaStreamComponent>(source->Id(), source);
    }
  }
  DummyRTCRtpReceiverPlatform(const DummyRTCRtpReceiverPlatform& other)
      : id_(other.id_), component_(other.component_) {}
  ~DummyRTCRtpReceiverPlatform() override = default;

  std::unique_ptr<RTCRtpReceiverPlatform> ShallowCopy() const override {
    return nullptr;
  }
  uintptr_t Id() const override { return id_; }
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> DtlsTransport() override {
    return nullptr;
  }
  webrtc::DtlsTransportInformation DtlsTransportInformation() override {
    static const webrtc::DtlsTransportInformation dummy(
        webrtc::DtlsTransportState::kNew);
    return dummy;
  }
  MediaStreamComponent* Track() const override { return component_; }
  Vector<String> StreamIds() const override { return Vector<String>(); }
  Vector<std::unique_ptr<RTCRtpSource>> GetSources() override {
    return Vector<std::unique_ptr<RTCRtpSource>>();
  }
  void GetStats(RTCStatsReportCallback,
                const Vector<webrtc::NonStandardGroupId>&) override {}
  std::unique_ptr<webrtc::RtpParameters> GetParameters() const override {
    return nullptr;
  }

  void SetJitterBufferMinimumDelay(
      base::Optional<double> delay_seconds) override {}

 private:
  const uintptr_t id_;
  Persistent<MediaStreamComponent> component_;
};

uintptr_t DummyRTCRtpReceiverPlatform::last_id_ = 0;

// Having a refcounted helper class allows multiple
// DummyRTCRtpTransceiverPlatforms to share the same internal states.
class DummyTransceiverInternal
    : public WTF::ThreadSafeRefCounted<DummyTransceiverInternal> {
 private:
  static uintptr_t last_id_;

 public:
  DummyTransceiverInternal(MediaStreamSource::StreamType type,
                           MediaStreamComponent* sender_component)
      : id_(++last_id_),
        sender_(sender_component),
        receiver_(type),
        direction_(webrtc::RtpTransceiverDirection::kSendRecv) {
    DCHECK(!sender_.Track() ||
           sender_.Track()->Source()->GetType() ==
               static_cast<MediaStreamSource::StreamType>(type));
  }

  uintptr_t id() const { return id_; }
  DummyRTCRtpSenderPlatform* sender() { return &sender_; }
  std::unique_ptr<DummyRTCRtpSenderPlatform> Sender() const {
    return std::make_unique<DummyRTCRtpSenderPlatform>(sender_);
  }
  DummyRTCRtpReceiverPlatform* receiver() { return &receiver_; }
  std::unique_ptr<DummyRTCRtpReceiverPlatform> Receiver() const {
    return std::make_unique<DummyRTCRtpReceiverPlatform>(receiver_);
  }
  webrtc::RtpTransceiverDirection direction() const { return direction_; }
  webrtc::RTCError set_direction(webrtc::RtpTransceiverDirection direction) {
    direction_ = direction;
    return webrtc::RTCError::OK();
  }

 private:
  const uintptr_t id_;
  DummyRTCRtpSenderPlatform sender_;
  DummyRTCRtpReceiverPlatform receiver_;
  webrtc::RtpTransceiverDirection direction_;
};

uintptr_t DummyTransceiverInternal::last_id_ = 0;

}  // namespace

class MockRTCPeerConnectionHandlerPlatform::DummyRTCRtpTransceiverPlatform
    : public RTCRtpTransceiverPlatform {
 public:
  DummyRTCRtpTransceiverPlatform(MediaStreamSource::StreamType type,
                                 MediaStreamComponent* component)
      : internal_(
            base::MakeRefCounted<DummyTransceiverInternal>(type, component)) {}
  DummyRTCRtpTransceiverPlatform(const DummyRTCRtpTransceiverPlatform& other)
      : internal_(other.internal_) {}
  ~DummyRTCRtpTransceiverPlatform() override {}

  scoped_refptr<DummyTransceiverInternal> internal() const { return internal_; }

  RTCRtpTransceiverPlatformImplementationType ImplementationType()
      const override {
    return RTCRtpTransceiverPlatformImplementationType::kFullTransceiver;
  }
  uintptr_t Id() const override { return internal_->id(); }
  String Mid() const override { return String(); }
  std::unique_ptr<RTCRtpSenderPlatform> Sender() const override {
    return internal_->Sender();
  }
  std::unique_ptr<RTCRtpReceiverPlatform> Receiver() const override {
    return internal_->Receiver();
  }
  bool Stopped() const override { return true; }
  webrtc::RtpTransceiverDirection Direction() const override {
    return internal_->direction();
  }
  webrtc::RTCError SetDirection(
      webrtc::RtpTransceiverDirection direction) override {
    return internal_->set_direction(direction);
  }
  base::Optional<webrtc::RtpTransceiverDirection> CurrentDirection()
      const override {
    return base::nullopt;
  }
  base::Optional<webrtc::RtpTransceiverDirection> FiredDirection()
      const override {
    return base::nullopt;
  }

 private:
  scoped_refptr<DummyTransceiverInternal> internal_;
};

MockRTCPeerConnectionHandlerPlatform::MockRTCPeerConnectionHandlerPlatform()
    : RTCPeerConnectionHandler(
          scheduler::GetSingleThreadTaskRunnerForTesting()) {}

MockRTCPeerConnectionHandlerPlatform::~MockRTCPeerConnectionHandlerPlatform() =
    default;

bool MockRTCPeerConnectionHandlerPlatform::Initialize(
    const webrtc::PeerConnectionInterface::RTCConfiguration&,
    const MediaConstraints&,
    WebLocalFrame*) {
  return true;
}

Vector<std::unique_ptr<RTCRtpTransceiverPlatform>>
MockRTCPeerConnectionHandlerPlatform::CreateOffer(RTCSessionDescriptionRequest*,
                                                  const MediaConstraints&) {
  return {};
}

Vector<std::unique_ptr<RTCRtpTransceiverPlatform>>
MockRTCPeerConnectionHandlerPlatform::CreateOffer(RTCSessionDescriptionRequest*,
                                                  RTCOfferOptionsPlatform*) {
  return {};
}

void MockRTCPeerConnectionHandlerPlatform::CreateAnswer(
    RTCSessionDescriptionRequest*,
    const MediaConstraints&) {}

void MockRTCPeerConnectionHandlerPlatform::CreateAnswer(
    RTCSessionDescriptionRequest*,
    RTCAnswerOptionsPlatform*) {}

void MockRTCPeerConnectionHandlerPlatform::SetLocalDescription(
    RTCVoidRequest*) {}

void MockRTCPeerConnectionHandlerPlatform::SetLocalDescription(
    RTCVoidRequest*,
    RTCSessionDescriptionPlatform*) {}

void MockRTCPeerConnectionHandlerPlatform::SetRemoteDescription(
    RTCVoidRequest*,
    RTCSessionDescriptionPlatform*) {}

const webrtc::PeerConnectionInterface::RTCConfiguration&
MockRTCPeerConnectionHandlerPlatform::GetConfiguration() const {
  static const webrtc::PeerConnectionInterface::RTCConfiguration configuration;
  return configuration;
}

webrtc::RTCErrorType MockRTCPeerConnectionHandlerPlatform::SetConfiguration(
    const webrtc::PeerConnectionInterface::RTCConfiguration&) {
  return webrtc::RTCErrorType::NONE;
}

void MockRTCPeerConnectionHandlerPlatform::AddICECandidate(
    RTCVoidRequest*,
    RTCIceCandidatePlatform*) {}

void MockRTCPeerConnectionHandlerPlatform::RestartIce() {}

void MockRTCPeerConnectionHandlerPlatform::GetStats(RTCStatsRequest*) {}

void MockRTCPeerConnectionHandlerPlatform::GetStats(
    RTCStatsReportCallback,
    const Vector<webrtc::NonStandardGroupId>&) {}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
MockRTCPeerConnectionHandlerPlatform::AddTransceiverWithTrack(
    MediaStreamComponent* component,
    const webrtc::RtpTransceiverInit&) {
  transceivers_.push_back(std::unique_ptr<DummyRTCRtpTransceiverPlatform>(
      new DummyRTCRtpTransceiverPlatform(component->Source()->GetType(),
                                         component)));
  std::unique_ptr<DummyRTCRtpTransceiverPlatform> copy(
      new DummyRTCRtpTransceiverPlatform(*transceivers_.back()));
  return std::unique_ptr<RTCRtpTransceiverPlatform>(std::move(copy));
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
MockRTCPeerConnectionHandlerPlatform::AddTransceiverWithKind(
    const String& kind,
    const webrtc::RtpTransceiverInit&) {
  transceivers_.push_back(std::unique_ptr<DummyRTCRtpTransceiverPlatform>(
      new DummyRTCRtpTransceiverPlatform(
          kind == "audio" ? MediaStreamSource::StreamType::kTypeAudio
                          : MediaStreamSource::StreamType::kTypeVideo,
          nullptr /*MediaStreamComponent*/)));
  std::unique_ptr<DummyRTCRtpTransceiverPlatform> copy(
      new DummyRTCRtpTransceiverPlatform(*transceivers_.back()));
  return std::unique_ptr<RTCRtpTransceiverPlatform>(std::move(copy));
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
MockRTCPeerConnectionHandlerPlatform::AddTrack(
    MediaStreamComponent* component,
    const MediaStreamDescriptorVector&) {
  transceivers_.push_back(std::unique_ptr<DummyRTCRtpTransceiverPlatform>(
      new DummyRTCRtpTransceiverPlatform(component->Source()->GetType(),
                                         component)));
  std::unique_ptr<DummyRTCRtpTransceiverPlatform> copy(
      new DummyRTCRtpTransceiverPlatform(*transceivers_.back()));
  return std::unique_ptr<RTCRtpTransceiverPlatform>(std::move(copy));
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
MockRTCPeerConnectionHandlerPlatform::RemoveTrack(
    RTCRtpSenderPlatform* sender) {
  const DummyRTCRtpTransceiverPlatform* transceiver_of_sender = nullptr;
  for (const auto& transceiver : transceivers_) {
    if (transceiver->Sender()->Id() == sender->Id()) {
      transceiver_of_sender = transceiver.get();
      break;
    }
  }
  transceiver_of_sender->internal()->sender()->internal()->set_track(nullptr);
  std::unique_ptr<DummyRTCRtpTransceiverPlatform> copy(
      new DummyRTCRtpTransceiverPlatform(*transceiver_of_sender));
  return std::unique_ptr<RTCRtpTransceiverPlatform>(std::move(copy));
}

scoped_refptr<webrtc::DataChannelInterface>
MockRTCPeerConnectionHandlerPlatform::CreateDataChannel(
    const String& label,
    const webrtc::DataChannelInit&) {
  return nullptr;
}

void MockRTCPeerConnectionHandlerPlatform::Stop() {}
void MockRTCPeerConnectionHandlerPlatform::StopAndUnregister() {}

webrtc::PeerConnectionInterface*
MockRTCPeerConnectionHandlerPlatform::NativePeerConnection() {
  return nullptr;
}

void MockRTCPeerConnectionHandlerPlatform::
    RunSynchronousOnceClosureOnSignalingThread(CrossThreadOnceClosure closure,
                                               const char* trace_event_name) {}

void MockRTCPeerConnectionHandlerPlatform::
    RunSynchronousRepeatingClosureOnSignalingThread(
        const base::RepeatingClosure& closure,
        const char* trace_event_name) {}

void MockRTCPeerConnectionHandlerPlatform::TrackIceConnectionStateChange(
    RTCPeerConnectionHandler::IceConnectionStateVersion version,
    webrtc::PeerConnectionInterface::IceConnectionState state) {}

}  // namespace blink
