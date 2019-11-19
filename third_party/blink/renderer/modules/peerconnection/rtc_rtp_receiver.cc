// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"

#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_rtc_rtp_source.h"
#include "third_party/blink/public/web/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtls_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_capabilities.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"
#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/rtp_parameters.h"

namespace blink {

RTCRtpReceiver::RTCRtpReceiver(RTCPeerConnection* pc,
                               std::unique_ptr<WebRTCRtpReceiver> receiver,
                               MediaStreamTrack* track,
                               MediaStreamVector streams)
    : pc_(pc),
      receiver_(std::move(receiver)),
      track_(track),
      streams_(std::move(streams)) {
  DCHECK(receiver_);
  DCHECK(track_);
}

MediaStreamTrack* RTCRtpReceiver::track() const {
  return track_;
}

RTCDtlsTransport* RTCRtpReceiver::transport() {
  return transport_;
}

RTCDtlsTransport* RTCRtpReceiver::rtcpTransport() {
  // Chrome does not support turning off RTCP-mux.
  return nullptr;
}

double RTCRtpReceiver::playoutDelayHint(bool& is_null, ExceptionState&) {
  is_null = !playout_delay_hint_.has_value();
  return playout_delay_hint_.value_or(0.0);
}

void RTCRtpReceiver::setPlayoutDelayHint(double value,
                                         bool is_null,
                                         ExceptionState& exception_state) {
  base::Optional<double> hint =
      is_null ? base::nullopt : base::Optional<double>(value);
  if (hint && *hint < 0.0) {
    exception_state.ThrowTypeError("playoutDelayHint can't be negative");
    return;
  }

  playout_delay_hint_ = hint;
  receiver_->SetJitterBufferMinimumDelay(playout_delay_hint_);
}

HeapVector<Member<RTCRtpSynchronizationSource>>
RTCRtpReceiver::getSynchronizationSources() {
  UpdateSourcesIfNeeded();

  Document* document = To<Document>(pc_->GetExecutionContext());
  DocumentLoadTiming& time_converter = document->Loader()->GetTiming();

  HeapVector<Member<RTCRtpSynchronizationSource>> synchronization_sources;
  for (const auto& web_source : web_sources_) {
    if (web_source->SourceType() != WebRTCRtpSource::Type::kSSRC)
      continue;
    RTCRtpSynchronizationSource* synchronization_source =
        MakeGarbageCollected<RTCRtpSynchronizationSource>();
    synchronization_source->setTimestamp(
        time_converter
            .MonotonicTimeToPseudoWallTime(
                pc_->WebRtcTimestampToBlinkTimestamp(web_source->Timestamp()))
            .InMilliseconds());
    synchronization_source->setSource(web_source->Source());
    if (web_source->AudioLevel())
      synchronization_source->setAudioLevel(*web_source->AudioLevel());
    synchronization_source->setRtpTimestamp(web_source->RtpTimestamp());
    synchronization_sources.push_back(synchronization_source);
  }
  return synchronization_sources;
}

HeapVector<Member<RTCRtpContributingSource>>
RTCRtpReceiver::getContributingSources() {
  UpdateSourcesIfNeeded();

  Document* document = To<Document>(pc_->GetExecutionContext());
  DocumentLoadTiming& time_converter = document->Loader()->GetTiming();

  HeapVector<Member<RTCRtpContributingSource>> contributing_sources;
  for (const auto& web_source : web_sources_) {
    if (web_source->SourceType() != WebRTCRtpSource::Type::kCSRC)
      continue;
    RTCRtpContributingSource* contributing_source =
        MakeGarbageCollected<RTCRtpContributingSource>();
    contributing_source->setTimestamp(
        time_converter
            .MonotonicTimeToPseudoWallTime(
                pc_->WebRtcTimestampToBlinkTimestamp(web_source->Timestamp()))
            .InMilliseconds());
    contributing_source->setSource(web_source->Source());
    if (web_source->AudioLevel())
      contributing_source->setAudioLevel(*web_source->AudioLevel());
    contributing_source->setRtpTimestamp(web_source->RtpTimestamp());
    contributing_sources.push_back(contributing_source);
  }
  return contributing_sources;
}

ScriptPromise RTCRtpReceiver::getStats(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  receiver_->GetStats(
      WTF::Bind(WebRTCStatsReportCallbackResolver, WrapPersistent(resolver)),
      GetExposedGroupIds(script_state));
  return promise;
}

WebRTCRtpReceiver* RTCRtpReceiver::web_receiver() {
  return receiver_.get();
}

MediaStreamVector RTCRtpReceiver::streams() const {
  return streams_;
}

void RTCRtpReceiver::set_streams(MediaStreamVector streams) {
  streams_ = std::move(streams);
}

void RTCRtpReceiver::set_transceiver(RTCRtpTransceiver* transceiver) {
  transceiver_ = transceiver;
}

void RTCRtpReceiver::set_transport(RTCDtlsTransport* transport) {
  transport_ = transport;
}

void RTCRtpReceiver::UpdateSourcesIfNeeded() {
  if (!web_sources_needs_updating_)
    return;
  web_sources_ = receiver_->GetSources();
  // Clear the flag and schedule a microtask to reset it to true. This makes
  // the cache valid until the next microtask checkpoint. As such, sources
  // represent a snapshot and can be compared reliably in .js code, no risk of
  // being updated due to an RTP packet arriving. E.g.
  // "source.timestamp == source.timestamp" will always be true.
  web_sources_needs_updating_ = false;
  Microtask::EnqueueMicrotask(
      WTF::Bind(&RTCRtpReceiver::SetContributingSourcesNeedsUpdating,
                WrapWeakPersistent(this)));
}

void RTCRtpReceiver::SetContributingSourcesNeedsUpdating() {
  web_sources_needs_updating_ = true;
}

void RTCRtpReceiver::Trace(blink::Visitor* visitor) {
  visitor->Trace(pc_);
  visitor->Trace(track_);
  visitor->Trace(transport_);
  visitor->Trace(streams_);
  visitor->Trace(transceiver_);
  ScriptWrappable::Trace(visitor);
}

RTCRtpCapabilities* RTCRtpReceiver::getCapabilities(const String& kind) {
  if (kind != "audio" && kind != "video")
    return nullptr;

  RTCRtpCapabilities* capabilities = RTCRtpCapabilities::Create();
  capabilities->setCodecs(HeapVector<Member<RTCRtpCodecCapability>>());
  capabilities->setHeaderExtensions(
      HeapVector<Member<RTCRtpHeaderExtensionCapability>>());

  std::unique_ptr<webrtc::RtpCapabilities> rtc_capabilities =
      PeerConnectionDependencyFactory::GetInstance()->GetSenderCapabilities(
          kind.Utf8());

  HeapVector<Member<RTCRtpCodecCapability>> codecs;
  codecs.ReserveInitialCapacity(
      SafeCast<wtf_size_t>(rtc_capabilities->codecs.size()));
  for (const auto& rtc_codec : rtc_capabilities->codecs) {
    auto* codec = RTCRtpCodecCapability::Create();
    codec->setMimeType(WTF::String::FromUTF8(rtc_codec.mime_type()));
    if (rtc_codec.clock_rate)
      codec->setClockRate(rtc_codec.clock_rate.value());
    if (rtc_codec.num_channels)
      codec->setChannels(rtc_codec.num_channels.value());
    if (!rtc_codec.parameters.empty()) {
      std::string sdp_fmtp_line;
      for (const auto& parameter : rtc_codec.parameters) {
        if (!sdp_fmtp_line.empty())
          sdp_fmtp_line += ";";
        sdp_fmtp_line += parameter.first + "=" + parameter.second;
      }
      codec->setSdpFmtpLine(sdp_fmtp_line.c_str());
    }
    codecs.push_back(codec);
  }
  capabilities->setCodecs(codecs);

  HeapVector<Member<RTCRtpHeaderExtensionCapability>> header_extensions;
  header_extensions.ReserveInitialCapacity(
      SafeCast<wtf_size_t>(rtc_capabilities->header_extensions.size()));
  for (const auto& rtc_header_extension : rtc_capabilities->header_extensions) {
    auto* header_extension = RTCRtpHeaderExtensionCapability::Create();
    header_extension->setUri(WTF::String::FromUTF8(rtc_header_extension.uri));
    header_extensions.push_back(header_extension);
  }
  capabilities->setHeaderExtensions(header_extensions);

  return capabilities;
}

RTCRtpReceiveParameters* RTCRtpReceiver::getParameters() {
  RTCRtpReceiveParameters* parameters = RTCRtpReceiveParameters::Create();
  std::unique_ptr<webrtc::RtpParameters> webrtc_parameters =
      receiver_->GetParameters();

  RTCRtcpParameters* rtcp = RTCRtcpParameters::Create();
  rtcp->setReducedSize(webrtc_parameters->rtcp.reduced_size);
  parameters->setRtcp(rtcp);

  HeapVector<Member<RTCRtpDecodingParameters>> encodings;
  encodings.ReserveCapacity(
      SafeCast<wtf_size_t>(webrtc_parameters->encodings.size()));
  for (const auto& webrtc_encoding : webrtc_parameters->encodings) {
    RTCRtpDecodingParameters* encoding = RTCRtpDecodingParameters::Create();
    if (!webrtc_encoding.rid.empty()) {
      // TODO(orphis): Add rid when supported by WebRTC
    }
    encodings.push_back(encoding);
  }
  parameters->setEncodings(encodings);

  HeapVector<Member<RTCRtpHeaderExtensionParameters>> headers;
  headers.ReserveCapacity(
      SafeCast<wtf_size_t>(webrtc_parameters->header_extensions.size()));
  for (const auto& webrtc_header : webrtc_parameters->header_extensions) {
    headers.push_back(ToRtpHeaderExtensionParameters(webrtc_header));
  }
  parameters->setHeaderExtensions(headers);

  HeapVector<Member<RTCRtpCodecParameters>> codecs;
  codecs.ReserveCapacity(
      SafeCast<wtf_size_t>(webrtc_parameters->codecs.size()));
  for (const auto& webrtc_codec : webrtc_parameters->codecs) {
    codecs.push_back(ToRtpCodecParameters(webrtc_codec));
  }
  parameters->setCodecs(codecs);

  return parameters;
}

}  // namespace blink
