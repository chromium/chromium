// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_platform.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_dtmf_sender_handler.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
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

webrtc::PeerConnectionInterface::RTCConfiguration DefaultConfiguration() {
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  return config;
}

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
      : internal_(base::MakeRefCounted<DummyRtpSenderInternal>(component)),
        audio_transformer_(std::make_unique<RTCEncodedAudioStreamTransformer>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting())),
        video_transformer_(std::make_unique<RTCEncodedVideoStreamTransformer>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            nullptr)) {}
  DummyRTCRtpSenderPlatform(const DummyRTCRtpSenderPlatform& other)
      : internal_(other.internal_),
        audio_transformer_(std::make_unique<RTCEncodedAudioStreamTransformer>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting())),
        video_transformer_(std::make_unique<RTCEncodedVideoStreamTransformer>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            nullptr)) {}
  ~DummyRTCRtpSenderPlatform() override = default;

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
                     std::optional<webrtc::DegradationPreference>,
                     RTCVoidRequest*) override {}
  void GetStats(RTCStatsReportCallback) override {}
  void SetStreams(const Vector<String>& stream_ids) override {}

  RTCEncodedAudioStreamTransformer* GetEncodedAudioStreamTransformer()
      const override {
    return audio_transformer_.get();
  }

  RTCEncodedVideoStreamTransformer* GetEncodedVideoStreamTransformer()
      const override {
    return video_transformer_.get();
  }

 private:
  scoped_refptr<DummyRtpSenderInternal> internal_;
  std::unique_ptr<RTCEncodedAudioStreamTransformer> audio_transformer_;
  std::unique_ptr<RTCEncodedVideoStreamTransformer> video_transformer_;
};

class DummyRTCRtpReceiverPlatform : public RTCRtpReceiverPlatform {
 private:
  static uintptr_t last_id_;

 public:
  explicit DummyRTCRtpReceiverPlatform(MediaStreamSource::StreamType type)
      : id_(++last_id_),
        audio_transformer_(std::make_unique<RTCEncodedAudioStreamTransformer>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting())),
        video_transformer_(std::make_unique<RTCEncodedVideoStreamTransformer>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            nullptr)) {
    if (type == MediaStreamSource::StreamType::kTypeAudio) {
      auto* source = MakeGarbageCollected<MediaStreamSource>(
          String::FromUTF8("remoteAudioId"),
          MediaStreamSource::StreamType::kTypeAudio,
          String::FromUTF8("remoteAudioName"), /*remote=*/true,
          /*platform_source=*/nullptr);
      component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
          source->Id(), source,
          std::make_unique<MediaStreamAudioTrack>(/*is_local_track=*/false));
    } else {
      DCHECK_EQ(type, MediaStreamSource::StreamType::kTypeVideo);
      auto platform_source = std::make_unique<MockMediaStreamVideoSource>();
      auto* platform_source_ptr = platform_source.get();
      auto* source = MakeGarbageCollected<MediaStreamSource>(
          String::FromUTF8("remoteVideoId"),
          MediaStreamSource::StreamType::kTypeVideo,
          String::FromUTF8("remoteVideoName"), /*remote=*/true,
          std::move(platform_source));
      component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
          source->Id(), source,
          std::make_unique<MediaStreamVideoTrack>(
              platform_source_ptr,
              MediaStreamVideoSource::ConstraintsOnceCallback(),
              /*enabled=*/true));
    }
  }
  DummyRTCRtpReceiverPlatform(const DummyRTCRtpReceiverPlatform& other)
      : id_(other.id_),
        component_(other.component_),
        audio_transformer_(std::make_unique<RTCEncodedAudioStreamTransformer>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting())),
        video_transformer_(std::make_unique<RTCEncodedVideoStreamTransformer>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            nullptr)) {}
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
  void GetStats(RTCStatsReportCallback) override {}
  std::unique_ptr<webrtc::RtpParameters> GetParameters() const override {
    return nullptr;
  }

  void SetJitterBufferMinimumDelay(
      std::optional<double> delay_seconds) override {}

  RTCEncodedAudioStreamTransformer* GetEncodedAudioStreamTransformer()
      const override {
    return audio_transformer_.get();
  }

  RTCEncodedVideoStreamTransformer* GetEncodedVideoStreamTransformer()
      const override {
    return video_transformer_.get();
  }

 private:
  const uintptr_t id_;
  Persistent<MediaStreamComponent> component_;
  std::unique_ptr<RTCEncodedAudioStreamTransformer> audio_transformer_;
  std::unique_ptr<RTCEncodedVideoStreamTransformer> video_transformer_;
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
           sender_.Track()->GetSourceType() ==
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

  uintptr_t Id() const override { return internal_->id(); }
  String Mid() const override { return String(); }
  std::unique_ptr<RTCRtpSenderPlatform> Sender() const override {
    return internal_->Sender();
  }
  std::unique_ptr<RTCRtpReceiverPlatform> Receiver() const override {
    return internal_->Receiver();
  }
  webrtc::RtpTransceiverDirection Direction() const override {
    return internal_->direction();
  }
  webrtc::RTCError SetDirection(
      webrtc::RtpTransceiverDirection direction) override {
    return internal_->set_direction(direction);
  }
  std::optional<webrtc::RtpTransceiverDirection> CurrentDirection()
      const override {
    return std::nullopt;
  }
  std::optional<webrtc::RtpTransceiverDirection> FiredDirection()
      const override {
    return std::nullopt;
  }
  webrtc::RTCError Stop() override { return webrtc::RTCError::OK(); }
  webrtc::RTCError SetCodecPreferences(
      Vector<webrtc::RtpCodecCapability>) override {
    return webrtc::RTCError::OK();
  }
  webrtc::RTCError SetHeaderExtensionsToNegotiate(
      Vector<webrtc::RtpHeaderExtensionCapability> header_extensions) override {
    return webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION);
  }
  Vector<webrtc::RtpHeaderExtensionCapability> GetNegotiatedHeaderExtensions()
      const override {
    return {};
  }
  Vector<webrtc::RtpHeaderExtensionCapability> GetHeaderExtensionsToNegotiate()
      const override {
    return {};
  }

 private:
  scoped_refptr<DummyTransceiverInternal> internal_;
};

MockRTCPeerConnectionHandlerPlatform::MockRTCPeerConnectionHandlerPlatform()
    : RTCPeerConnectionHandler(
          scheduler::GetSingleThreadTaskRunnerForTesting()),
      native_peer_connection_(webrtc::MockPeerConnectionInterface::Create()) {}

MockRTCPeerConnectionHandlerPlatform::~MockRTCPeerConnectionHandlerPlatform() =
    default;

bool MockRTCPeerConnectionHandlerPlatform::Initialize(
    ExecutionContext*,
    const webrtc::PeerConnectionInterface::RTCConfiguration&,
    WebLocalFrame*,
    ExceptionState&,
    RTCRtpTransport*) {
  return true;
}

Vector<std::unique_ptr<RTCRtpTransceiverPlatform>>
MockRTCPeerConnectionHandlerPlatform::CreateOffer(RTCSessionDescriptionRequest*,
                                                  RTCOfferOptionsPlatform*) {
  return {};
}

void MockRTCPeerConnectionHandlerPlatform::CreateAnswer(
    RTCSessionDescriptionRequest*,
    RTCAnswerOptionsPlatform*) {}

void MockRTCPeerConnectionHandlerPlatform::SetLocalDescription(
    RTCVoidRequest*) {}

void MockRTCPeerConnectionHandlerPlatform::SetLocalDescription(
    RTCVoidRequest*,
    ParsedSessionDescription) {}

void MockRTCPeerConnectionHandlerPlatform::SetRemoteDescription(
    RTCVoidRequest*,
    ParsedSessionDescription) {}

const webrtc::PeerConnectionInterface::RTCConfiguration&
MockRTCPeerConnectionHandlerPlatform::GetConfiguration() const {
  static const webrtc::PeerConnectionInterface::RTCConfiguration configuration =
      DefaultConfiguration();
  return configuration;
}

webrtc::RTCErrorType MockRTCPeerConnectionHandlerPlatform::SetConfiguration(
    const webrtc::PeerConnectionInterface::RTCConfiguration&) {
  return webrtc::RTCErrorType::NONE;
}

void MockRTCPeerConnectionHandlerPlatform::AddIceCandidate(
    RTCVoidRequest*,
    RTCIceCandidatePlatform*) {}

void MockRTCPeerConnectionHandlerPlatform::RestartIce() {}

void MockRTCPeerConnectionHandlerPlatform::GetStats(RTCStatsReportCallback) {}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
MockRTCPeerConnectionHandlerPlatform::AddTransceiverWithTrack(
    MediaStreamComponent* component,
    const webrtc::RtpTransceiverInit&) {
  transceivers_.push_back(std::make_unique<DummyRTCRtpTransceiverPlatform>(
      component->GetSourceType(), component));
  std::unique_ptr<DummyRTCRtpTransceiverPlatform> copy(
      new DummyRTCRtpTransceiverPlatform(*transceivers_.back()));
  return std::unique_ptr<RTCRtpTransceiverPlatform>(std::move(copy));
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
MockRTCPeerConnectionHandlerPlatform::AddTransceiverWithKind(
    const String& kind,
    const webrtc::RtpTransceiverInit&) {
  transceivers_.push_back(std::make_unique<DummyRTCRtpTransceiverPlatform>(
      kind == "audio" ? MediaStreamSource::StreamType::kTypeAudio
                      : MediaStreamSource::StreamType::kTypeVideo,
      nullptr /*MediaStreamComponent*/));
  std::unique_ptr<DummyRTCRtpTransceiverPlatform> copy(
      new DummyRTCRtpTransceiverPlatform(*transceivers_.back()));
  return std::unique_ptr<RTCRtpTransceiverPlatform>(std::move(copy));
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
MockRTCPeerConnectionHandlerPlatform::AddTrack(
    MediaStreamComponent* component,
    const MediaStreamDescriptorVector&) {
  transceivers_.push_back(std::make_unique<DummyRTCRtpTransceiverPlatform>(
      component->GetSourceType(), component));
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

rtc::scoped_refptr<webrtc::DataChannelInterface>
MockRTCPeerConnectionHandlerPlatform::CreateDataChannel(
    const String& label,
    const webrtc::DataChannelInit&) {
  return nullptr;
}

void MockRTCPeerConnectionHandlerPlatform::Close() {}
void MockRTCPeerConnectionHandlerPlatform::CloseAndUnregister() {}

webrtc::PeerConnectionInterface*
MockRTCPeerConnectionHandlerPlatform::NativePeerConnection() {
  return native_peer_connection_.get();
}

void MockRTCPeerConnectionHandlerPlatform::
    RunSynchronousOnceClosureOnSignalingThread(base::OnceClosure closure,
                                               const char* trace_event_name) {}

void MockRTCPeerConnectionHandlerPlatform::TrackIceConnectionStateChange(
    webrtc::PeerConnectionInterface::IceConnectionState state) {}

}  // namespace blink
