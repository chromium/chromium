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
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"

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
    NOTREACHED_IN_MIGRATION();
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

RTCCodecStats* ToV8Stat(ScriptState* script_state,
                        const webrtc::RTCCodecStats& webrtc_stat) {
  RTCCodecStats* v8_codec =
      MakeGarbageCollected<RTCCodecStats>(script_state->GetIsolate());
  if (webrtc_stat.transport_id.has_value()) {
    v8_codec->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.payload_type.has_value()) {
    v8_codec->setPayloadType(*webrtc_stat.payload_type);
  }
  if (webrtc_stat.channels.has_value()) {
    v8_codec->setChannels(*webrtc_stat.channels);
  }
  if (webrtc_stat.mime_type.has_value()) {
    v8_codec->setMimeType(String::FromUTF8(*webrtc_stat.mime_type));
  }
  if (webrtc_stat.clock_rate.has_value()) {
    v8_codec->setClockRate(*webrtc_stat.clock_rate);
  }
  if (webrtc_stat.sdp_fmtp_line.has_value()) {
    v8_codec->setSdpFmtpLine(String::FromUTF8(*webrtc_stat.sdp_fmtp_line));
  }
  return v8_codec;
}

RTCInboundRtpStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCInboundRtpStreamStats& webrtc_stat,
    bool expose_hardware_caps) {
  RTCInboundRtpStreamStats* v8_stat =
      MakeGarbageCollected<RTCInboundRtpStreamStats>(
          script_state->GetIsolate());
  // RTCRtpStreamStats
  if (webrtc_stat.ssrc.has_value()) {
    v8_stat->setSsrc(*webrtc_stat.ssrc);
  }
  if (webrtc_stat.kind.has_value()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
    // mediaType is a legacy alias for kind.
    v8_stat->setMediaType(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.transport_id.has_value()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.codec_id.has_value()) {
    v8_stat->setCodecId(String::FromUTF8(*webrtc_stat.codec_id));
  }
  // RTCReceivedRtpStreamStats
  if (webrtc_stat.packets_lost.has_value()) {
    v8_stat->setPacketsLost(*webrtc_stat.packets_lost);
  }
  if (webrtc_stat.jitter.has_value()) {
    v8_stat->setJitter(*webrtc_stat.jitter);
  }
  // RTCInboundRtpStreamStats
  if (webrtc_stat.track_identifier.has_value()) {
    v8_stat->setTrackIdentifier(
        String::FromUTF8(*webrtc_stat.track_identifier));
  }
  if (webrtc_stat.mid.has_value()) {
    v8_stat->setMid(String::FromUTF8(*webrtc_stat.mid));
  }
  if (webrtc_stat.remote_id.has_value()) {
    v8_stat->setRemoteId(String::FromUTF8(*webrtc_stat.remote_id));
  }
  if (webrtc_stat.frames_decoded.has_value()) {
    v8_stat->setFramesDecoded(*webrtc_stat.frames_decoded);
  }
  if (webrtc_stat.key_frames_decoded.has_value()) {
    v8_stat->setKeyFramesDecoded(*webrtc_stat.key_frames_decoded);
  }
  if (webrtc_stat.frames_dropped.has_value()) {
    v8_stat->setFramesDropped(*webrtc_stat.frames_dropped);
  }
  if (webrtc_stat.frame_width.has_value()) {
    v8_stat->setFrameWidth(*webrtc_stat.frame_width);
  }
  if (webrtc_stat.frame_height.has_value()) {
    v8_stat->setFrameHeight(*webrtc_stat.frame_height);
  }
  if (webrtc_stat.frames_per_second.has_value()) {
    v8_stat->setFramesPerSecond(*webrtc_stat.frames_per_second);
  }
  if (webrtc_stat.qp_sum.has_value()) {
    v8_stat->setQpSum(*webrtc_stat.qp_sum);
  }
  if (webrtc_stat.total_decode_time.has_value()) {
    v8_stat->setTotalDecodeTime(*webrtc_stat.total_decode_time);
  }
  if (webrtc_stat.total_inter_frame_delay.has_value()) {
    v8_stat->setTotalInterFrameDelay(*webrtc_stat.total_inter_frame_delay);
  }
  if (webrtc_stat.total_squared_inter_frame_delay.has_value()) {
    v8_stat->setTotalSquaredInterFrameDelay(
        *webrtc_stat.total_squared_inter_frame_delay);
  }
  if (webrtc_stat.pause_count.has_value()) {
    v8_stat->setPauseCount(*webrtc_stat.pause_count);
  }
  if (webrtc_stat.total_pauses_duration.has_value()) {
    v8_stat->setTotalPausesDuration(*webrtc_stat.total_pauses_duration);
  }
  if (webrtc_stat.freeze_count.has_value()) {
    v8_stat->setFreezeCount(*webrtc_stat.freeze_count);
  }
  if (webrtc_stat.total_freezes_duration.has_value()) {
    v8_stat->setTotalFreezesDuration(*webrtc_stat.total_freezes_duration);
  }
  if (webrtc_stat.last_packet_received_timestamp.has_value()) {
    v8_stat->setLastPacketReceivedTimestamp(
        *webrtc_stat.last_packet_received_timestamp);
  }
  if (webrtc_stat.header_bytes_received.has_value()) {
    v8_stat->setHeaderBytesReceived(*webrtc_stat.header_bytes_received);
  }
  if (webrtc_stat.packets_discarded.has_value()) {
    v8_stat->setPacketsDiscarded(*webrtc_stat.packets_discarded);
  }
  if (webrtc_stat.packets_received.has_value()) {
    v8_stat->setPacketsReceived(*webrtc_stat.packets_received);
  }
  if (webrtc_stat.fec_packets_received.has_value()) {
    v8_stat->setFecPacketsReceived(*webrtc_stat.fec_packets_received);
  }
  if (webrtc_stat.fec_packets_discarded.has_value()) {
    v8_stat->setFecPacketsDiscarded(*webrtc_stat.fec_packets_discarded);
  }
  if (webrtc_stat.fec_bytes_received.has_value()) {
    v8_stat->setFecBytesReceived(*webrtc_stat.fec_bytes_received);
  }
  if (webrtc_stat.fec_ssrc.has_value()) {
    v8_stat->setFecSsrc(*webrtc_stat.fec_ssrc);
  }
  if (webrtc_stat.bytes_received.has_value()) {
    v8_stat->setBytesReceived(*webrtc_stat.bytes_received);
  }
  if (webrtc_stat.nack_count.has_value()) {
    v8_stat->setNackCount(*webrtc_stat.nack_count);
  }
  if (webrtc_stat.fir_count.has_value()) {
    v8_stat->setFirCount(*webrtc_stat.fir_count);
  }
  if (webrtc_stat.pli_count.has_value()) {
    v8_stat->setPliCount(*webrtc_stat.pli_count);
  }
  if (webrtc_stat.total_processing_delay.has_value()) {
    v8_stat->setTotalProcessingDelay(*webrtc_stat.total_processing_delay);
  }
  if (webrtc_stat.estimated_playout_timestamp.has_value()) {
    v8_stat->setEstimatedPlayoutTimestamp(
        *webrtc_stat.estimated_playout_timestamp);
  }
  if (webrtc_stat.jitter_buffer_delay.has_value()) {
    v8_stat->setJitterBufferDelay(*webrtc_stat.jitter_buffer_delay);
  }
  if (webrtc_stat.jitter_buffer_target_delay.has_value()) {
    v8_stat->setJitterBufferTargetDelay(
        *webrtc_stat.jitter_buffer_target_delay);
  }
  if (webrtc_stat.jitter_buffer_emitted_count.has_value()) {
    v8_stat->setJitterBufferEmittedCount(
        *webrtc_stat.jitter_buffer_emitted_count);
  }
  if (webrtc_stat.jitter_buffer_minimum_delay.has_value()) {
    v8_stat->setJitterBufferMinimumDelay(
        *webrtc_stat.jitter_buffer_minimum_delay);
  }
  if (webrtc_stat.total_samples_received.has_value()) {
    v8_stat->setTotalSamplesReceived(*webrtc_stat.total_samples_received);
  }
  if (webrtc_stat.concealed_samples.has_value()) {
    v8_stat->setConcealedSamples(*webrtc_stat.concealed_samples);
  }
  if (webrtc_stat.silent_concealed_samples.has_value()) {
    v8_stat->setSilentConcealedSamples(*webrtc_stat.silent_concealed_samples);
  }
  if (webrtc_stat.concealment_events.has_value()) {
    v8_stat->setConcealmentEvents(*webrtc_stat.concealment_events);
  }
  if (webrtc_stat.inserted_samples_for_deceleration.has_value()) {
    v8_stat->setInsertedSamplesForDeceleration(
        *webrtc_stat.inserted_samples_for_deceleration);
  }
  if (webrtc_stat.removed_samples_for_acceleration.has_value()) {
    v8_stat->setRemovedSamplesForAcceleration(
        *webrtc_stat.removed_samples_for_acceleration);
  }
  if (webrtc_stat.audio_level.has_value()) {
    v8_stat->setAudioLevel(*webrtc_stat.audio_level);
  }
  if (webrtc_stat.total_audio_energy.has_value()) {
    v8_stat->setTotalAudioEnergy(*webrtc_stat.total_audio_energy);
  }
  if (webrtc_stat.total_samples_duration.has_value()) {
    v8_stat->setTotalSamplesDuration(*webrtc_stat.total_samples_duration);
  }
  if (webrtc_stat.frames_received.has_value()) {
    v8_stat->setFramesReceived(*webrtc_stat.frames_received);
  }
  if (webrtc_stat.playout_id.has_value()) {
    v8_stat->setPlayoutId(String::FromUTF8(*webrtc_stat.playout_id));
  }
  if (webrtc_stat.frames_assembled_from_multiple_packets.has_value()) {
    v8_stat->setFramesAssembledFromMultiplePackets(
        *webrtc_stat.frames_assembled_from_multiple_packets);
  }
  if (webrtc_stat.total_assembly_time.has_value()) {
    v8_stat->setTotalAssemblyTime(*webrtc_stat.total_assembly_time);
  }
  if (expose_hardware_caps) {
    if (webrtc_stat.power_efficient_decoder.has_value()) {
      v8_stat->setPowerEfficientDecoder(*webrtc_stat.power_efficient_decoder);
    }
    if (webrtc_stat.decoder_implementation.has_value()) {
      v8_stat->setDecoderImplementation(
          String::FromUTF8(*webrtc_stat.decoder_implementation));
    }
  }
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcinboundrtpstreamstats-contenttype
  if (webrtc_stat.content_type.has_value()) {
    v8_stat->setContentType(String::FromUTF8(*webrtc_stat.content_type));
  }
  // https://github.com/w3c/webrtc-provisional-stats/issues/40
  if (webrtc_stat.goog_timing_frame_info.has_value()) {
    v8_stat->setGoogTimingFrameInfo(
        String::FromUTF8(*webrtc_stat.goog_timing_frame_info));
  }
  if (webrtc_stat.retransmitted_packets_received.has_value()) {
    v8_stat->setRetransmittedPacketsReceived(
        *webrtc_stat.retransmitted_packets_received);
  }
  if (webrtc_stat.retransmitted_bytes_received.has_value()) {
    v8_stat->setRetransmittedBytesReceived(
        *webrtc_stat.retransmitted_bytes_received);
  }
  if (webrtc_stat.rtx_ssrc.has_value()) {
    v8_stat->setRtxSsrc(*webrtc_stat.rtx_ssrc);
  }
  return v8_stat;
}

RTCRemoteInboundRtpStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCRemoteInboundRtpStreamStats& webrtc_stat) {
  RTCRemoteInboundRtpStreamStats* v8_stat =
      MakeGarbageCollected<RTCRemoteInboundRtpStreamStats>(
          script_state->GetIsolate());
  // RTCRtpStreamStats
  if (webrtc_stat.ssrc.has_value()) {
    v8_stat->setSsrc(*webrtc_stat.ssrc);
  }
  if (webrtc_stat.kind.has_value()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
    // mediaType is a legacy alias for kind.
    v8_stat->setMediaType(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.transport_id.has_value()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.codec_id.has_value()) {
    v8_stat->setCodecId(String::FromUTF8(*webrtc_stat.codec_id));
  }
  // RTCReceivedRtpStreamStats
  if (webrtc_stat.packets_lost.has_value()) {
    v8_stat->setPacketsLost(*webrtc_stat.packets_lost);
  }
  if (webrtc_stat.jitter.has_value()) {
    v8_stat->setJitter(*webrtc_stat.jitter);
  }
  // RTCRemoteInboundRtpStreamStats
  if (webrtc_stat.local_id.has_value()) {
    v8_stat->setLocalId(String::FromUTF8(*webrtc_stat.local_id));
  }
  if (webrtc_stat.round_trip_time.has_value()) {
    v8_stat->setRoundTripTime(*webrtc_stat.round_trip_time);
  }
  if (webrtc_stat.total_round_trip_time.has_value()) {
    v8_stat->setTotalRoundTripTime(*webrtc_stat.total_round_trip_time);
  }
  if (webrtc_stat.fraction_lost.has_value()) {
    v8_stat->setFractionLost(*webrtc_stat.fraction_lost);
  }
  if (webrtc_stat.round_trip_time_measurements.has_value()) {
    v8_stat->setRoundTripTimeMeasurements(
        *webrtc_stat.round_trip_time_measurements);
  }
  return v8_stat;
}

RTCOutboundRtpStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCOutboundRtpStreamStats& webrtc_stat,
    bool expose_hardware_caps) {
  RTCOutboundRtpStreamStats* v8_stat =
      MakeGarbageCollected<RTCOutboundRtpStreamStats>(
          script_state->GetIsolate());
  // RTCRtpStreamStats
  if (webrtc_stat.ssrc.has_value()) {
    v8_stat->setSsrc(*webrtc_stat.ssrc);
  }
  if (webrtc_stat.kind.has_value()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
    // mediaType is a legacy alias for kind.
    v8_stat->setMediaType(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.transport_id.has_value()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.codec_id.has_value()) {
    v8_stat->setCodecId(String::FromUTF8(*webrtc_stat.codec_id));
  }
  // RTCSentRtpStreamStats
  if (webrtc_stat.packets_sent.has_value()) {
    v8_stat->setPacketsSent(*webrtc_stat.packets_sent);
  }
  if (webrtc_stat.bytes_sent.has_value()) {
    v8_stat->setBytesSent(*webrtc_stat.bytes_sent);
  }
  // RTCOutboundRtpStreamStats
  if (webrtc_stat.mid.has_value()) {
    v8_stat->setMid(String::FromUTF8(*webrtc_stat.mid));
  }
  if (webrtc_stat.media_source_id.has_value()) {
    v8_stat->setMediaSourceId(String::FromUTF8(*webrtc_stat.media_source_id));
  }
  if (webrtc_stat.remote_id.has_value()) {
    v8_stat->setRemoteId(String::FromUTF8(*webrtc_stat.remote_id));
  }
  if (webrtc_stat.rid.has_value()) {
    v8_stat->setRid(String::FromUTF8(*webrtc_stat.rid));
  }
  if (webrtc_stat.header_bytes_sent.has_value()) {
    v8_stat->setHeaderBytesSent(*webrtc_stat.header_bytes_sent);
  }
  if (webrtc_stat.retransmitted_packets_sent.has_value()) {
    v8_stat->setRetransmittedPacketsSent(
        *webrtc_stat.retransmitted_packets_sent);
  }
  if (webrtc_stat.retransmitted_bytes_sent.has_value()) {
    v8_stat->setRetransmittedBytesSent(*webrtc_stat.retransmitted_bytes_sent);
  }
  if (webrtc_stat.rtx_ssrc.has_value()) {
    v8_stat->setRtxSsrc(*webrtc_stat.rtx_ssrc);
  }
  if (webrtc_stat.target_bitrate.has_value()) {
    v8_stat->setTargetBitrate(*webrtc_stat.target_bitrate);
  }
  if (webrtc_stat.total_encoded_bytes_target.has_value()) {
    v8_stat->setTotalEncodedBytesTarget(
        *webrtc_stat.total_encoded_bytes_target);
  }
  if (webrtc_stat.frame_width.has_value()) {
    v8_stat->setFrameWidth(*webrtc_stat.frame_width);
  }
  if (webrtc_stat.frame_height.has_value()) {
    v8_stat->setFrameHeight(*webrtc_stat.frame_height);
  }
  if (webrtc_stat.frames_per_second.has_value()) {
    v8_stat->setFramesPerSecond(*webrtc_stat.frames_per_second);
  }
  if (webrtc_stat.frames_sent.has_value()) {
    v8_stat->setFramesSent(*webrtc_stat.frames_sent);
  }
  if (webrtc_stat.huge_frames_sent.has_value()) {
    v8_stat->setHugeFramesSent(*webrtc_stat.huge_frames_sent);
  }
  if (webrtc_stat.frames_encoded.has_value()) {
    v8_stat->setFramesEncoded(*webrtc_stat.frames_encoded);
  }
  if (webrtc_stat.key_frames_encoded.has_value()) {
    v8_stat->setKeyFramesEncoded(*webrtc_stat.key_frames_encoded);
  }
  if (webrtc_stat.qp_sum.has_value()) {
    v8_stat->setQpSum(*webrtc_stat.qp_sum);
  }
  if (webrtc_stat.total_encode_time.has_value()) {
    v8_stat->setTotalEncodeTime(*webrtc_stat.total_encode_time);
  }
  if (webrtc_stat.total_packet_send_delay.has_value()) {
    v8_stat->setTotalPacketSendDelay(*webrtc_stat.total_packet_send_delay);
  }
  if (webrtc_stat.quality_limitation_reason.has_value()) {
    v8_stat->setQualityLimitationReason(
        String::FromUTF8(*webrtc_stat.quality_limitation_reason));
  }
  if (webrtc_stat.quality_limitation_durations.has_value()) {
    Vector<std::pair<String, double>> quality_durations;
    for (const auto& [key, value] : *webrtc_stat.quality_limitation_durations) {
      quality_durations.emplace_back(String::FromUTF8(key), value);
    }
    v8_stat->setQualityLimitationDurations(std::move(quality_durations));
  }
  if (webrtc_stat.quality_limitation_resolution_changes.has_value()) {
    v8_stat->setQualityLimitationResolutionChanges(
        *webrtc_stat.quality_limitation_resolution_changes);
  }
  if (webrtc_stat.nack_count.has_value()) {
    v8_stat->setNackCount(*webrtc_stat.nack_count);
  }
  if (webrtc_stat.fir_count.has_value()) {
    v8_stat->setFirCount(*webrtc_stat.fir_count);
  }
  if (webrtc_stat.pli_count.has_value()) {
    v8_stat->setPliCount(*webrtc_stat.pli_count);
  }
  if (webrtc_stat.active.has_value()) {
    v8_stat->setActive(*webrtc_stat.active);
  }
  if (webrtc_stat.scalability_mode.has_value()) {
    v8_stat->setScalabilityMode(
        String::FromUTF8(*webrtc_stat.scalability_mode));
  }
  if (expose_hardware_caps) {
    if (webrtc_stat.encoder_implementation.has_value()) {
      v8_stat->setEncoderImplementation(
          String::FromUTF8(*webrtc_stat.encoder_implementation));
    }
    if (webrtc_stat.power_efficient_encoder.has_value()) {
      v8_stat->setPowerEfficientEncoder(*webrtc_stat.power_efficient_encoder);
    }
  }
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcoutboundrtpstreamstats-contenttype
  if (webrtc_stat.content_type.has_value()) {
    v8_stat->setContentType(String::FromUTF8(*webrtc_stat.content_type));
  }
  return v8_stat;
}

RTCRemoteOutboundRtpStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCRemoteOutboundRtpStreamStats& webrtc_stat) {
  RTCRemoteOutboundRtpStreamStats* v8_stat =
      MakeGarbageCollected<RTCRemoteOutboundRtpStreamStats>(
          script_state->GetIsolate());
  // RTCRtpStreamStats
  if (webrtc_stat.ssrc.has_value()) {
    v8_stat->setSsrc(*webrtc_stat.ssrc);
  }
  if (webrtc_stat.kind.has_value()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
    // mediaType is a legacy alias for kind.
    v8_stat->setMediaType(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.transport_id.has_value()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.codec_id.has_value()) {
    v8_stat->setCodecId(String::FromUTF8(*webrtc_stat.codec_id));
  }
  // RTCSendRtpStreamStats
  if (webrtc_stat.packets_sent.has_value()) {
    v8_stat->setPacketsSent(*webrtc_stat.packets_sent);
  }
  if (webrtc_stat.bytes_sent.has_value()) {
    v8_stat->setBytesSent(*webrtc_stat.bytes_sent);
  }
  // RTCRemoteOutboundRtpStreamStats
  if (webrtc_stat.local_id.has_value()) {
    v8_stat->setLocalId(String::FromUTF8(*webrtc_stat.local_id));
  }
  if (webrtc_stat.remote_timestamp.has_value()) {
    v8_stat->setRemoteTimestamp(*webrtc_stat.remote_timestamp);
  }
  if (webrtc_stat.reports_sent.has_value()) {
    v8_stat->setReportsSent(*webrtc_stat.reports_sent);
  }
  if (webrtc_stat.round_trip_time.has_value()) {
    v8_stat->setRoundTripTime(*webrtc_stat.round_trip_time);
  }
  if (webrtc_stat.total_round_trip_time.has_value()) {
    v8_stat->setTotalRoundTripTime(*webrtc_stat.total_round_trip_time);
  }
  if (webrtc_stat.round_trip_time_measurements.has_value()) {
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
  if (webrtc_stat.track_identifier.has_value()) {
    v8_stat->setTrackIdentifier(
        String::FromUTF8(*webrtc_stat.track_identifier));
  }
  if (webrtc_stat.kind.has_value()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  // RTCAudioSourceStats
  if (webrtc_stat.audio_level.has_value()) {
    v8_stat->setAudioLevel(*webrtc_stat.audio_level);
  }
  if (webrtc_stat.total_audio_energy.has_value()) {
    v8_stat->setTotalAudioEnergy(*webrtc_stat.total_audio_energy);
  }
  if (webrtc_stat.total_samples_duration.has_value()) {
    v8_stat->setTotalSamplesDuration(*webrtc_stat.total_samples_duration);
  }
  if (webrtc_stat.echo_return_loss.has_value()) {
    v8_stat->setEchoReturnLoss(*webrtc_stat.echo_return_loss);
  }
  if (webrtc_stat.echo_return_loss_enhancement.has_value()) {
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
  if (webrtc_stat.track_identifier.has_value()) {
    v8_stat->setTrackIdentifier(
        String::FromUTF8(*webrtc_stat.track_identifier));
  }
  if (webrtc_stat.kind.has_value()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  // RTCVideoSourceStats
  if (webrtc_stat.width.has_value()) {
    v8_stat->setWidth(*webrtc_stat.width);
  }
  if (webrtc_stat.height.has_value()) {
    v8_stat->setHeight(*webrtc_stat.height);
  }
  if (webrtc_stat.frames.has_value()) {
    v8_stat->setFrames(*webrtc_stat.frames);
  }
  if (webrtc_stat.frames_per_second.has_value()) {
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

  if (webrtc_stat.kind.has_value()) {
    v8_stat->setKind(String::FromUTF8(*webrtc_stat.kind));
  }
  if (webrtc_stat.synthesized_samples_duration.has_value()) {
    v8_stat->setSynthesizedSamplesDuration(
        *webrtc_stat.synthesized_samples_duration);
  }
  if (webrtc_stat.synthesized_samples_events.has_value()) {
    v8_stat->setSynthesizedSamplesEvents(base::saturated_cast<uint32_t>(
        *webrtc_stat.synthesized_samples_events));
  }
  if (webrtc_stat.total_samples_duration.has_value()) {
    v8_stat->setTotalSamplesDuration(*webrtc_stat.total_samples_duration);
  }
  if (webrtc_stat.total_playout_delay.has_value()) {
    v8_stat->setTotalPlayoutDelay(*webrtc_stat.total_playout_delay);
  }
  if (webrtc_stat.total_samples_count.has_value()) {
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

  if (webrtc_stat.data_channels_opened.has_value()) {
    v8_stat->setDataChannelsOpened(*webrtc_stat.data_channels_opened);
  }
  if (webrtc_stat.data_channels_closed.has_value()) {
    v8_stat->setDataChannelsClosed(*webrtc_stat.data_channels_closed);
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#dcstats-dict*
RTCDataChannelStats* ToV8Stat(ScriptState* script_state,
                              const webrtc::RTCDataChannelStats& webrtc_stat) {
  RTCDataChannelStats* v8_stat =
      MakeGarbageCollected<RTCDataChannelStats>(script_state->GetIsolate());

  if (webrtc_stat.label.has_value()) {
    v8_stat->setLabel(String::FromUTF8(*webrtc_stat.label));
  }
  if (webrtc_stat.protocol.has_value()) {
    v8_stat->setProtocol(String::FromUTF8(*webrtc_stat.protocol));
  }
  if (webrtc_stat.data_channel_identifier.has_value()) {
    v8_stat->setDataChannelIdentifier(*webrtc_stat.data_channel_identifier);
  }
  if (webrtc_stat.state.has_value()) {
    v8_stat->setState(String::FromUTF8(*webrtc_stat.state));
  }
  if (webrtc_stat.messages_sent.has_value()) {
    v8_stat->setMessagesSent(*webrtc_stat.messages_sent);
  }
  if (webrtc_stat.bytes_sent.has_value()) {
    v8_stat->setBytesSent(*webrtc_stat.bytes_sent);
  }
  if (webrtc_stat.messages_received.has_value()) {
    v8_stat->setMessagesReceived(*webrtc_stat.messages_received);
  }
  if (webrtc_stat.bytes_received.has_value()) {
    v8_stat->setBytesReceived(*webrtc_stat.bytes_received);
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#transportstats-dict*
RTCTransportStats* ToV8Stat(ScriptState* script_state,
                            const webrtc::RTCTransportStats& webrtc_stat) {
  RTCTransportStats* v8_stat =
      MakeGarbageCollected<RTCTransportStats>(script_state->GetIsolate());

  if (webrtc_stat.packets_sent.has_value()) {
    v8_stat->setPacketsSent(*webrtc_stat.packets_sent);
  }
  if (webrtc_stat.packets_received.has_value()) {
    v8_stat->setPacketsReceived(*webrtc_stat.packets_received);
  }
  if (webrtc_stat.bytes_sent.has_value()) {
    v8_stat->setBytesSent(*webrtc_stat.bytes_sent);
  }
  if (webrtc_stat.bytes_received.has_value()) {
    v8_stat->setBytesReceived(*webrtc_stat.bytes_received);
  }
  if (webrtc_stat.ice_role.has_value()) {
    v8_stat->setIceRole(String::FromUTF8(*webrtc_stat.ice_role));
  }
  if (webrtc_stat.ice_local_username_fragment.has_value()) {
    v8_stat->setIceLocalUsernameFragment(
        String::FromUTF8(*webrtc_stat.ice_local_username_fragment));
  }
  if (webrtc_stat.dtls_state.has_value()) {
    v8_stat->setDtlsState(String::FromUTF8(*webrtc_stat.dtls_state));
  }
  if (webrtc_stat.ice_state.has_value()) {
    v8_stat->setIceState(String::FromUTF8(*webrtc_stat.ice_state));
  }
  if (webrtc_stat.selected_candidate_pair_id.has_value()) {
    v8_stat->setSelectedCandidatePairId(
        String::FromUTF8(*webrtc_stat.selected_candidate_pair_id));
  }
  if (webrtc_stat.local_certificate_id.has_value()) {
    v8_stat->setLocalCertificateId(
        String::FromUTF8(*webrtc_stat.local_certificate_id));
  }
  if (webrtc_stat.remote_certificate_id.has_value()) {
    v8_stat->setRemoteCertificateId(
        String::FromUTF8(*webrtc_stat.remote_certificate_id));
  }
  if (webrtc_stat.tls_version.has_value()) {
    v8_stat->setTlsVersion(String::FromUTF8(*webrtc_stat.tls_version));
  }
  if (webrtc_stat.dtls_cipher.has_value()) {
    v8_stat->setDtlsCipher(String::FromUTF8(*webrtc_stat.dtls_cipher));
  }
  if (webrtc_stat.dtls_role.has_value()) {
    v8_stat->setDtlsRole(String::FromUTF8(*webrtc_stat.dtls_role));
  }
  if (webrtc_stat.srtp_cipher.has_value()) {
    v8_stat->setSrtpCipher(String::FromUTF8(*webrtc_stat.srtp_cipher));
  }
  if (webrtc_stat.selected_candidate_pair_changes.has_value()) {
    v8_stat->setSelectedCandidatePairChanges(
        *webrtc_stat.selected_candidate_pair_changes);
  }
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtctransportstats-rtcptransportstatsid
  if (webrtc_stat.rtcp_transport_stats_id.has_value()) {
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
  if (webrtc_stat.transport_id.has_value()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.address.has_value()) {
    v8_stat->setAddress(String::FromUTF8(*webrtc_stat.address));
  }
  if (webrtc_stat.port.has_value()) {
    v8_stat->setPort(*webrtc_stat.port);
  }
  if (webrtc_stat.protocol.has_value()) {
    v8_stat->setProtocol(String::FromUTF8(*webrtc_stat.protocol));
  }
  if (webrtc_stat.candidate_type.has_value()) {
    v8_stat->setCandidateType(String::FromUTF8(*webrtc_stat.candidate_type));
  }
  if (webrtc_stat.priority.has_value()) {
    v8_stat->setPriority(*webrtc_stat.priority);
  }
  if (webrtc_stat.url.has_value()) {
    v8_stat->setUrl(String::FromUTF8(*webrtc_stat.url));
  }
  if (webrtc_stat.relay_protocol.has_value()) {
    v8_stat->setRelayProtocol(String::FromUTF8(*webrtc_stat.relay_protocol));
  }
  if (webrtc_stat.foundation.has_value()) {
    v8_stat->setFoundation(String::FromUTF8(*webrtc_stat.foundation));
  }
  if (webrtc_stat.related_address.has_value()) {
    v8_stat->setRelatedAddress(String::FromUTF8(*webrtc_stat.related_address));
  }
  if (webrtc_stat.related_port.has_value()) {
    v8_stat->setRelatedPort(*webrtc_stat.related_port);
  }
  if (webrtc_stat.username_fragment.has_value()) {
    v8_stat->setUsernameFragment(
        String::FromUTF8(*webrtc_stat.username_fragment));
  }
  if (webrtc_stat.tcp_type.has_value()) {
    v8_stat->setTcpType(String::FromUTF8(*webrtc_stat.tcp_type));
  }
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcicecandidatestats-networktype
  // Note: additional work needed to reach consensus on the privacy model.
  if (webrtc_stat.network_type.has_value()) {
    v8_stat->setNetworkType(String::FromUTF8(*webrtc_stat.network_type));
  }
  // Non-standard and obsolete stats.
  if (webrtc_stat.is_remote.has_value()) {
    v8_stat->setIsRemote(*webrtc_stat.is_remote);
  }
  if (webrtc_stat.ip.has_value()) {
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
  if (webrtc_stat.transport_id.has_value()) {
    v8_stat->setTransportId(String::FromUTF8(*webrtc_stat.transport_id));
  }
  if (webrtc_stat.local_candidate_id.has_value()) {
    v8_stat->setLocalCandidateId(
        String::FromUTF8(*webrtc_stat.local_candidate_id));
  }
  if (webrtc_stat.remote_candidate_id.has_value()) {
    v8_stat->setRemoteCandidateId(
        String::FromUTF8(*webrtc_stat.remote_candidate_id));
  }
  if (webrtc_stat.state.has_value()) {
    v8_stat->setState(String::FromUTF8(*webrtc_stat.state));
  }
  if (webrtc_stat.nominated.has_value()) {
    v8_stat->setNominated(*webrtc_stat.nominated);
  }
  if (webrtc_stat.packets_sent.has_value()) {
    v8_stat->setPacketsSent(*webrtc_stat.packets_sent);
  }
  if (webrtc_stat.packets_received.has_value()) {
    v8_stat->setPacketsReceived(*webrtc_stat.packets_received);
  }
  if (webrtc_stat.bytes_sent.has_value()) {
    v8_stat->setBytesSent(*webrtc_stat.bytes_sent);
  }
  if (webrtc_stat.bytes_received.has_value()) {
    v8_stat->setBytesReceived(*webrtc_stat.bytes_received);
  }
  if (webrtc_stat.last_packet_sent_timestamp.has_value()) {
    v8_stat->setLastPacketSentTimestamp(
        *webrtc_stat.last_packet_sent_timestamp);
  }
  if (webrtc_stat.last_packet_received_timestamp.has_value()) {
    v8_stat->setLastPacketReceivedTimestamp(
        *webrtc_stat.last_packet_received_timestamp);
  }
  if (webrtc_stat.total_round_trip_time.has_value()) {
    v8_stat->setTotalRoundTripTime(*webrtc_stat.total_round_trip_time);
  }
  if (webrtc_stat.current_round_trip_time.has_value()) {
    v8_stat->setCurrentRoundTripTime(*webrtc_stat.current_round_trip_time);
  }
  if (webrtc_stat.available_outgoing_bitrate.has_value()) {
    v8_stat->setAvailableOutgoingBitrate(
        *webrtc_stat.available_outgoing_bitrate);
  }
  if (webrtc_stat.available_incoming_bitrate.has_value()) {
    v8_stat->setAvailableIncomingBitrate(
        *webrtc_stat.available_incoming_bitrate);
  }
  if (webrtc_stat.requests_received.has_value()) {
    v8_stat->setRequestsReceived(*webrtc_stat.requests_received);
  }
  if (webrtc_stat.requests_sent.has_value()) {
    v8_stat->setRequestsSent(*webrtc_stat.requests_sent);
  }
  if (webrtc_stat.responses_received.has_value()) {
    v8_stat->setResponsesReceived(*webrtc_stat.responses_received);
  }
  if (webrtc_stat.responses_sent.has_value()) {
    v8_stat->setResponsesSent(*webrtc_stat.responses_sent);
  }
  if (webrtc_stat.consent_requests_sent.has_value()) {
    v8_stat->setConsentRequestsSent(*webrtc_stat.consent_requests_sent);
  }
  if (webrtc_stat.packets_discarded_on_send.has_value()) {
    v8_stat->setPacketsDiscardedOnSend(
        base::saturated_cast<uint32_t>(*webrtc_stat.packets_discarded_on_send));
  }
  if (webrtc_stat.bytes_discarded_on_send.has_value()) {
    v8_stat->setBytesDiscardedOnSend(*webrtc_stat.bytes_discarded_on_send);
  }
  // Non-standard and obsolete stats.
  if (webrtc_stat.writable.has_value()) {
    v8_stat->setWritable(*webrtc_stat.writable);
  }
  if (webrtc_stat.priority.has_value()) {
    v8_stat->setPriority(*webrtc_stat.priority);
  }
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#certificatestats-dict*
RTCCertificateStats* ToV8Stat(ScriptState* script_state,
                              const webrtc::RTCCertificateStats& webrtc_stat) {
  RTCCertificateStats* v8_stat =
      MakeGarbageCollected<RTCCertificateStats>(script_state->GetIsolate());
  if (webrtc_stat.fingerprint.has_value()) {
    v8_stat->setFingerprint(String::FromUTF8(*webrtc_stat.fingerprint));
  }
  if (webrtc_stat.fingerprint_algorithm.has_value()) {
    v8_stat->setFingerprintAlgorithm(
        String::FromUTF8(*webrtc_stat.fingerprint_algorithm));
  }
  if (webrtc_stat.base64_certificate.has_value()) {
    v8_stat->setBase64Certificate(
        String::FromUTF8(*webrtc_stat.base64_certificate));
  }
  if (webrtc_stat.issuer_certificate_id.has_value()) {
    v8_stat->setIssuerCertificateId(
        String::FromUTF8(*webrtc_stat.issuer_certificate_id));
  }
  return v8_stat;
}

RTCStats* RTCStatsToIDL(ScriptState* script_state,
                        const webrtc::RTCStats& stat,
                        bool expose_hardware_caps) {
  RTCStats* v8_stats = nullptr;
  if (strcmp(stat.type(), "codec") == 0) {
    v8_stats = ToV8Stat(script_state, stat.cast_to<webrtc::RTCCodecStats>());
  } else if (strcmp(stat.type(), "inbound-rtp") == 0) {
    v8_stats =
        ToV8Stat(script_state, stat.cast_to<webrtc::RTCInboundRtpStreamStats>(),
                 expose_hardware_caps);
  } else if (strcmp(stat.type(), "outbound-rtp") == 0) {
    v8_stats = ToV8Stat(script_state,
                        stat.cast_to<webrtc::RTCOutboundRtpStreamStats>(),
                        expose_hardware_caps);
  } else if (strcmp(stat.type(), "remote-inbound-rtp") == 0) {
    v8_stats = ToV8Stat(script_state,
                        stat.cast_to<webrtc::RTCRemoteInboundRtpStreamStats>());
  } else if (strcmp(stat.type(), "remote-outbound-rtp") == 0) {
    v8_stats = ToV8Stat(
        script_state, stat.cast_to<webrtc::RTCRemoteOutboundRtpStreamStats>());
  } else if (strcmp(stat.type(), "media-source") == 0) {
    // Type media-source indicates a parent type. The actual stats are based on
    // the kind.
    const auto& media_source =
        static_cast<const webrtc::RTCMediaSourceStats&>(stat);
    DCHECK(media_source.kind.has_value());
    std::string kind = media_source.kind.value_or("");
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
      std::unique_ptr<RTCStatsReportPlatform> report)
      : report_(std::move(report)) {}

  bool FetchNextItem(ScriptState* script_state,
                     String& key,
                     ScriptValue& value,
                     ExceptionState& exception_state) override {
    return FetchNextItemIdl(script_state, key, value, exception_state);
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
      v8_stat = RTCStatsToIDL(script_state, *rtc_stats, expose_hardware_caps);
      if (v8_stat) {
        break;
      }
      rtc_stats = report_->NextStats();
    }
    if (!rtc_stats) {
      return false;
    }
    key = String::FromUTF8(rtc_stats->id());
    value = ScriptValue::From(script_state, v8_stat);
    return true;
  }

 private:
  std::unique_ptr<RTCStatsReportPlatform> report_;
};

}  // namespace

RTCStatsReport::RTCStatsReport(std::unique_ptr<RTCStatsReportPlatform> report)
    : report_(std::move(report)) {}

uint32_t RTCStatsReport::size() const {
  return base::saturated_cast<uint32_t>(report_->Size());
}

PairSyncIterable<RTCStatsReport>::IterationSource*
RTCStatsReport::CreateIterationSource(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<RTCStatsReportIterationSource>(
      report_->CopyHandle());
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
      script_state, *stats, ExposeHardwareCapabilityStats(script_state));
  if (!v8_stats) {
    return false;
  }
  value = ScriptValue::From(script_state, v8_stats);
  return true;
}

bool RTCStatsReport::GetMapEntry(ScriptState* script_state,
                                 const String& key,
                                 ScriptValue& value,
                                 ExceptionState& exception_state) {
  return GetMapEntryIdl(script_state, key, value, exception_state);
}

}  // namespace blink
