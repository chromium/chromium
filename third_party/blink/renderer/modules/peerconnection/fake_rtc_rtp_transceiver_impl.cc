// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/peerconnection/fake_rtc_rtp_transceiver_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_dtmf_sender_handler.h"

namespace blink {

MediaStreamComponent* CreateMediaStreamComponent(
    const String& id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto audio_source = std::make_unique<blink::MediaStreamAudioSource>(
      std::move(task_runner), true /* is_local_source */);
  auto* audio_source_ptr = audio_source.get();
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      id, MediaStreamSource::kTypeAudio, "audio_track", false,
      std::move(audio_source));

  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
      source->Id(), source,
      std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */));
  audio_source_ptr->ConnectToInitializedTrack(component);
  return component;
}

FakeRTCRtpSenderImpl::FakeRTCRtpSenderImpl(
    std::optional<String> track_id,
    Vector<String> stream_ids,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : track_id_(std::move(track_id)),
      stream_ids_(std::move(stream_ids)),
      task_runner_(task_runner) {}

FakeRTCRtpSenderImpl::FakeRTCRtpSenderImpl(const FakeRTCRtpSenderImpl&) =
    default;

FakeRTCRtpSenderImpl::~FakeRTCRtpSenderImpl() {}

FakeRTCRtpSenderImpl& FakeRTCRtpSenderImpl::operator=(
    const FakeRTCRtpSenderImpl&) = default;

std::unique_ptr<blink::RTCRtpSenderPlatform> FakeRTCRtpSenderImpl::ShallowCopy()
    const {
  return std::make_unique<FakeRTCRtpSenderImpl>(*this);
}

uintptr_t FakeRTCRtpSenderImpl::Id() const {
  NOTIMPLEMENTED();
  return 0;
}

rtc::scoped_refptr<webrtc::DtlsTransportInterface>
FakeRTCRtpSenderImpl::DtlsTransport() {
  NOTIMPLEMENTED();
  return nullptr;
}

webrtc::DtlsTransportInformation
FakeRTCRtpSenderImpl::DtlsTransportInformation() {
  NOTIMPLEMENTED();
  static webrtc::DtlsTransportInformation dummy(
      webrtc::DtlsTransportState::kNew);
  return dummy;
}

MediaStreamComponent* FakeRTCRtpSenderImpl::Track() const {
  return track_id_ ? CreateMediaStreamComponent(*track_id_, task_runner_)
                   : nullptr;
}

Vector<String> FakeRTCRtpSenderImpl::StreamIds() const {
  return stream_ids_;
}

void FakeRTCRtpSenderImpl::ReplaceTrack(MediaStreamComponent* with_track,
                                        RTCVoidRequest* request) {
  NOTIMPLEMENTED();
}

std::unique_ptr<blink::RtcDtmfSenderHandler>
FakeRTCRtpSenderImpl::GetDtmfSender() const {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<webrtc::RtpParameters> FakeRTCRtpSenderImpl::GetParameters()
    const {
  return std::make_unique<webrtc::RtpParameters>();
}

void FakeRTCRtpSenderImpl::SetParameters(
    Vector<webrtc::RtpEncodingParameters>,
    std::optional<webrtc::DegradationPreference>,
    blink::RTCVoidRequest*) {
  NOTIMPLEMENTED();
}

void FakeRTCRtpSenderImpl::GetStats(RTCStatsReportCallback) {
  NOTIMPLEMENTED();
}

void FakeRTCRtpSenderImpl::SetStreams(const Vector<String>& stream_ids) {
  NOTIMPLEMENTED();
}

FakeRTCRtpReceiverImpl::FakeRTCRtpReceiverImpl(
    const String& track_id,
    Vector<String> stream_ids,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : component_(CreateMediaStreamComponent(track_id, task_runner)),
      stream_ids_(std::move(stream_ids)) {}

FakeRTCRtpReceiverImpl::FakeRTCRtpReceiverImpl(const FakeRTCRtpReceiverImpl&) =
    default;

FakeRTCRtpReceiverImpl::~FakeRTCRtpReceiverImpl() {}

FakeRTCRtpReceiverImpl& FakeRTCRtpReceiverImpl::operator=(
    const FakeRTCRtpReceiverImpl&) = default;

std::unique_ptr<RTCRtpReceiverPlatform> FakeRTCRtpReceiverImpl::ShallowCopy()
    const {
  return std::make_unique<FakeRTCRtpReceiverImpl>(*this);
}

uintptr_t FakeRTCRtpReceiverImpl::Id() const {
  NOTIMPLEMENTED();
  return 0;
}

rtc::scoped_refptr<webrtc::DtlsTransportInterface>
FakeRTCRtpReceiverImpl::DtlsTransport() {
  NOTIMPLEMENTED();
  return nullptr;
}

webrtc::DtlsTransportInformation
FakeRTCRtpReceiverImpl::DtlsTransportInformation() {
  NOTIMPLEMENTED();
  static webrtc::DtlsTransportInformation dummy(
      webrtc::DtlsTransportState::kNew);
  return dummy;
}

MediaStreamComponent* FakeRTCRtpReceiverImpl::Track() const {
  return component_;
}

Vector<String> FakeRTCRtpReceiverImpl::StreamIds() const {
  return stream_ids_;
}

Vector<std::unique_ptr<RTCRtpSource>> FakeRTCRtpReceiverImpl::GetSources() {
  NOTIMPLEMENTED();
  return {};
}

void FakeRTCRtpReceiverImpl::GetStats(RTCStatsReportCallback) {
  NOTIMPLEMENTED();
}

std::unique_ptr<webrtc::RtpParameters> FakeRTCRtpReceiverImpl::GetParameters()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeRTCRtpReceiverImpl::SetJitterBufferMinimumDelay(
    std::optional<double> delay_seconds) {
  NOTIMPLEMENTED();
}

FakeRTCRtpTransceiverImpl::FakeRTCRtpTransceiverImpl(
    const String& mid,
    FakeRTCRtpSenderImpl sender,
    FakeRTCRtpReceiverImpl receiver,
    webrtc::RtpTransceiverDirection direction,
    std::optional<webrtc::RtpTransceiverDirection> current_direction)
    : mid_(mid),
      sender_(std::move(sender)),
      receiver_(std::move(receiver)),
      direction_(std::move(direction)),
      current_direction_(std::move(current_direction)) {}

FakeRTCRtpTransceiverImpl::~FakeRTCRtpTransceiverImpl() {}

uintptr_t FakeRTCRtpTransceiverImpl::Id() const {
  NOTIMPLEMENTED();
  return 0u;
}

String FakeRTCRtpTransceiverImpl::Mid() const {
  return mid_;
}

std::unique_ptr<blink::RTCRtpSenderPlatform> FakeRTCRtpTransceiverImpl::Sender()
    const {
  return sender_.ShallowCopy();
}

std::unique_ptr<RTCRtpReceiverPlatform> FakeRTCRtpTransceiverImpl::Receiver()
    const {
  return receiver_.ShallowCopy();
}

webrtc::RtpTransceiverDirection FakeRTCRtpTransceiverImpl::Direction() const {
  return direction_;
}

webrtc::RTCError FakeRTCRtpTransceiverImpl::SetDirection(
    webrtc::RtpTransceiverDirection direction) {
  NOTIMPLEMENTED();
  return webrtc::RTCError::OK();
}

std::optional<webrtc::RtpTransceiverDirection>
FakeRTCRtpTransceiverImpl::CurrentDirection() const {
  return current_direction_;
}

std::optional<webrtc::RtpTransceiverDirection>
FakeRTCRtpTransceiverImpl::FiredDirection() const {
  NOTIMPLEMENTED();
  return std::nullopt;
}

webrtc::RTCError FakeRTCRtpTransceiverImpl::Stop() {
  return webrtc::RTCError::OK();
}

webrtc::RTCError FakeRTCRtpTransceiverImpl::SetCodecPreferences(
    Vector<webrtc::RtpCodecCapability>) {
  return webrtc::RTCError::OK();
}

webrtc::RTCError FakeRTCRtpTransceiverImpl::SetHeaderExtensionsToNegotiate(
    Vector<webrtc::RtpHeaderExtensionCapability> header_extensions) {
  return webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION);
}

Vector<webrtc::RtpHeaderExtensionCapability>
FakeRTCRtpTransceiverImpl::GetNegotiatedHeaderExtensions() const {
  return {};
}

Vector<webrtc::RtpHeaderExtensionCapability>
FakeRTCRtpTransceiverImpl::GetHeaderExtensionsToNegotiate() const {
  return {};
}

}  // namespace blink
