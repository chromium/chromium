// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"

#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_rtc_rtp_contributing_source.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_capabilities.h"
#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/rtpparameters.h"

namespace blink {

RTCRtpReceiver::RTCRtpReceiver(std::unique_ptr<WebRTCRtpReceiver> receiver,
                               MediaStreamTrack* track,
                               MediaStreamVector streams)
    : receiver_(std::move(receiver)),
      track_(track),
      streams_(std::move(streams)) {
  DCHECK(receiver_);
  DCHECK(track_);
}

MediaStreamTrack* RTCRtpReceiver::track() const {
  return track_;
}

const HeapVector<Member<RTCRtpContributingSource>>&
RTCRtpReceiver::getContributingSources() {
  UpdateSourcesIfNeeded();
  return contributing_sources_;
}

ScriptPromise RTCRtpReceiver::getStats(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  receiver_->GetStats(WebRTCStatsReportCallbackResolver::Create(resolver));
  return promise;
}

const WebRTCRtpReceiver& RTCRtpReceiver::web_receiver() const {
  return *receiver_;
}

MediaStreamVector RTCRtpReceiver::streams() const {
  return streams_;
}

void RTCRtpReceiver::set_streams(MediaStreamVector streams) {
  streams_ = std::move(streams);
}

void RTCRtpReceiver::UpdateSourcesIfNeeded() {
  if (!contributing_sources_needs_updating_)
    return;
  contributing_sources_.clear();
  for (const std::unique_ptr<WebRTCRtpContributingSource>&
           web_contributing_source : receiver_->GetSources()) {
    if (web_contributing_source->SourceType() ==
        WebRTCRtpContributingSourceType::SSRC) {
      // TODO(hbos): When |getSynchronizationSources| is added to get SSRC
      // sources don't ignore SSRCs here.
      continue;
    }
    DCHECK_EQ(web_contributing_source->SourceType(),
              WebRTCRtpContributingSourceType::CSRC);
    RTCRtpContributingSource* contributing_source =
        new RTCRtpContributingSource(this, *web_contributing_source);
    contributing_sources_.push_back(contributing_source);
  }
  // Clear the flag and schedule a microtask to reset it to true. This makes
  // the cache valid until the next microtask checkpoint. As such, sources
  // represent a snapshot and can be compared reliably in .js code, no risk of
  // being updated due to an RTP packet arriving. E.g.
  // "source.timestamp == source.timestamp" will always be true.
  contributing_sources_needs_updating_ = false;
  Microtask::EnqueueMicrotask(
      WTF::Bind(&RTCRtpReceiver::SetContributingSourcesNeedsUpdating,
                WrapWeakPersistent(this)));
}

void RTCRtpReceiver::SetContributingSourcesNeedsUpdating() {
  contributing_sources_needs_updating_ = true;
}

void RTCRtpReceiver::Trace(blink::Visitor* visitor) {
  visitor->Trace(track_);
  visitor->Trace(streams_);
  visitor->Trace(contributing_sources_);
  ScriptWrappable::Trace(visitor);
}

void RTCRtpReceiver::getCapabilities(
    const String& kind,
    base::Optional<RTCRtpCapabilities>& capabilities) {
  if (kind != "audio" && kind != "video")
    return;

  capabilities = RTCRtpCapabilities{};

  std::unique_ptr<webrtc::RtpCapabilities> rtc_capabilities =
      blink::Platform::Current()->GetRtpSenderCapabilities(kind);

  HeapVector<RTCRtpCodecCapability> codecs;
  codecs.ReserveInitialCapacity(
      SafeCast<wtf_size_t>(rtc_capabilities->codecs.size()));
  for (const auto& rtc_codec : rtc_capabilities->codecs) {
    codecs.emplace_back();
    auto& codec = codecs.back();
    codec.setMimeType(WTF::String::FromUTF8(rtc_codec.mime_type().c_str()));
    if (rtc_codec.clock_rate)
      codec.setClockRate(rtc_codec.clock_rate.value());
    if (rtc_codec.num_channels)
      codec.setChannels(rtc_codec.num_channels.value());
    if (rtc_codec.parameters.size()) {
      std::string sdp_fmtp_line;
      for (const auto& parameter : rtc_codec.parameters) {
        if (sdp_fmtp_line.size())
          sdp_fmtp_line += ";";
        sdp_fmtp_line += parameter.first + "=" + parameter.second;
      }
      codec.setSdpFmtpLine(sdp_fmtp_line.c_str());
    }
  }
  capabilities->setCodecs(codecs);

  HeapVector<RTCRtpHeaderExtensionCapability> header_extensions;
  header_extensions.ReserveInitialCapacity(
      SafeCast<wtf_size_t>(rtc_capabilities->header_extensions.size()));
  for (const auto& rtc_header_extension : rtc_capabilities->header_extensions) {
    header_extensions.emplace_back();
    auto& header_extension = header_extensions.back();
    header_extension.setUri(
        WTF::String::FromUTF8(rtc_header_extension.uri.c_str()));
  }
  capabilities->setHeaderExtensions(header_extensions);
}

}  // namespace blink
