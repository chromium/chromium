// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_audio_playout_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_audio_source_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_certificate_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_codec_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_data_channel_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_candidate_pair_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_candidate_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_inbound_rtp_stream_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_media_source_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_media_stream_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_media_stream_track_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_outbound_rtp_stream_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_peer_connection_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_received_rtp_stream_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_remote_inbound_rtp_stream_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_remote_outbound_rtp_stream_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_sent_rtp_stream_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_transport_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_video_source_stats.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"
#include "v8-local-handle.h"
#include "v8-object.h"

namespace blink {

namespace {

template <typename T>
v8::Local<v8::Value> HashMapToValue(ScriptState* script_state,
                                    HashMap<String, T>&& map) {
  V8ObjectBuilder builder(script_state);
  for (auto& it : map) {
    builder.Add(it.key, it.value);
  }
  v8::Local<v8::Object> v8_object = builder.V8Value();
  if (v8_object.IsEmpty()) {
    NOTREACHED();
    return v8::Undefined(script_state->GetIsolate());
  }
  return v8_object;
}

bool IsCapturing(LocalDOMWindow* window) {
  UserMediaClient* user_media_client = UserMediaClient::From(window);
  return user_media_client && user_media_client->IsCapturing();
}

bool ExposeHardwareCapabilityStats(ScriptState* script_state) {
  // According the the spec description at
  // https://w3c.github.io/webrtc-stats/#dfn-exposing-hardware-is-allowed,
  // hardware capabilities may be exposed if the context capturing state is
  // true.
  ExecutionContext* ctx = ExecutionContext::From(script_state);
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(ctx);
  return window && IsCapturing(window);
}

v8::Local<v8::Object> RTCStatsToV8Object(ScriptState* script_state,
                                         const RTCStatsWrapper* stats) {
  V8ObjectBuilder builder(script_state);

  builder.AddString("id", stats->Id());
  builder.AddNumber("timestamp", stats->TimestampMs());
  builder.AddString("type", stats->GetType());

  const bool expose_hardware_caps = ExposeHardwareCapabilityStats(script_state);

  for (size_t i = 0; i < stats->MembersCount(); ++i) {
    std::unique_ptr<RTCStatsMember> member = stats->GetMember(i);
    if (!member->IsDefined() ||
        (!expose_hardware_caps &&
         member->Restriction() ==
             RTCStatsMember::ExposureRestriction::kHardwareCapability)) {
      continue;
    }
    String name = member->GetName();
    switch (member->GetType()) {
      case webrtc::RTCStatsMemberInterface::kBool:
        builder.AddBoolean(name, member->ValueBool());
        break;
      case webrtc::RTCStatsMemberInterface::kInt32:
        builder.AddNumber(name, static_cast<double>(member->ValueInt32()));
        break;
      case webrtc::RTCStatsMemberInterface::kUint32:
        builder.AddNumber(name, static_cast<double>(member->ValueUint32()));
        break;
      case webrtc::RTCStatsMemberInterface::kInt64:
        builder.AddNumber(name, static_cast<double>(member->ValueInt64()));
        break;
      case webrtc::RTCStatsMemberInterface::kUint64:
        builder.AddNumber(name, static_cast<double>(member->ValueUint64()));
        break;
      case webrtc::RTCStatsMemberInterface::kDouble:
        builder.AddNumber(name, member->ValueDouble());
        break;
      case webrtc::RTCStatsMemberInterface::kString:
        builder.AddString(name, member->ValueString());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceBool: {
        builder.Add(name, member->ValueSequenceBool());
        break;
      }
      case webrtc::RTCStatsMemberInterface::kSequenceInt32:
        builder.Add(name, member->ValueSequenceInt32());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceUint32:
        builder.Add(name, member->ValueSequenceUint32());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceInt64:
        builder.Add(name, member->ValueSequenceInt64());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceUint64:
        builder.Add(name, member->ValueSequenceUint64());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceDouble:
        builder.Add(name, member->ValueSequenceDouble());
        break;
      case webrtc::RTCStatsMemberInterface::kSequenceString:
        builder.Add(name, member->ValueSequenceString());
        break;
      case webrtc::RTCStatsMemberInterface::kMapStringUint64:
        builder.Add(
            name, HashMapToValue(script_state, member->ValueMapStringUint64()));
        break;
      case webrtc::RTCStatsMemberInterface::kMapStringDouble:
        builder.Add(
            name, HashMapToValue(script_state, member->ValueMapStringDouble()));
        break;
      default:
        NOTREACHED();
    }
  }

  v8::Local<v8::Object> v8_object = builder.V8Value();
  CHECK(!v8_object.IsEmpty());
  return v8_object;
}

RTCCodecStats* ToV8Stat(ScriptState* script_state,
                        const webrtc::RTCCodecStats& webrtc_stat) {
  RTCCodecStats* v8_codec =
      MakeGarbageCollected<RTCCodecStats>(script_state->GetIsolate());
  if (webrtc_stat.transport_id.is_defined()) {
    v8_codec->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.payload_type.is_defined()) {
    v8_codec->setPayloadType(*webrtc_stat.payload_type);
  }
  if (webrtc_stat.channels.is_defined()) {
    v8_codec->setChannels(*webrtc_stat.channels);
  }
  if (webrtc_stat.mime_type.is_defined()) {
    v8_codec->setMimeType(String::FromUTF8(*webrtc_stat.mime_type));
  }
  if (webrtc_stat.clock_rate.is_defined()) {
    v8_codec->setClockRate(*webrtc_stat.clock_rate);
  }
  if (webrtc_stat.sdp_fmtp_line.is_defined()) {
    v8_codec->setSdpFmtpLine(String::FromUTF8(*webrtc_stat.sdp_fmtp_line));
  }
  return v8_codec;
}

RTCInboundRtpStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCInboundRTPStreamStats& webrtc_stat,
    bool expose_hardware_caps,
    bool unship_deprecated_stats) {
  RTCInboundRtpStreamStats* v8_stat =
      MakeGarbageCollected<RTCInboundRtpStreamStats>(
          script_state->GetIsolate());
  // RTCRtpStreamStats
  if (webrtc_stat.ssrc.is_defined()) {
    v8_stat->setSsrc(*webrtc_stat.ssrc);
  }
  if (webrtc_stat.kind.is_defined()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.transport_id.is_defined()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.codec_id.is_defined()) {
    v8_stat->setCodecId(String::FromUTF8(*webrtc_stat.codec_id));
  }
  // RTCRtpStreamStats legacy stats
  if (webrtc_stat.media_type.is_defined()) {
    v8_stat->setMediaType(String::FromUTF8(*webrtc_stat.media_type));
  }
  if (!unship_deprecated_stats && webrtc_stat.track_id.is_defined()) {
    v8_stat->setTrackId(String::FromUTF8(*webrtc_stat.track_id));
  }
  // RTCReceivedRtpStreamStats
  if (webrtc_stat.packets_lost.is_defined()) {
    v8_stat->setPacketsLost(*webrtc_stat.packets_lost);
  }
  if (webrtc_stat.jitter.is_defined()) {
    v8_stat->setJitter(*webrtc_stat.jitter);
  }
  // RTCInboundRtpStreamStats
  if (webrtc_stat.track_identifier.is_defined()) {
    v8_stat->setTrackIdentifier(
        String::FromUTF8(*webrtc_stat.track_identifier));
  }
  if (webrtc_stat.mid.is_defined()) {
    v8_stat->setMid(String::FromUTF8(*webrtc_stat.mid));
  }
  if (webrtc_stat.remote_id.is_defined()) {
    v8_stat->setRemoteId(String::FromUTF8(*webrtc_stat.remote_id));
  }
  if (webrtc_stat.frames_decoded.is_defined()) {
    v8_stat->setFramesDecoded(*webrtc_stat.frames_decoded);
  }
  if (webrtc_stat.key_frames_decoded.is_defined()) {
    v8_stat->setKeyFramesDecoded(*webrtc_stat.key_frames_decoded);
  }
  if (webrtc_stat.frames_dropped.is_defined()) {
    v8_stat->setFramesDropped(*webrtc_stat.frames_dropped);
  }
  if (webrtc_stat.frame_width.is_defined()) {
    v8_stat->setFrameWidth(*webrtc_stat.frame_width);
  }
  if (webrtc_stat.frame_height.is_defined()) {
    v8_stat->setFrameHeight(*webrtc_stat.frame_height);
  }
  if (webrtc_stat.frames_per_second.is_defined()) {
    v8_stat->setFramesPerSecond(*webrtc_stat.frames_per_second);
  }
  if (webrtc_stat.qp_sum.is_defined()) {
    v8_stat->setQpSum(*webrtc_stat.qp_sum);
  }
  if (webrtc_stat.total_decode_time.is_defined()) {
    v8_stat->setTotalDecodeTime(*webrtc_stat.total_decode_time);
  }
  if (webrtc_stat.total_inter_frame_delay.is_defined()) {
    v8_stat->setTotalInterFrameDelay(*webrtc_stat.total_inter_frame_delay);
  }
  if (webrtc_stat.total_squared_inter_frame_delay.is_defined()) {
    v8_stat->setTotalSquaredInterFrameDelay(
        *webrtc_stat.total_squared_inter_frame_delay);
  }
  if (webrtc_stat.pause_count.is_defined()) {
    v8_stat->setPauseCount(*webrtc_stat.pause_count);
  }
  if (webrtc_stat.total_pauses_duration.is_defined()) {
    v8_stat->setTotalPausesDuration(*webrtc_stat.total_pauses_duration);
  }
  if (webrtc_stat.freeze_count.is_defined()) {
    v8_stat->setFreezeCount(*webrtc_stat.freeze_count);
  }
  if (webrtc_stat.total_freezes_duration.is_defined()) {
    v8_stat->setTotalFreezesDuration(*webrtc_stat.total_freezes_duration);
  }
  if (webrtc_stat.last_packet_received_timestamp.is_defined()) {
    v8_stat->setLastPacketReceivedTimestamp(
        *webrtc_stat.last_packet_received_timestamp);
  }
  if (webrtc_stat.header_bytes_received.is_defined()) {
    v8_stat->setHeaderBytesReceived(*webrtc_stat.header_bytes_received);
  }
  if (webrtc_stat.packets_discarded.is_defined()) {
    v8_stat->setPacketsDiscarded(*webrtc_stat.packets_discarded);
  }
  if (webrtc_stat.packets_received.is_defined()) {
    v8_stat->setPacketsReceived(*webrtc_stat.packets_received);
  }
  if (webrtc_stat.fec_packets_received.is_defined()) {
    v8_stat->setFecPacketsReceived(*webrtc_stat.fec_packets_received);
  }
  if (webrtc_stat.fec_packets_discarded.is_defined()) {
    v8_stat->setFecPacketsDiscarded(*webrtc_stat.fec_packets_discarded);
  }
  if (webrtc_stat.bytes_received.is_defined()) {
    v8_stat->setBytesReceived(*webrtc_stat.bytes_received);
  }
  if (webrtc_stat.nack_count.is_defined()) {
    v8_stat->setNackCount(*webrtc_stat.nack_count);
  }
  if (webrtc_stat.fir_count.is_defined()) {
    v8_stat->setFirCount(*webrtc_stat.fir_count);
  }
  if (webrtc_stat.pli_count.is_defined()) {
    v8_stat->setPliCount(*webrtc_stat.pli_count);
  }
  if (webrtc_stat.total_processing_delay.is_defined()) {
    v8_stat->setTotalProcessingDelay(*webrtc_stat.total_processing_delay);
  }
  if (webrtc_stat.estimated_playout_timestamp.is_defined()) {
    v8_stat->setEstimatedPlayoutTimestamp(
        *webrtc_stat.estimated_playout_timestamp);
  }
  if (webrtc_stat.jitter_buffer_delay.is_defined()) {
    v8_stat->setJitterBufferDelay(*webrtc_stat.jitter_buffer_delay);
  }
  if (webrtc_stat.jitter_buffer_target_delay.is_defined()) {
    v8_stat->setJitterBufferTargetDelay(
        *webrtc_stat.jitter_buffer_target_delay);
  }
  if (webrtc_stat.jitter_buffer_emitted_count.is_defined()) {
    v8_stat->setJitterBufferEmittedCount(
        *webrtc_stat.jitter_buffer_emitted_count);
  }
  if (webrtc_stat.jitter_buffer_minimum_delay.is_defined()) {
    v8_stat->setJitterBufferMinimumDelay(
        *webrtc_stat.jitter_buffer_minimum_delay);
  }
  if (webrtc_stat.total_samples_received.is_defined()) {
    v8_stat->setTotalSamplesReceived(*webrtc_stat.total_samples_received);
  }
  if (webrtc_stat.concealed_samples.is_defined()) {
    v8_stat->setConcealedSamples(*webrtc_stat.concealed_samples);
  }
  if (webrtc_stat.silent_concealed_samples.is_defined()) {
    v8_stat->setSilentConcealedSamples(*webrtc_stat.silent_concealed_samples);
  }
  if (webrtc_stat.concealment_events.is_defined()) {
    v8_stat->setConcealmentEvents(*webrtc_stat.concealment_events);
  }
  if (webrtc_stat.inserted_samples_for_deceleration.is_defined()) {
    v8_stat->setInsertedSamplesForDeceleration(
        *webrtc_stat.inserted_samples_for_deceleration);
  }
  if (webrtc_stat.removed_samples_for_acceleration.is_defined()) {
    v8_stat->setRemovedSamplesForAcceleration(
        *webrtc_stat.removed_samples_for_acceleration);
  }
  if (webrtc_stat.audio_level.is_defined()) {
    v8_stat->setAudioLevel(*webrtc_stat.audio_level);
  }
  if (webrtc_stat.total_audio_energy.is_defined()) {
    v8_stat->setTotalAudioEnergy(*webrtc_stat.total_audio_energy);
  }
  if (webrtc_stat.total_samples_duration.is_defined()) {
    v8_stat->setTotalSamplesDuration(*webrtc_stat.total_samples_duration);
  }
  if (webrtc_stat.frames_received.is_defined()) {
    v8_stat->setFramesReceived(*webrtc_stat.frames_received);
  }
  if (webrtc_stat.playout_id.is_defined()) {
    v8_stat->setPlayoutId(String::FromUTF8(*webrtc_stat.playout_id));
  }
  if (webrtc_stat.frames_assembled_from_multiple_packets.is_defined()) {
    v8_stat->setFramesAssembledFromMultiplePackets(
        *webrtc_stat.frames_assembled_from_multiple_packets);
  }
  if (webrtc_stat.total_assembly_time.is_defined()) {
    v8_stat->setTotalAssemblyTime(*webrtc_stat.total_assembly_time);
  }
  if (expose_hardware_caps) {
    if (webrtc_stat.power_efficient_decoder.is_defined()) {
      v8_stat->setPowerEfficientDecoder(*webrtc_stat.power_efficient_decoder);
    }
    if (webrtc_stat.decoder_implementation.is_defined()) {
      v8_stat->setDecoderImplementation(
          String::FromUTF8(*webrtc_stat.decoder_implementation));
    }
  }
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcinboundrtpstreamstats-contenttype
  if (webrtc_stat.content_type.is_defined()) {
    v8_stat->setContentType(String::FromUTF8(*webrtc_stat.content_type));
  }
  // https://github.com/w3c/webrtc-provisional-stats/issues/40
  if (webrtc_stat.goog_timing_frame_info.is_defined()) {
    v8_stat->setGoogTimingFrameInfo(
        String::FromUTF8(*webrtc_stat.goog_timing_frame_info));
  }
  return v8_stat;
}

RTCRemoteInboundRtpStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCRemoteInboundRtpStreamStats& webrtc_stat,
    bool unship_deprecated_stats) {
  RTCRemoteInboundRtpStreamStats* v8_stat =
      MakeGarbageCollected<RTCRemoteInboundRtpStreamStats>(
          script_state->GetIsolate());
  // RTCRtpStreamStats
  if (webrtc_stat.ssrc.is_defined()) {
    v8_stat->setSsrc(*webrtc_stat.ssrc);
  }
  if (webrtc_stat.kind.is_defined()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.transport_id.is_defined()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.codec_id.is_defined()) {
    v8_stat->setCodecId(String::FromUTF8(*webrtc_stat.codec_id));
  }
  // RTCRtpStreamStats legacy stats
  if (webrtc_stat.media_type.is_defined()) {
    v8_stat->setMediaType(String::FromUTF8(*webrtc_stat.media_type));
  }
  if (!unship_deprecated_stats && webrtc_stat.track_id.is_defined()) {
    v8_stat->setTrackId(String::FromUTF8(*webrtc_stat.track_id));
  }
  // RTCReceivedRtpStreamStats
  if (webrtc_stat.packets_lost.is_defined()) {
    v8_stat->setPacketsLost(*webrtc_stat.packets_lost);
  }
  if (webrtc_stat.jitter.is_defined()) {
    v8_stat->setJitter(*webrtc_stat.jitter);
  }
  // RTCRemoteInboundRtpStreamStats
  if (webrtc_stat.local_id.is_defined()) {
    v8_stat->setLocalId(String::FromUTF8(*webrtc_stat.local_id));
  }
  if (webrtc_stat.round_trip_time.is_defined()) {
    v8_stat->setRoundTripTime(*webrtc_stat.round_trip_time);
  }
  if (webrtc_stat.total_round_trip_time.is_defined()) {
    v8_stat->setTotalRoundTripTime(*webrtc_stat.total_round_trip_time);
  }
  if (webrtc_stat.fraction_lost.is_defined()) {
    v8_stat->setFractionLost(*webrtc_stat.fraction_lost);
  }
  if (webrtc_stat.round_trip_time_measurements.is_defined()) {
    v8_stat->setRoundTripTimeMeasurements(
        *webrtc_stat.round_trip_time_measurements);
  }
  return v8_stat;
}

RTCOutboundRtpStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCOutboundRTPStreamStats& webrtc_stat,
    bool expose_hardware_caps,
    bool unship_deprecated_stats) {
  RTCOutboundRtpStreamStats* v8_stat =
      MakeGarbageCollected<RTCOutboundRtpStreamStats>(
          script_state->GetIsolate());
  // RTCRtpStreamStats
  if (webrtc_stat.ssrc.is_defined()) {
    v8_stat->setSsrc(*webrtc_stat.ssrc);
  }
  if (webrtc_stat.kind.is_defined()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.transport_id.is_defined()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.codec_id.is_defined()) {
    v8_stat->setCodecId(String::FromUTF8(*webrtc_stat.codec_id));
  }
  // RTCRtpStreamStats legacy stats
  if (webrtc_stat.media_type.is_defined()) {
    v8_stat->setMediaType(String::FromUTF8(*webrtc_stat.media_type));
  }
  if (!unship_deprecated_stats && webrtc_stat.track_id.is_defined()) {
    v8_stat->setTrackId(String::FromUTF8(*webrtc_stat.track_id));
  }
  // RTCSentRtpStreamStats
  if (webrtc_stat.packets_sent.is_defined()) {
    v8_stat->setPacketsSent(*webrtc_stat.packets_sent);
  }
  if (webrtc_stat.bytes_sent.is_defined()) {
    v8_stat->setBytesSent(*webrtc_stat.bytes_sent);
  }
  // RTCOutboundRtpStreamStats
  if (webrtc_stat.mid.is_defined()) {
    v8_stat->setMid(String::FromUTF8(*webrtc_stat.mid));
  }
  if (webrtc_stat.media_source_id.is_defined()) {
    v8_stat->setMediaSourceId(String::FromUTF8(*webrtc_stat.media_source_id));
  }
  if (webrtc_stat.remote_id.is_defined()) {
    v8_stat->setRemoteId(String::FromUTF8(*webrtc_stat.remote_id));
  }
  if (webrtc_stat.rid.is_defined()) {
    v8_stat->setRid(String::FromUTF8(*webrtc_stat.rid));
  }
  if (webrtc_stat.header_bytes_sent.is_defined()) {
    v8_stat->setHeaderBytesSent(*webrtc_stat.header_bytes_sent);
  }
  if (webrtc_stat.retransmitted_packets_sent.is_defined()) {
    v8_stat->setRetransmittedPacketsSent(
        *webrtc_stat.retransmitted_packets_sent);
  }
  if (webrtc_stat.retransmitted_bytes_sent.is_defined()) {
    v8_stat->setRetransmittedBytesSent(*webrtc_stat.retransmitted_bytes_sent);
  }
  if (webrtc_stat.target_bitrate.is_defined()) {
    v8_stat->setTargetBitrate(*webrtc_stat.target_bitrate);
  }
  if (webrtc_stat.total_encoded_bytes_target.is_defined()) {
    v8_stat->setTotalEncodedBytesTarget(
        *webrtc_stat.total_encoded_bytes_target);
  }
  if (webrtc_stat.frame_width.is_defined()) {
    v8_stat->setFrameWidth(*webrtc_stat.frame_width);
  }
  if (webrtc_stat.frame_height.is_defined()) {
    v8_stat->setFrameHeight(*webrtc_stat.frame_height);
  }
  if (webrtc_stat.frames_per_second.is_defined()) {
    v8_stat->setFramesPerSecond(*webrtc_stat.frames_per_second);
  }
  if (webrtc_stat.frames_sent.is_defined()) {
    v8_stat->setFramesSent(*webrtc_stat.frames_sent);
  }
  if (webrtc_stat.huge_frames_sent.is_defined()) {
    v8_stat->setHugeFramesSent(*webrtc_stat.huge_frames_sent);
  }
  if (webrtc_stat.frames_encoded.is_defined()) {
    v8_stat->setFramesEncoded(*webrtc_stat.frames_encoded);
  }
  if (webrtc_stat.key_frames_encoded.is_defined()) {
    v8_stat->setKeyFramesEncoded(*webrtc_stat.key_frames_encoded);
  }
  if (webrtc_stat.qp_sum.is_defined()) {
    v8_stat->setQpSum(*webrtc_stat.qp_sum);
  }
  if (webrtc_stat.total_encode_time.is_defined()) {
    v8_stat->setTotalEncodeTime(*webrtc_stat.total_encode_time);
  }
  if (webrtc_stat.total_packet_send_delay.is_defined()) {
    v8_stat->setTotalPacketSendDelay(*webrtc_stat.total_packet_send_delay);
  }
  if (webrtc_stat.quality_limitation_reason.is_defined()) {
    v8_stat->setQualityLimitationReason(
        String::FromUTF8(*webrtc_stat.quality_limitation_reason));
  }
  if (webrtc_stat.quality_limitation_durations.is_defined()) {
    Vector<std::pair<String, double>> quality_durations;
    for (const auto& [key, value] : *webrtc_stat.quality_limitation_durations) {
      quality_durations.emplace_back(String::FromUTF8(key), value);
    }
    v8_stat->setQualityLimitationDurations(std::move(quality_durations));
  }
  if (webrtc_stat.quality_limitation_resolution_changes.is_defined()) {
    v8_stat->setQualityLimitationResolutionChanges(
        *webrtc_stat.quality_limitation_resolution_changes);
  }
  if (webrtc_stat.nack_count.is_defined()) {
    v8_stat->setNackCount(*webrtc_stat.nack_count);
  }
  if (webrtc_stat.fir_count.is_defined()) {
    v8_stat->setFirCount(*webrtc_stat.fir_count);
  }
  if (webrtc_stat.pli_count.is_defined()) {
    v8_stat->setPliCount(*webrtc_stat.pli_count);
  }
  if (webrtc_stat.active.is_defined()) {
    v8_stat->setActive(*webrtc_stat.active);
  }
  if (webrtc_stat.scalability_mode.is_defined()) {
    v8_stat->setScalabilityMode(
        String::FromUTF8(*webrtc_stat.scalability_mode));
  }
  if (expose_hardware_caps) {
    if (webrtc_stat.encoder_implementation.is_defined()) {
      v8_stat->setEncoderImplementation(
          String::FromUTF8(*webrtc_stat.encoder_implementation));
    }
    if (webrtc_stat.power_efficient_encoder.is_defined()) {
      v8_stat->setPowerEfficientEncoder(*webrtc_stat.power_efficient_encoder);
    }
  }
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcoutboundrtpstreamstats-contenttype
  if (webrtc_stat.content_type.is_defined()) {
    v8_stat->setContentType(String::FromUTF8(*webrtc_stat.content_type));
  }
  return v8_stat;
}

RTCRemoteOutboundRtpStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCRemoteOutboundRtpStreamStats& webrtc_stat,
    bool unship_deprecated_stats) {
  RTCRemoteOutboundRtpStreamStats* v8_stat =
      MakeGarbageCollected<RTCRemoteOutboundRtpStreamStats>(
          script_state->GetIsolate());
  // RTCRtpStreamStats
  if (webrtc_stat.ssrc.is_defined()) {
    v8_stat->setSsrc(*webrtc_stat.ssrc);
  }
  if (webrtc_stat.kind.is_defined()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.transport_id.is_defined()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.codec_id.is_defined()) {
    v8_stat->setCodecId(String::FromUTF8(*webrtc_stat.codec_id));
  }
  // RTCRtpStreamStats legacy stats
  if (webrtc_stat.media_type.is_defined()) {
    v8_stat->setMediaType(String::FromUTF8(*webrtc_stat.media_type));
  }
  if (!unship_deprecated_stats && webrtc_stat.track_id.is_defined()) {
    v8_stat->setTrackId(String::FromUTF8(*webrtc_stat.track_id));
  }
  // RTCSendRtpStreamStats
  if (webrtc_stat.packets_sent.is_defined()) {
    v8_stat->setPacketsSent(*webrtc_stat.packets_sent);
  }
  if (webrtc_stat.bytes_sent.is_defined()) {
    v8_stat->setBytesSent(*webrtc_stat.bytes_sent);
  }
  // RTCRemoteOutboundRtpStreamStats
  if (webrtc_stat.local_id.is_defined()) {
    v8_stat->setLocalId(String::FromUTF8(*webrtc_stat.local_id));
  }
  if (webrtc_stat.remote_timestamp.is_defined()) {
    v8_stat->setRemoteTimestamp(*webrtc_stat.remote_timestamp);
  }
  if (webrtc_stat.reports_sent.is_defined()) {
    v8_stat->setReportsSent(*webrtc_stat.reports_sent);
  }
  if (webrtc_stat.round_trip_time.is_defined()) {
    v8_stat->setRoundTripTime(*webrtc_stat.round_trip_time);
  }
  if (webrtc_stat.total_round_trip_time.is_defined()) {
    v8_stat->setTotalRoundTripTime(*webrtc_stat.total_round_trip_time);
  }
  if (webrtc_stat.round_trip_time_measurements.is_defined()) {
    v8_stat->setRoundTripTimeMeasurements(
        *webrtc_stat.round_trip_time_measurements);
  }
  return v8_stat;
}

RTCAudioSourceStats* ToV8Stat(ScriptState* script_state,
                              const webrtc::RTCAudioSourceStats& webrtc_stat) {
  RTCAudioSourceStats* v8_stat =
      MakeGarbageCollected<RTCAudioSourceStats>(script_state->GetIsolate());
  // RTCMediaSourceStats
  if (webrtc_stat.track_identifier.is_defined()) {
    v8_stat->setTrackIdentifier(
        String::FromUTF8(*webrtc_stat.track_identifier));
  }
  if (webrtc_stat.kind.is_defined()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  // RTCAudioSourceStats
  if (webrtc_stat.audio_level.is_defined()) {
    v8_stat->setAudioLevel(*webrtc_stat.audio_level);
  }
  if (webrtc_stat.total_audio_energy.is_defined()) {
    v8_stat->setTotalAudioEnergy(*webrtc_stat.total_audio_energy);
  }
  if (webrtc_stat.total_samples_duration.is_defined()) {
    v8_stat->setTotalSamplesDuration(*webrtc_stat.total_samples_duration);
  }
  if (webrtc_stat.echo_return_loss.is_defined()) {
    v8_stat->setEchoReturnLoss(*webrtc_stat.echo_return_loss);
  }
  if (webrtc_stat.echo_return_loss_enhancement.is_defined()) {
    v8_stat->setEchoReturnLossEnhancement(
        *webrtc_stat.echo_return_loss_enhancement);
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#videosourcestats-dict*
RTCVideoSourceStats* ToV8Stat(ScriptState* script_state,
                              const webrtc::RTCVideoSourceStats& webrtc_stat) {
  RTCVideoSourceStats* v8_stat =
      MakeGarbageCollected<RTCVideoSourceStats>(script_state->GetIsolate());
  // RTCMediaSourceStats
  if (webrtc_stat.track_identifier.is_defined()) {
    v8_stat->setTrackIdentifier(
        String::FromUTF8(*webrtc_stat.track_identifier));
  }
  if (webrtc_stat.kind.is_defined()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  // RTCVideoSourceStats
  if (webrtc_stat.width.is_defined()) {
    v8_stat->setWidth(*webrtc_stat.width);
  }
  if (webrtc_stat.height.is_defined()) {
    v8_stat->setHeight(*webrtc_stat.height);
  }
  if (webrtc_stat.frames.is_defined()) {
    v8_stat->setFrames(*webrtc_stat.frames);
  }
  if (webrtc_stat.frames_per_second.is_defined()) {
    v8_stat->setFramesPerSecond(*webrtc_stat.frames_per_second);
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#playoutstats-dict*
RTCAudioPlayoutStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCAudioPlayoutStats& webrtc_stat) {
  RTCAudioPlayoutStats* v8_stat =
      MakeGarbageCollected<RTCAudioPlayoutStats>(script_state->GetIsolate());

  if (webrtc_stat.kind.is_defined()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.synthesized_samples_duration.is_defined()) {
    v8_stat->setSynthesizedSamplesDuration(
        *webrtc_stat.synthesized_samples_duration);
  }
  if (webrtc_stat.synthesized_samples_events.is_defined()) {
    v8_stat->setSynthesizedSamplesEvents(base::saturated_cast<uint32_t>(
        *webrtc_stat.synthesized_samples_events));
  }
  if (webrtc_stat.total_samples_duration.is_defined()) {
    v8_stat->setTotalSamplesDuration(*webrtc_stat.total_samples_duration);
  }
  if (webrtc_stat.total_playout_delay.is_defined()) {
    v8_stat->setTotalPlayoutDelay(*webrtc_stat.total_playout_delay);
  }
  if (webrtc_stat.total_samples_count.is_defined()) {
    v8_stat->setTotalSamplesCount(*webrtc_stat.total_samples_count);
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#pcstats-dict*
RTCPeerConnectionStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCPeerConnectionStats& webrtc_stat) {
  RTCPeerConnectionStats* v8_stat =
      MakeGarbageCollected<RTCPeerConnectionStats>(script_state->GetIsolate());

  if (webrtc_stat.data_channels_opened.is_defined()) {
    v8_stat->setDataChannelsOpened(*webrtc_stat.data_channels_opened);
  }
  if (webrtc_stat.data_channels_closed.is_defined()) {
    v8_stat->setDataChannelsClosed(*webrtc_stat.data_channels_closed);
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#dcstats-dict*
RTCDataChannelStats* ToV8Stat(ScriptState* script_state,
                              const webrtc::RTCDataChannelStats& webrtc_stat) {
  RTCDataChannelStats* v8_stat =
      MakeGarbageCollected<RTCDataChannelStats>(script_state->GetIsolate());

  if (webrtc_stat.label.is_defined()) {
    v8_stat->setLabel(String::FromUTF8(*webrtc_stat.label));
  }
  if (webrtc_stat.protocol.is_defined()) {
    v8_stat->setProtocol(String::FromUTF8(*webrtc_stat.protocol));
  }
  if (webrtc_stat.data_channel_identifier.is_defined()) {
    v8_stat->setDataChannelIdentifier(*webrtc_stat.data_channel_identifier);
  }
  if (webrtc_stat.state.is_defined()) {
    v8_stat->setState(String::FromUTF8(*webrtc_stat.state));
  }
  if (webrtc_stat.messages_sent.is_defined()) {
    v8_stat->setMessagesSent(*webrtc_stat.messages_sent);
  }
  if (webrtc_stat.bytes_sent.is_defined()) {
    v8_stat->setBytesSent(*webrtc_stat.bytes_sent);
  }
  if (webrtc_stat.messages_received.is_defined()) {
    v8_stat->setMessagesReceived(*webrtc_stat.messages_received);
  }
  if (webrtc_stat.bytes_received.is_defined()) {
    v8_stat->setBytesReceived(*webrtc_stat.bytes_received);
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#transportstats-dict*
RTCTransportStats* ToV8Stat(ScriptState* script_state,
                            const webrtc::RTCTransportStats& webrtc_stat) {
  RTCTransportStats* v8_stat =
      MakeGarbageCollected<RTCTransportStats>(script_state->GetIsolate());

  if (webrtc_stat.packets_sent.is_defined()) {
    v8_stat->setPacketsSent(*webrtc_stat.packets_sent);
  }
  if (webrtc_stat.packets_received.is_defined()) {
    v8_stat->setPacketsReceived(*webrtc_stat.packets_received);
  }
  if (webrtc_stat.bytes_sent.is_defined()) {
    v8_stat->setBytesSent(*webrtc_stat.bytes_sent);
  }
  if (webrtc_stat.bytes_received.is_defined()) {
    v8_stat->setBytesReceived(*webrtc_stat.bytes_received);
  }
  if (webrtc_stat.ice_role.is_defined()) {
    v8_stat->setIceRole(String::FromUTF8(*webrtc_stat.ice_role));
  }
  if (webrtc_stat.ice_local_username_fragment.is_defined()) {
    v8_stat->setIceLocalUsernameFragment(
        String::FromUTF8(*webrtc_stat.ice_local_username_fragment));
  }
  if (webrtc_stat.dtls_state.is_defined()) {
    v8_stat->setDtlsState(String::FromUTF8(*webrtc_stat.dtls_state));
  }
  if (webrtc_stat.ice_state.is_defined()) {
    v8_stat->setIceState(String::FromUTF8(*webrtc_stat.ice_state));
  }
  if (webrtc_stat.selected_candidate_pair_id.is_defined()) {
    v8_stat->setSelectedCandidatePairId(
        String::FromUTF8(*webrtc_stat.selected_candidate_pair_id));
  }
  if (webrtc_stat.local_certificate_id.is_defined()) {
    v8_stat->setLocalCertificateId(
        String::FromUTF8(*webrtc_stat.local_certificate_id));
  }
  if (webrtc_stat.remote_certificate_id.is_defined()) {
    v8_stat->setRemoteCertificateId(
        String::FromUTF8(*webrtc_stat.remote_certificate_id));
  }
  if (webrtc_stat.tls_version.is_defined()) {
    v8_stat->setTlsVersion(String::FromUTF8(*webrtc_stat.tls_version));
  }
  if (webrtc_stat.dtls_cipher.is_defined()) {
    v8_stat->setDtlsCipher(String::FromUTF8(*webrtc_stat.dtls_cipher));
  }
  if (webrtc_stat.dtls_role.is_defined()) {
    v8_stat->setDtlsRole(String::FromUTF8(*webrtc_stat.dtls_role));
  }
  if (webrtc_stat.srtp_cipher.is_defined()) {
    v8_stat->setSrtpCipher(String::FromUTF8(*webrtc_stat.srtp_cipher));
  }
  if (webrtc_stat.selected_candidate_pair_changes.is_defined()) {
    v8_stat->setSelectedCandidatePairChanges(
        *webrtc_stat.selected_candidate_pair_changes);
  }
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtctransportstats-rtcptransportstatsid
  if (webrtc_stat.rtcp_transport_stats_id.is_defined()) {
    v8_stat->setRtcpTransportStatsId(
        String::FromUTF8(*webrtc_stat.rtcp_transport_stats_id));
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#icecandidate-dict*
RTCIceCandidateStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCIceCandidateStats& webrtc_stat) {
  RTCIceCandidateStats* v8_stat =
      MakeGarbageCollected<RTCIceCandidateStats>(script_state->GetIsolate());
  if (webrtc_stat.transport_id.is_defined()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.address.is_defined()) {
    v8_stat->setAddress(String::FromUTF8(*webrtc_stat.address));
  }
  if (webrtc_stat.port.is_defined()) {
    v8_stat->setPort(*webrtc_stat.port);
  }
  if (webrtc_stat.protocol.is_defined()) {
    v8_stat->setProtocol(String::FromUTF8(*webrtc_stat.protocol));
  }
  if (webrtc_stat.candidate_type.is_defined()) {
    v8_stat->setCandidateType(String::FromUTF8(*webrtc_stat.candidate_type));
  }
  if (webrtc_stat.priority.is_defined()) {
    v8_stat->setPriority(*webrtc_stat.priority);
  }
  if (webrtc_stat.url.is_defined()) {
    v8_stat->setUrl(String::FromUTF8(*webrtc_stat.url));
  }
  if (webrtc_stat.relay_protocol.is_defined()) {
    v8_stat->setRelayProtocol(String::FromUTF8(*webrtc_stat.relay_protocol));
  }
  if (webrtc_stat.foundation.is_defined()) {
    v8_stat->setFoundation(String::FromUTF8(*webrtc_stat.foundation));
  }
  if (webrtc_stat.related_address.is_defined()) {
    v8_stat->setRelatedAddress(String::FromUTF8(*webrtc_stat.related_address));
  }
  if (webrtc_stat.related_port.is_defined()) {
    v8_stat->setRelatedPort(*webrtc_stat.related_port);
  }
  if (webrtc_stat.username_fragment.is_defined()) {
    v8_stat->setUsernameFragment(
        String::FromUTF8(*webrtc_stat.username_fragment));
  }
  if (webrtc_stat.tcp_type.is_defined()) {
    v8_stat->setTcpType(String::FromUTF8(*webrtc_stat.tcp_type));
  }
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcicecandidatestats-networktype
  // Note: additional work needed to reach consensus on the privacy model.
  if (webrtc_stat.network_type.is_defined()) {
    v8_stat->setNetworkType(String::FromUTF8(*webrtc_stat.network_type));
  }
  // Non-standard and obsolete stats.
  if (webrtc_stat.is_remote.is_defined()) {
    v8_stat->setIsRemote(*webrtc_stat.is_remote);
  }
  if (webrtc_stat.ip.is_defined()) {
    v8_stat->setIp(String::FromUTF8(*webrtc_stat.ip));
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#candidatepair-dict*
RTCIceCandidatePairStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCIceCandidatePairStats& webrtc_stat) {
  RTCIceCandidatePairStats* v8_stat =
      MakeGarbageCollected<RTCIceCandidatePairStats>(
          script_state->GetIsolate());
  if (webrtc_stat.transport_id.is_defined()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.local_candidate_id.is_defined()) {
    v8_stat->setLocalCandidateId(
        String::FromUTF8(*webrtc_stat.local_candidate_id));
  }
  if (webrtc_stat.remote_candidate_id.is_defined()) {
    v8_stat->setRemoteCandidateId(
        String::FromUTF8(*webrtc_stat.remote_candidate_id));
  }
  if (webrtc_stat.state.is_defined()) {
    v8_stat->setState(String::FromUTF8(*webrtc_stat.state));
  }
  if (webrtc_stat.nominated.is_defined()) {
    v8_stat->setNominated(*webrtc_stat.nominated);
  }
  if (webrtc_stat.packets_sent.is_defined()) {
    v8_stat->setPacketsSent(*webrtc_stat.packets_sent);
  }
  if (webrtc_stat.packets_received.is_defined()) {
    v8_stat->setPacketsReceived(*webrtc_stat.packets_received);
  }
  if (webrtc_stat.bytes_sent.is_defined()) {
    v8_stat->setBytesSent(*webrtc_stat.bytes_sent);
  }
  if (webrtc_stat.bytes_received.is_defined()) {
    v8_stat->setBytesReceived(*webrtc_stat.bytes_received);
  }
  if (webrtc_stat.last_packet_sent_timestamp.is_defined()) {
    v8_stat->setLastPacketSentTimestamp(
        *webrtc_stat.last_packet_sent_timestamp);
  }
  if (webrtc_stat.last_packet_received_timestamp.is_defined()) {
    v8_stat->setLastPacketReceivedTimestamp(
        *webrtc_stat.last_packet_received_timestamp);
  }
  if (webrtc_stat.total_round_trip_time.is_defined()) {
    v8_stat->setTotalRoundTripTime(*webrtc_stat.total_round_trip_time);
  }
  if (webrtc_stat.current_round_trip_time.is_defined()) {
    v8_stat->setCurrentRoundTripTime(*webrtc_stat.current_round_trip_time);
  }
  if (webrtc_stat.available_outgoing_bitrate.is_defined()) {
    v8_stat->setAvailableOutgoingBitrate(
        *webrtc_stat.available_outgoing_bitrate);
  }
  if (webrtc_stat.available_incoming_bitrate.is_defined()) {
    v8_stat->setAvailableIncomingBitrate(
        *webrtc_stat.available_incoming_bitrate);
  }
  if (webrtc_stat.requests_received.is_defined()) {
    v8_stat->setRequestsReceived(*webrtc_stat.requests_received);
  }
  if (webrtc_stat.requests_sent.is_defined()) {
    v8_stat->setRequestsSent(*webrtc_stat.requests_sent);
  }
  if (webrtc_stat.responses_received.is_defined()) {
    v8_stat->setResponsesReceived(*webrtc_stat.responses_received);
  }
  if (webrtc_stat.responses_sent.is_defined()) {
    v8_stat->setResponsesSent(*webrtc_stat.responses_sent);
  }
  if (webrtc_stat.consent_requests_sent.is_defined()) {
    v8_stat->setConsentRequestsSent(*webrtc_stat.consent_requests_sent);
  }
  if (webrtc_stat.packets_discarded_on_send.is_defined()) {
    v8_stat->setPacketsDiscardedOnSend(
        base::saturated_cast<uint32_t>(*webrtc_stat.packets_discarded_on_send));
  }
  if (webrtc_stat.bytes_discarded_on_send.is_defined()) {
    v8_stat->setBytesDiscardedOnSend(*webrtc_stat.bytes_discarded_on_send);
  }
  // Non-standard and obsolete stats.
  if (webrtc_stat.writable.is_defined()) {
    v8_stat->setWritable(*webrtc_stat.writable);
  }
  if (webrtc_stat.priority.is_defined()) {
    v8_stat->setPriority(*webrtc_stat.priority);
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#certificatestats-dict*
RTCCertificateStats* ToV8Stat(ScriptState* script_state,
                              const webrtc::RTCCertificateStats& webrtc_stat) {
  RTCCertificateStats* v8_stat =
      MakeGarbageCollected<RTCCertificateStats>(script_state->GetIsolate());
  if (webrtc_stat.fingerprint.is_defined()) {
    v8_stat->setFingerprint(String::FromUTF8(*webrtc_stat.fingerprint));
  }
  if (webrtc_stat.fingerprint_algorithm.is_defined()) {
    v8_stat->setFingerprintAlgorithm(
        String::FromUTF8(*webrtc_stat.fingerprint_algorithm));
  }
  if (webrtc_stat.base64_certificate.is_defined()) {
    v8_stat->setBase64Certificate(
        String::FromUTF8(*webrtc_stat.base64_certificate));
  }
  if (webrtc_stat.issuer_certificate_id.is_defined()) {
    v8_stat->setIssuerCertificateId(
        String::FromUTF8(*webrtc_stat.issuer_certificate_id));
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#obsolete-rtcmediastreamstats-members
// TODO(https://crbug.com/1374215): Delete when "track" and "stream" have been
// unshipped.
RTCMediaStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::DEPRECATED_RTCMediaStreamStats& webrtc_stat) {
  RTCMediaStreamStats* v8_stat =
      MakeGarbageCollected<RTCMediaStreamStats>(script_state->GetIsolate());
  if (webrtc_stat.stream_identifier.is_defined()) {
    v8_stat->setStreamIdentifier(
        String::FromUTF8(*webrtc_stat.stream_identifier));
  }
  if (webrtc_stat.track_ids.is_defined()) {
    Vector<String> track_ids;
    for (const std::string& track_id : *webrtc_stat.track_ids) {
      track_ids.emplace_back(String::FromUTF8(track_id));
    }
    v8_stat->setTrackIds(std::move(track_ids));
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#obsolete-rtcmediastreamtrackstats-members
// TODO(https://crbug.com/1374215): Delete when "track" and "stream" have been
// unshipped.
RTCMediaStreamTrackStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::DEPRECATED_RTCMediaStreamTrackStats& webrtc_stat) {
  RTCMediaStreamTrackStats* v8_stat =
      MakeGarbageCollected<RTCMediaStreamTrackStats>(
          script_state->GetIsolate());
  if (webrtc_stat.track_identifier.is_defined()) {
    v8_stat->setTrackIdentifier(
        String::FromUTF8(*webrtc_stat.track_identifier));
  }
  if (webrtc_stat.ended.is_defined()) {
    v8_stat->setEnded(*webrtc_stat.ended);
  }
  if (webrtc_stat.kind.is_defined()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.remote_source.is_defined()) {
    v8_stat->setRemoteSource(*webrtc_stat.remote_source);
  }
  if (webrtc_stat.jitter_buffer_delay.is_defined()) {
    v8_stat->setJitterBufferDelay(*webrtc_stat.jitter_buffer_delay);
  }
  if (webrtc_stat.jitter_buffer_emitted_count.is_defined()) {
    v8_stat->setJitterBufferEmittedCount(
        *webrtc_stat.jitter_buffer_emitted_count);
  }
  // Audio-only, send and receive side.
  if (webrtc_stat.audio_level.is_defined()) {
    v8_stat->setAudioLevel(*webrtc_stat.audio_level);
  }
  if (webrtc_stat.total_audio_energy.is_defined()) {
    v8_stat->setTotalAudioEnergy(*webrtc_stat.total_audio_energy);
  }
  if (webrtc_stat.total_samples_duration.is_defined()) {
    v8_stat->setTotalSamplesDuration(*webrtc_stat.total_samples_duration);
  }
  // Audio-only, send side.
  if (webrtc_stat.echo_return_loss.is_defined()) {
    v8_stat->setEchoReturnLoss(*webrtc_stat.echo_return_loss);
  }
  if (webrtc_stat.echo_return_loss_enhancement.is_defined()) {
    v8_stat->setEchoReturnLossEnhancement(
        *webrtc_stat.echo_return_loss_enhancement);
  }
  // Audio-only, receive side.
  if (webrtc_stat.total_samples_received.is_defined()) {
    v8_stat->setTotalSamplesReceived(*webrtc_stat.total_samples_received);
  }
  if (webrtc_stat.concealed_samples.is_defined()) {
    v8_stat->setConcealedSamples(*webrtc_stat.concealed_samples);
  }
  if (webrtc_stat.silent_concealed_samples.is_defined()) {
    v8_stat->setSilentConcealedSamples(*webrtc_stat.silent_concealed_samples);
  }
  if (webrtc_stat.concealment_events.is_defined()) {
    v8_stat->setConcealmentEvents(*webrtc_stat.concealment_events);
  }
  if (webrtc_stat.inserted_samples_for_deceleration.is_defined()) {
    v8_stat->setInsertedSamplesForDeceleration(
        *webrtc_stat.inserted_samples_for_deceleration);
  }
  if (webrtc_stat.removed_samples_for_acceleration.is_defined()) {
    v8_stat->setRemovedSamplesForAcceleration(
        *webrtc_stat.removed_samples_for_acceleration);
  }
  // Video-only, send and receive side.
  if (webrtc_stat.frame_width.is_defined()) {
    v8_stat->setFrameWidth(*webrtc_stat.frame_width);
  }
  if (webrtc_stat.frame_height.is_defined()) {
    v8_stat->setFrameHeight(*webrtc_stat.frame_height);
  }
  // Video-only, send side.
  if (webrtc_stat.frames_sent.is_defined()) {
    v8_stat->setFramesSent(*webrtc_stat.frames_sent);
  }
  if (webrtc_stat.huge_frames_sent.is_defined()) {
    v8_stat->setHugeFramesSent(*webrtc_stat.huge_frames_sent);
  }
  // Video-only, receive side.
  if (webrtc_stat.frames_received.is_defined()) {
    v8_stat->setFramesReceived(*webrtc_stat.frames_received);
  }
  if (webrtc_stat.frames_decoded.is_defined()) {
    v8_stat->setFramesDecoded(*webrtc_stat.frames_decoded);
  }
  if (webrtc_stat.frames_dropped.is_defined()) {
    v8_stat->setFramesDropped(*webrtc_stat.frames_dropped);
  }
  // Non-standard and obsolete stats.
  if (webrtc_stat.media_source_id.is_defined()) {
    v8_stat->setMediaSourceId(String::FromUTF8(*webrtc_stat.media_source_id));
  }
  if (webrtc_stat.detached.is_defined()) {
    v8_stat->setDetached(*webrtc_stat.detached);
  }
  return v8_stat;
}

RTCStats* RTCStatsToIDL(ScriptState* script_state,
                        const webrtc::RTCStats& stat,
                        bool expose_hardware_caps,
                        bool unship_deprecated_stats) {
  RTCStats* v8_stats = nullptr;
  if (strcmp(stat.type(), "codec") == 0) {
    v8_stats = ToV8Stat(script_state, stat.cast_to<webrtc::RTCCodecStats>());
  } else if (strcmp(stat.type(), "inbound-rtp") == 0) {
    v8_stats =
        ToV8Stat(script_state, stat.cast_to<webrtc::RTCInboundRTPStreamStats>(),
                 expose_hardware_caps, unship_deprecated_stats);
  } else if (strcmp(stat.type(), "outbound-rtp") == 0) {
    v8_stats = ToV8Stat(script_state,
                        stat.cast_to<webrtc::RTCOutboundRTPStreamStats>(),
                        expose_hardware_caps, unship_deprecated_stats);
  } else if (strcmp(stat.type(), "remote-inbound-rtp") == 0) {
    v8_stats = ToV8Stat(script_state,
                        stat.cast_to<webrtc::RTCRemoteInboundRtpStreamStats>(),
                        unship_deprecated_stats);
  } else if (strcmp(stat.type(), "remote-outbound-rtp") == 0) {
    v8_stats = ToV8Stat(script_state,
                        stat.cast_to<webrtc::RTCRemoteOutboundRtpStreamStats>(),
                        unship_deprecated_stats);
  } else if (strcmp(stat.type(), "media-source") == 0) {
    // Type media-source indicates a parent type. The actual stats are based on
    // the kind.
    const auto& media_source =
        static_cast<const webrtc::RTCMediaSourceStats&>(stat);
    DCHECK(media_source.kind.is_defined());
    std::string kind = media_source.kind.ValueOrDefault("");
    if (kind == "audio") {
      v8_stats =
          ToV8Stat(script_state, stat.cast_to<webrtc::RTCAudioSourceStats>());
    } else if (kind == "video") {
      v8_stats =
          ToV8Stat(script_state, stat.cast_to<webrtc::RTCVideoSourceStats>());
    } else {
      NOTIMPLEMENTED() << "Unhandled media source stat type: " << kind;
      return nullptr;
    }
  } else if (strcmp(stat.type(), "media-playout") == 0) {
    v8_stats =
        ToV8Stat(script_state, stat.cast_to<webrtc::RTCAudioPlayoutStats>());
  } else if (strcmp(stat.type(), "peer-connection") == 0) {
    v8_stats =
        ToV8Stat(script_state, stat.cast_to<webrtc::RTCPeerConnectionStats>());
  } else if (strcmp(stat.type(), "data-channel") == 0) {
    v8_stats =
        ToV8Stat(script_state, stat.cast_to<webrtc::RTCDataChannelStats>());
  } else if (strcmp(stat.type(), "stream") == 0) {
    if (unship_deprecated_stats) {
      return nullptr;
    }
    v8_stats = ToV8Stat(script_state,
                        stat.cast_to<webrtc::DEPRECATED_RTCMediaStreamStats>());
  } else if (strcmp(stat.type(), "track") == 0) {
    if (unship_deprecated_stats) {
      return nullptr;
    }
    v8_stats =
        ToV8Stat(script_state,
                 stat.cast_to<webrtc::DEPRECATED_RTCMediaStreamTrackStats>());
  } else if (strcmp(stat.type(), "transport") == 0) {
    v8_stats =
        ToV8Stat(script_state, stat.cast_to<webrtc::RTCTransportStats>());
  } else if (strcmp(stat.type(), "candidate-pair") == 0) {
    v8_stats = ToV8Stat(script_state,
                        stat.cast_to<webrtc::RTCIceCandidatePairStats>());
  } else if (strcmp(stat.type(), "local-candidate") == 0) {
    v8_stats = ToV8Stat(script_state,
                        stat.cast_to<webrtc::RTCLocalIceCandidateStats>());
  } else if (strcmp(stat.type(), "remote-candidate") == 0) {
    v8_stats = ToV8Stat(script_state,
                        stat.cast_to<webrtc::RTCRemoteIceCandidateStats>());
  } else if (strcmp(stat.type(), "certificate") == 0) {
    v8_stats =
        ToV8Stat(script_state, stat.cast_to<webrtc::RTCCertificateStats>());
  } else {
    DVLOG(2) << "Unhandled stat-type " << stat.type();
    return nullptr;
  }

  v8_stats->setId(String::FromUTF8(stat.id()));
  v8_stats->setTimestamp(stat.timestamp().ms<double>());
  v8_stats->setType(String::FromUTF8(stat.type()));
  return v8_stats;
}

class RTCStatsReportIterationSource final
    : public PairSyncIterable<RTCStatsReport>::IterationSource {
 public:
  explicit RTCStatsReportIterationSource(
      std::unique_ptr<RTCStatsReportPlatform> report,
      bool use_web_idl)
      : report_(std::move(report)), use_web_idl_(use_web_idl) {}

  bool FetchNextItem(ScriptState* script_state,
                     String& key,
                     ScriptValue& value,
                     ExceptionState& exception_state) override {
    if (use_web_idl_) {
      return FetchNextItemIdl(script_state, key, value, exception_state);
    }
    std::unique_ptr<RTCStatsWrapper> stats = report_->Next();
    if (!stats) {
      return false;
    }
    key = stats->Id();
    value = ScriptValue(script_state->GetIsolate(),
                        RTCStatsToV8Object(script_state, stats.get()));
    return true;
  }

  bool FetchNextItemIdl(ScriptState* script_state,
                        String& key,
                        ScriptValue& value,
                        ExceptionState& exception_state) {
    const bool expose_hardware_caps =
        ExposeHardwareCapabilityStats(script_state);
    const webrtc::RTCStats* rtc_stats = report_->NextStats();
    RTCStats* v8_stat = nullptr;
    // Loop until a stat can be converted.
    while (rtc_stats) {
      v8_stat = RTCStatsToIDL(script_state, *rtc_stats, expose_hardware_caps,
                              report_->unship_deprecated_stats());
      if (v8_stat) {
        break;
      }
      rtc_stats = report_->NextStats();
    }
    if (!rtc_stats) {
      return false;
    }
    key = String::FromUTF8(rtc_stats->id());
    value = ScriptValue(script_state->GetIsolate(),
                        v8_stat->ToV8Value(script_state));
    return true;
  }

 private:
  std::unique_ptr<RTCStatsReportPlatform> report_;
  const bool use_web_idl_;
};

}  // namespace

Vector<webrtc::NonStandardGroupId> GetExposedGroupIds(
    const ScriptState* script_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());
  Vector<webrtc::NonStandardGroupId> enabled_origin_trials;
  if (RuntimeEnabledFeatures::RtcAudioJitterBufferMaxPacketsEnabled(context)) {
    enabled_origin_trials.emplace_back(
        webrtc::NonStandardGroupId::kRtcAudioJitterBufferMaxPackets);
  }
  if (RuntimeEnabledFeatures::RTCStatsRelativePacketArrivalDelayEnabled(
          context)) {
    enabled_origin_trials.emplace_back(
        webrtc::NonStandardGroupId::kRtcStatsRelativePacketArrivalDelay);
  }
  return enabled_origin_trials;
}

RTCStatsReport::RTCStatsReport(std::unique_ptr<RTCStatsReportPlatform> report)
    : report_(std::move(report)),
      use_web_idl_(
          base::FeatureList::IsEnabled(features::kWebRtcStatsReportIdl)) {}

uint32_t RTCStatsReport::size() const {
  return base::saturated_cast<uint32_t>(report_->Size());
}

PairSyncIterable<RTCStatsReport>::IterationSource*
RTCStatsReport::CreateIterationSource(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<RTCStatsReportIterationSource>(
      report_->CopyHandle(), use_web_idl_);
}

bool RTCStatsReport::GetMapEntryIdl(ScriptState* script_state,
                                    const String& key,
                                    ScriptValue& value,
                                    ExceptionState&) {
  const webrtc::RTCStats* stats = report_->stats_report().Get(key.Utf8());
  if (!stats) {
    return false;
  }

  RTCStats* v8_stats = RTCStatsToIDL(
      script_state, *stats, ExposeHardwareCapabilityStats(script_state),
      report_->unship_deprecated_stats());
  if (!v8_stats) {
    return false;
  }
  value = ScriptValue(script_state->GetIsolate(),
                      v8_stats->ToV8Value(script_state));
  return true;
}

bool RTCStatsReport::GetMapEntry(ScriptState* script_state,
                                 const String& key,
                                 ScriptValue& value,
                                 ExceptionState& exception_state) {
  if (use_web_idl_) {
    return GetMapEntryIdl(script_state, key, value, exception_state);
  }
  std::unique_ptr<RTCStatsWrapper> stats = report_->GetStats(key);
  if (!stats) {
    return false;
  }
  value = ScriptValue(script_state->GetIsolate(),
                      RTCStatsToV8Object(script_state, stats.get()));
  return true;
}

}  // namespace blink
