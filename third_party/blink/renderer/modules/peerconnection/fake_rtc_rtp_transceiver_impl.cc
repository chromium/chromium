// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/modules/peerconnection/fake_rtc_rtp_transceiver_impl.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_dtmf_sender_handler.h"

namespace blink {

blink::WebMediaStreamTrack CreateWebMediaStreamTrack(
    const std::string& id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  blink::WebMediaStreamSource web_source;
  web_source.Initialize(blink::WebString::FromUTF8(id),
                        blink::WebMediaStreamSource::kTypeAudio,
                        blink::WebString::FromUTF8("audio_track"), false);
  std::unique_ptr<blink::MediaStreamAudioSource> audio_source_ptr =
      std::make_unique<blink::MediaStreamAudioSource>(
          std::move(task_runner), true /* is_local_source */);
  blink::MediaStreamAudioSource* audio_source = audio_source_ptr.get();
  // Takes ownership of |audio_source_ptr|.
  web_source.SetPlatformSource(std::move(audio_source_ptr));

  blink::WebMediaStreamTrack web_track;
  web_track.Initialize(web_source.Id(), web_source);
  audio_source->ConnectToTrack(web_track);
  return web_track;
}

FakeRTCRtpSenderImpl::FakeRTCRtpSenderImpl(
    base::Optional<std::string> track_id,
    std::vector<std::string> stream_ids,
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

blink::WebMediaStreamTrack FakeRTCRtpSenderImpl::Track() const {
  return track_id_ ? CreateWebMediaStreamTrack(*track_id_, task_runner_)
                   : blink::WebMediaStreamTrack();  // null
}

blink::WebVector<blink::WebString> FakeRTCRtpSenderImpl::StreamIds() const {
  blink::WebVector<blink::WebString> web_stream_ids(stream_ids_.size());
  for (size_t i = 0; i < stream_ids_.size(); ++i) {
    web_stream_ids[i] = blink::WebString::FromUTF8(stream_ids_[i]);
  }
  return web_stream_ids;
}

void FakeRTCRtpSenderImpl::ReplaceTrack(blink::WebMediaStreamTrack with_track,
                                        blink::RTCVoidRequest* request) {
  NOTIMPLEMENTED();
}

std::unique_ptr<blink::RtcDtmfSenderHandler>
FakeRTCRtpSenderImpl::GetDtmfSender() const {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<webrtc::RtpParameters> FakeRTCRtpSenderImpl::GetParameters()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeRTCRtpSenderImpl::SetParameters(
    blink::WebVector<webrtc::RtpEncodingParameters>,
    webrtc::DegradationPreference,
    blink::RTCVoidRequest*) {
  NOTIMPLEMENTED();
}

void FakeRTCRtpSenderImpl::GetStats(
    blink::WebRTCStatsReportCallback,
    const blink::WebVector<webrtc::NonStandardGroupId>&) {
  NOTIMPLEMENTED();
}

void FakeRTCRtpSenderImpl::SetStreams(
    const blink::WebVector<blink::WebString>& stream_ids) {
  NOTIMPLEMENTED();
}

FakeRTCRtpReceiverImpl::FakeRTCRtpReceiverImpl(
    const std::string& track_id,
    std::vector<std::string> stream_ids,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : track_(CreateWebMediaStreamTrack(track_id, task_runner)),
      stream_ids_(std::move(stream_ids)) {}

FakeRTCRtpReceiverImpl::FakeRTCRtpReceiverImpl(const FakeRTCRtpReceiverImpl&) =
    default;

FakeRTCRtpReceiverImpl::~FakeRTCRtpReceiverImpl() {}

FakeRTCRtpReceiverImpl& FakeRTCRtpReceiverImpl::operator=(
    const FakeRTCRtpReceiverImpl&) = default;

std::unique_ptr<blink::WebRTCRtpReceiver> FakeRTCRtpReceiverImpl::ShallowCopy()
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

const blink::WebMediaStreamTrack& FakeRTCRtpReceiverImpl::Track() const {
  return track_;
}

blink::WebVector<blink::WebString> FakeRTCRtpReceiverImpl::StreamIds() const {
  blink::WebVector<blink::WebString> web_stream_ids(stream_ids_.size());
  for (size_t i = 0; i < stream_ids_.size(); ++i) {
    web_stream_ids[i] = blink::WebString::FromUTF8(stream_ids_[i]);
  }
  return web_stream_ids;
}

blink::WebVector<std::unique_ptr<blink::WebRTCRtpSource>>
FakeRTCRtpReceiverImpl::GetSources() {
  NOTIMPLEMENTED();
  return {};
}

void FakeRTCRtpReceiverImpl::GetStats(
    blink::WebRTCStatsReportCallback,
    const blink::WebVector<webrtc::NonStandardGroupId>&) {
  NOTIMPLEMENTED();
}

std::unique_ptr<webrtc::RtpParameters> FakeRTCRtpReceiverImpl::GetParameters()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeRTCRtpReceiverImpl::SetJitterBufferMinimumDelay(
    base::Optional<double> delay_seconds) {
  NOTIMPLEMENTED();
}

FakeRTCRtpTransceiverImpl::FakeRTCRtpTransceiverImpl(
    base::Optional<std::string> mid,
    FakeRTCRtpSenderImpl sender,
    FakeRTCRtpReceiverImpl receiver,
    bool stopped,
    webrtc::RtpTransceiverDirection direction,
    base::Optional<webrtc::RtpTransceiverDirection> current_direction)
    : mid_(std::move(mid)),
      sender_(std::move(sender)),
      receiver_(std::move(receiver)),
      stopped_(stopped),
      direction_(std::move(direction)),
      current_direction_(std::move(current_direction)) {}

FakeRTCRtpTransceiverImpl::~FakeRTCRtpTransceiverImpl() {}

blink::WebRTCRtpTransceiverImplementationType
FakeRTCRtpTransceiverImpl::ImplementationType() const {
  return blink::WebRTCRtpTransceiverImplementationType::kFullTransceiver;
}

uintptr_t FakeRTCRtpTransceiverImpl::Id() const {
  NOTIMPLEMENTED();
  return 0u;
}

blink::WebString FakeRTCRtpTransceiverImpl::Mid() const {
  return mid_ ? blink::WebString::FromUTF8(*mid_) : blink::WebString();
}

std::unique_ptr<blink::RTCRtpSenderPlatform> FakeRTCRtpTransceiverImpl::Sender()
    const {
  return sender_.ShallowCopy();
}

std::unique_ptr<blink::WebRTCRtpReceiver> FakeRTCRtpTransceiverImpl::Receiver()
    const {
  return receiver_.ShallowCopy();
}

bool FakeRTCRtpTransceiverImpl::Stopped() const {
  return stopped_;
}

webrtc::RtpTransceiverDirection FakeRTCRtpTransceiverImpl::Direction() const {
  return direction_;
}

void FakeRTCRtpTransceiverImpl::SetDirection(
    webrtc::RtpTransceiverDirection direction) {
  NOTIMPLEMENTED();
}

base::Optional<webrtc::RtpTransceiverDirection>
FakeRTCRtpTransceiverImpl::CurrentDirection() const {
  return current_direction_;
}

base::Optional<webrtc::RtpTransceiverDirection>
FakeRTCRtpTransceiverImpl::FiredDirection() const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

}  // namespace blink
