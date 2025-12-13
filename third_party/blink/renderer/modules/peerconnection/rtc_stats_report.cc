// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/notimplemented.h"
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
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"

namespace blink {

namespace {

template <typename T>
T StatsConversionHelper(const T& value) {
  return value;
}
String StatsConversionHelper(const std::string& value) {
  return String::FromUTF8(value);
}

// Macro to reduce the transformation between WebRTC and v8 values
// to a single line. Arguments are the webrtc stat and the equivalent
// v8 setter function. Uses StatsConversionHelper to specialize for
// std::string -> String::FromUTF8.
#define SET_STAT(webrtc_stat, v8_setter)            \
  if (webrtc_stat.has_value()) {                    \
    v8_setter(StatsConversionHelper(*webrtc_stat)); \
  }

#define SET_STAT_ENUM(webrtc_stat, v8_setter, V8EnumType)                 \
  if (webrtc_stat.has_value()) {                                          \
    v8_setter(                                                            \
        V8EnumType::Create(StatsConversionHelper(*webrtc_stat)).value()); \
  }

template <typename T>
v8::Local<v8::Value> HashMapToValue(ScriptState* script_state,
                                    HashMap<String, T>&& map) {
  V8ObjectBuilder builder(script_state);
  for (auto& it : map) {
    builder.Add(it.key, it.value);
  }
  v8::Local<v8::Object> v8_object = builder.V8Object();
  if (v8_object.IsEmpty()) {
    NOTREACHED();
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
  SET_STAT(webrtc_stat.transport_id, v8_codec->setTransportId)
  SET_STAT(webrtc_stat.payload_type, v8_codec->setPayloadType);
  SET_STAT(webrtc_stat.channels, v8_codec->setChannels);
  SET_STAT(webrtc_stat.mime_type, v8_codec->setMimeType);
  SET_STAT(webrtc_stat.clock_rate, v8_codec->setClockRate);
  SET_STAT(webrtc_stat.sdp_fmtp_line, v8_codec->setSdpFmtpLine);
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
  SET_STAT(webrtc_stat.ssrc, v8_stat->setSsrc);
  SET_STAT(webrtc_stat.kind, v8_stat->setKind);
  // mediaType is a legacy alias for kind.
  SET_STAT(webrtc_stat.kind, v8_stat->setMediaType);
  SET_STAT(webrtc_stat.transport_id, v8_stat->setTransportId);
  SET_STAT(webrtc_stat.codec_id, v8_stat->setCodecId);
  // RTCReceivedRtpStreamStats
  SET_STAT(webrtc_stat.packets_lost, v8_stat->setPacketsLost);
  SET_STAT(webrtc_stat.jitter, v8_stat->setJitter);
  SET_STAT(webrtc_stat.packets_received_with_ect1,
           v8_stat->setPacketsReceivedWithEct1);
  SET_STAT(webrtc_stat.packets_received_with_ce,
           v8_stat->setPacketsReceivedWithCe);
  SET_STAT(webrtc_stat.packets_reported_as_lost,
           v8_stat->setPacketsReportedAsLost);
  SET_STAT(webrtc_stat.packets_reported_as_lost_but_recovered,
           v8_stat->setPacketsReportedAsLostButRecovered);
  // RTCInboundRtpStreamStats
  SET_STAT(webrtc_stat.track_identifier, v8_stat->setTrackIdentifier);
  SET_STAT(webrtc_stat.mid, v8_stat->setMid);
  SET_STAT(webrtc_stat.remote_id, v8_stat->setRemoteId);
  SET_STAT(webrtc_stat.frames_decoded, v8_stat->setFramesDecoded);
  SET_STAT(webrtc_stat.key_frames_decoded, v8_stat->setKeyFramesDecoded);
  SET_STAT(webrtc_stat.frames_dropped, v8_stat->setFramesDropped);
  SET_STAT(webrtc_stat.frame_width, v8_stat->setFrameWidth);
  SET_STAT(webrtc_stat.frame_height, v8_stat->setFrameHeight);
  SET_STAT(webrtc_stat.frames_per_second, v8_stat->setFramesPerSecond);
  SET_STAT(webrtc_stat.qp_sum, v8_stat->setQpSum);
  SET_STAT(webrtc_stat.total_corruption_probability,
           v8_stat->setTotalCorruptionProbability);
  SET_STAT(webrtc_stat.total_squared_corruption_probability,
           v8_stat->setTotalSquaredCorruptionProbability);
  SET_STAT(webrtc_stat.corruption_measurements,
           v8_stat->setCorruptionMeasurements);
  SET_STAT(webrtc_stat.total_decode_time, v8_stat->setTotalDecodeTime);
  SET_STAT(webrtc_stat.total_inter_frame_delay,
           v8_stat->setTotalInterFrameDelay);
  SET_STAT(webrtc_stat.total_squared_inter_frame_delay,
           v8_stat->setTotalSquaredInterFrameDelay);
  SET_STAT(webrtc_stat.pause_count, v8_stat->setPauseCount);
  SET_STAT(webrtc_stat.total_pauses_duration, v8_stat->setTotalPausesDuration);
  SET_STAT(webrtc_stat.freeze_count, v8_stat->setFreezeCount);
  SET_STAT(webrtc_stat.total_freezes_duration,
           v8_stat->setTotalFreezesDuration);
  SET_STAT(webrtc_stat.last_packet_received_timestamp,
           v8_stat->setLastPacketReceivedTimestamp);
  SET_STAT(webrtc_stat.header_bytes_received, v8_stat->setHeaderBytesReceived);
  SET_STAT(webrtc_stat.packets_discarded, v8_stat->setPacketsDiscarded);
  SET_STAT(webrtc_stat.packets_received, v8_stat->setPacketsReceived);
  SET_STAT(webrtc_stat.fec_packets_received, v8_stat->setFecPacketsReceived);
  SET_STAT(webrtc_stat.fec_packets_discarded, v8_stat->setFecPacketsDiscarded);
  SET_STAT(webrtc_stat.fec_bytes_received, v8_stat->setFecBytesReceived);
  SET_STAT(webrtc_stat.fec_ssrc, v8_stat->setFecSsrc);
  SET_STAT(webrtc_stat.bytes_received, v8_stat->setBytesReceived);
  SET_STAT(webrtc_stat.nack_count, v8_stat->setNackCount);
  SET_STAT(webrtc_stat.fir_count, v8_stat->setFirCount);
  SET_STAT(webrtc_stat.pli_count, v8_stat->setPliCount);
  SET_STAT(webrtc_stat.total_processing_delay,
           v8_stat->setTotalProcessingDelay);
  SET_STAT(webrtc_stat.estimated_playout_timestamp,
           v8_stat->setEstimatedPlayoutTimestamp);
  SET_STAT(webrtc_stat.jitter_buffer_delay, v8_stat->setJitterBufferDelay);
  SET_STAT(webrtc_stat.jitter_buffer_target_delay,
           v8_stat->setJitterBufferTargetDelay);
  SET_STAT(webrtc_stat.jitter_buffer_emitted_count,
           v8_stat->setJitterBufferEmittedCount);
  SET_STAT(webrtc_stat.jitter_buffer_minimum_delay,
           v8_stat->setJitterBufferMinimumDelay);
  SET_STAT(webrtc_stat.total_samples_received,
           v8_stat->setTotalSamplesReceived);
  SET_STAT(webrtc_stat.concealed_samples, v8_stat->setConcealedSamples);
  SET_STAT(webrtc_stat.silent_concealed_samples,
           v8_stat->setSilentConcealedSamples);
  SET_STAT(webrtc_stat.concealment_events, v8_stat->setConcealmentEvents);
  SET_STAT(webrtc_stat.inserted_samples_for_deceleration,
           v8_stat->setInsertedSamplesForDeceleration);
  SET_STAT(webrtc_stat.removed_samples_for_acceleration,
           v8_stat->setRemovedSamplesForAcceleration);
  SET_STAT(webrtc_stat.audio_level, v8_stat->setAudioLevel);
  SET_STAT(webrtc_stat.total_audio_energy, v8_stat->setTotalAudioEnergy);
  SET_STAT(webrtc_stat.total_samples_duration,
           v8_stat->setTotalSamplesDuration);
  SET_STAT(webrtc_stat.frames_received, v8_stat->setFramesReceived);
  SET_STAT(webrtc_stat.playout_id, v8_stat->setPlayoutId);
  SET_STAT(webrtc_stat.frames_assembled_from_multiple_packets,
           v8_stat->setFramesAssembledFromMultiplePackets);
  SET_STAT(webrtc_stat.total_assembly_time, v8_stat->setTotalAssemblyTime);
  if (expose_hardware_caps) {
    SET_STAT(webrtc_stat.power_efficient_decoder,
             v8_stat->setPowerEfficientDecoder);
    SET_STAT(webrtc_stat.decoder_implementation,
             v8_stat->setDecoderImplementation);
  }
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcinboundrtpstreamstats-contenttype
  SET_STAT(webrtc_stat.content_type, v8_stat->setContentType);
  // https://github.com/w3c/webrtc-provisional-stats/issues/40
  SET_STAT(webrtc_stat.goog_timing_frame_info, v8_stat->setGoogTimingFrameInfo);
  SET_STAT(webrtc_stat.retransmitted_packets_received,
           v8_stat->setRetransmittedPacketsReceived);
  SET_STAT(webrtc_stat.retransmitted_bytes_received,
           v8_stat->setRetransmittedBytesReceived);
  SET_STAT(webrtc_stat.rtx_ssrc, v8_stat->setRtxSsrc);
  return v8_stat;
}

RTCRemoteInboundRtpStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCRemoteInboundRtpStreamStats& webrtc_stat) {
  RTCRemoteInboundRtpStreamStats* v8_stat =
      MakeGarbageCollected<RTCRemoteInboundRtpStreamStats>(
          script_state->GetIsolate());
  // RTCRtpStreamStats
  SET_STAT(webrtc_stat.ssrc, v8_stat->setSsrc);
  SET_STAT(webrtc_stat.kind, v8_stat->setKind);
  // mediaType is a legacy alias for kind.
  SET_STAT(webrtc_stat.kind, v8_stat->setMediaType);
  SET_STAT(webrtc_stat.transport_id, v8_stat->setTransportId);
  SET_STAT(webrtc_stat.codec_id, v8_stat->setCodecId);
  // RTCReceivedRtpStreamStats
  SET_STAT(webrtc_stat.packets_lost, v8_stat->setPacketsLost);
  SET_STAT(webrtc_stat.jitter, v8_stat->setJitter);
  // RTCRemoteInboundRtpStreamStats
  SET_STAT(webrtc_stat.local_id, v8_stat->setLocalId);
  SET_STAT(webrtc_stat.round_trip_time, v8_stat->setRoundTripTime);
  SET_STAT(webrtc_stat.total_round_trip_time, v8_stat->setTotalRoundTripTime);
  SET_STAT(webrtc_stat.fraction_lost, v8_stat->setFractionLost);
  SET_STAT(webrtc_stat.round_trip_time_measurements,
           v8_stat->setRoundTripTimeMeasurements);
  SET_STAT(webrtc_stat.packets_with_bleached_ect1_marking,
           v8_stat->setPacketsWithBleachedEct1Marking);
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
  SET_STAT(webrtc_stat.ssrc, v8_stat->setSsrc);
  SET_STAT(webrtc_stat.kind, v8_stat->setKind);
  // mediaType is a legacy alias for kind.
  SET_STAT(webrtc_stat.kind, v8_stat->setMediaType);
  SET_STAT(webrtc_stat.transport_id, v8_stat->setTransportId);
  SET_STAT(webrtc_stat.codec_id, v8_stat->setCodecId);
  // RTCSentRtpStreamStats
  SET_STAT(webrtc_stat.packets_sent, v8_stat->setPacketsSent);
  SET_STAT(webrtc_stat.bytes_sent, v8_stat->setBytesSent);
  // RTCOutboundRtpStreamStats
  SET_STAT(webrtc_stat.mid, v8_stat->setMid);
  SET_STAT(webrtc_stat.media_source_id, v8_stat->setMediaSourceId);
  SET_STAT(webrtc_stat.remote_id, v8_stat->setRemoteId);
  SET_STAT(webrtc_stat.rid, v8_stat->setRid);
  SET_STAT(webrtc_stat.encoding_index, v8_stat->setEncodingIndex);
  SET_STAT(webrtc_stat.header_bytes_sent, v8_stat->setHeaderBytesSent);
  SET_STAT(webrtc_stat.retransmitted_packets_sent,
           v8_stat->setRetransmittedPacketsSent);
  SET_STAT(webrtc_stat.retransmitted_bytes_sent,
           v8_stat->setRetransmittedBytesSent);
  SET_STAT(webrtc_stat.rtx_ssrc, v8_stat->setRtxSsrc);
  SET_STAT(webrtc_stat.target_bitrate, v8_stat->setTargetBitrate);
  SET_STAT(webrtc_stat.total_encoded_bytes_target,
           v8_stat->setTotalEncodedBytesTarget);
  SET_STAT(webrtc_stat.frame_width, v8_stat->setFrameWidth);
  SET_STAT(webrtc_stat.frame_height, v8_stat->setFrameHeight);
  SET_STAT(webrtc_stat.frames_per_second, v8_stat->setFramesPerSecond);
  SET_STAT(webrtc_stat.frames_sent, v8_stat->setFramesSent);
  SET_STAT(webrtc_stat.huge_frames_sent, v8_stat->setHugeFramesSent);
  SET_STAT(webrtc_stat.frames_encoded, v8_stat->setFramesEncoded);
  SET_STAT(webrtc_stat.key_frames_encoded, v8_stat->setKeyFramesEncoded);
  SET_STAT(webrtc_stat.qp_sum, v8_stat->setQpSum);
  if (expose_hardware_caps && webrtc_stat.psnr_sum.has_value()) {
    Vector<std::pair<String, double>> psnr_sum;
    for (const auto& [key, value] : *webrtc_stat.psnr_sum) {
      psnr_sum.emplace_back(String::FromUTF8(key), value);
    }
    v8_stat->setPsnrSum(std::move(psnr_sum));
  }
  SET_STAT(webrtc_stat.total_encode_time, v8_stat->setTotalEncodeTime);
  SET_STAT(webrtc_stat.total_packet_send_delay,
           v8_stat->setTotalPacketSendDelay);
  SET_STAT_ENUM(webrtc_stat.quality_limitation_reason,
                v8_stat->setQualityLimitationReason,
                V8RTCQualityLimitationReason);
  if (webrtc_stat.quality_limitation_durations.has_value()) {
    Vector<std::pair<String, double>> quality_durations;
    for (const auto& [key, value] : *webrtc_stat.quality_limitation_durations) {
      quality_durations.emplace_back(String::FromUTF8(key), value);
    }
    v8_stat->setQualityLimitationDurations(std::move(quality_durations));
  }
  SET_STAT(webrtc_stat.quality_limitation_resolution_changes,
           v8_stat->setQualityLimitationResolutionChanges);
  SET_STAT(webrtc_stat.nack_count, v8_stat->setNackCount);
  SET_STAT(webrtc_stat.fir_count, v8_stat->setFirCount);
  SET_STAT(webrtc_stat.pli_count, v8_stat->setPliCount);
  SET_STAT(webrtc_stat.active, v8_stat->setActive);
  SET_STAT(webrtc_stat.scalability_mode, v8_stat->setScalabilityMode);
  if (expose_hardware_caps) {
    SET_STAT(webrtc_stat.encoder_implementation,
             v8_stat->setEncoderImplementation);
    SET_STAT(webrtc_stat.power_efficient_encoder,
             v8_stat->setPowerEfficientEncoder);
  }
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcoutboundrtpstreamstats-contenttype
  SET_STAT(webrtc_stat.content_type, v8_stat->setContentType);

  SET_STAT(webrtc_stat.packets_sent_with_ect1, v8_stat->setPacketsSentWithEct1);
  return v8_stat;
}

RTCRemoteOutboundRtpStreamStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCRemoteOutboundRtpStreamStats& webrtc_stat) {
  RTCRemoteOutboundRtpStreamStats* v8_stat =
      MakeGarbageCollected<RTCRemoteOutboundRtpStreamStats>(
          script_state->GetIsolate());
  // RTCRtpStreamStats
  SET_STAT(webrtc_stat.ssrc, v8_stat->setSsrc);
  SET_STAT(webrtc_stat.kind, v8_stat->setKind);
  // mediaType is a legacy alias for kind.
  SET_STAT(webrtc_stat.kind, v8_stat->setMediaType);
  SET_STAT(webrtc_stat.transport_id, v8_stat->setTransportId);
  SET_STAT(webrtc_stat.codec_id, v8_stat->setCodecId);
  // RTCSendRtpStreamStats
  SET_STAT(webrtc_stat.packets_sent, v8_stat->setPacketsSent);
  SET_STAT(webrtc_stat.bytes_sent, v8_stat->setBytesSent);
  // RTCRemoteOutboundRtpStreamStats
  SET_STAT(webrtc_stat.local_id, v8_stat->setLocalId);
  SET_STAT(webrtc_stat.remote_timestamp, v8_stat->setRemoteTimestamp);
  SET_STAT(webrtc_stat.reports_sent, v8_stat->setReportsSent);
  SET_STAT(webrtc_stat.round_trip_time, v8_stat->setRoundTripTime);
  SET_STAT(webrtc_stat.total_round_trip_time, v8_stat->setTotalRoundTripTime);
  SET_STAT(webrtc_stat.round_trip_time_measurements,
           v8_stat->setRoundTripTimeMeasurements);
  return v8_stat;
}

RTCAudioSourceStats* ToV8Stat(ScriptState* script_state,
                              const webrtc::RTCAudioSourceStats& webrtc_stat) {
  RTCAudioSourceStats* v8_stat =
      MakeGarbageCollected<RTCAudioSourceStats>(script_state->GetIsolate());
  // RTCMediaSourceStats
  SET_STAT(webrtc_stat.track_identifier, v8_stat->setTrackIdentifier);
  SET_STAT(webrtc_stat.kind, v8_stat->setKind);
  // RTCAudioSourceStats
  SET_STAT(webrtc_stat.audio_level, v8_stat->setAudioLevel);
  SET_STAT(webrtc_stat.total_audio_energy, v8_stat->setTotalAudioEnergy);
  SET_STAT(webrtc_stat.total_samples_duration,
           v8_stat->setTotalSamplesDuration);
  SET_STAT(webrtc_stat.echo_return_loss, v8_stat->setEchoReturnLoss);
  SET_STAT(webrtc_stat.echo_return_loss_enhancement,
           v8_stat->setEchoReturnLossEnhancement);
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#videosourcestats-dict*
RTCVideoSourceStats* ToV8Stat(ScriptState* script_state,
                              const webrtc::RTCVideoSourceStats& webrtc_stat) {
  RTCVideoSourceStats* v8_stat =
      MakeGarbageCollected<RTCVideoSourceStats>(script_state->GetIsolate());
  // RTCMediaSourceStats
  SET_STAT(webrtc_stat.track_identifier, v8_stat->setTrackIdentifier);
  SET_STAT(webrtc_stat.kind, v8_stat->setKind);
  // RTCVideoSourceStats
  SET_STAT(webrtc_stat.width, v8_stat->setWidth);
  SET_STAT(webrtc_stat.height, v8_stat->setHeight);
  SET_STAT(webrtc_stat.frames, v8_stat->setFrames);
  SET_STAT(webrtc_stat.frames_per_second, v8_stat->setFramesPerSecond);
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#playoutstats-dict*
RTCAudioPlayoutStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCAudioPlayoutStats& webrtc_stat) {
  RTCAudioPlayoutStats* v8_stat =
      MakeGarbageCollected<RTCAudioPlayoutStats>(script_state->GetIsolate());

  SET_STAT(webrtc_stat.kind, v8_stat->setKind);
  SET_STAT(webrtc_stat.synthesized_samples_duration,
           v8_stat->setSynthesizedSamplesDuration);
  if (webrtc_stat.synthesized_samples_events.has_value()) {
    v8_stat->setSynthesizedSamplesEvents(base::saturated_cast<uint32_t>(
        *webrtc_stat.synthesized_samples_events));
  }
  SET_STAT(webrtc_stat.synthesized_samples_events,
           v8_stat->setSynthesizedSamplesEvents);
  SET_STAT(webrtc_stat.total_samples_duration,
           v8_stat->setTotalSamplesDuration);
  SET_STAT(webrtc_stat.total_playout_delay, v8_stat->setTotalPlayoutDelay);
  SET_STAT(webrtc_stat.total_samples_count, v8_stat->setTotalSamplesCount);
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#pcstats-dict*
RTCPeerConnectionStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCPeerConnectionStats& webrtc_stat) {
  RTCPeerConnectionStats* v8_stat =
      MakeGarbageCollected<RTCPeerConnectionStats>(script_state->GetIsolate());

  SET_STAT(webrtc_stat.data_channels_opened, v8_stat->setDataChannelsOpened);
  SET_STAT(webrtc_stat.data_channels_closed, v8_stat->setDataChannelsClosed);
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#dcstats-dict*
RTCDataChannelStats* ToV8Stat(ScriptState* script_state,
                              const webrtc::RTCDataChannelStats& webrtc_stat) {
  RTCDataChannelStats* v8_stat =
      MakeGarbageCollected<RTCDataChannelStats>(script_state->GetIsolate());

  SET_STAT(webrtc_stat.label, v8_stat->setLabel);
  SET_STAT(webrtc_stat.protocol, v8_stat->setProtocol);
  SET_STAT(webrtc_stat.data_channel_identifier,
           v8_stat->setDataChannelIdentifier);
  SET_STAT_ENUM(webrtc_stat.state, v8_stat->setState, V8RTCDataChannelState);
  SET_STAT(webrtc_stat.messages_sent, v8_stat->setMessagesSent);
  SET_STAT(webrtc_stat.bytes_sent, v8_stat->setBytesSent);
  SET_STAT(webrtc_stat.messages_received, v8_stat->setMessagesReceived);
  SET_STAT(webrtc_stat.bytes_received, v8_stat->setBytesReceived);
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#transportstats-dict*
RTCTransportStats* ToV8Stat(ScriptState* script_state,
                            const webrtc::RTCTransportStats& webrtc_stat) {
  RTCTransportStats* v8_stat =
      MakeGarbageCollected<RTCTransportStats>(script_state->GetIsolate());

  SET_STAT(webrtc_stat.packets_sent, v8_stat->setPacketsSent);
  SET_STAT(webrtc_stat.packets_received, v8_stat->setPacketsReceived);
  SET_STAT(webrtc_stat.bytes_sent, v8_stat->setBytesSent);
  SET_STAT(webrtc_stat.bytes_received, v8_stat->setBytesReceived);
  SET_STAT_ENUM(webrtc_stat.ice_role, v8_stat->setIceRole, V8RTCIceRole);
  SET_STAT(webrtc_stat.ice_local_username_fragment,
           v8_stat->setIceLocalUsernameFragment);
  SET_STAT_ENUM(webrtc_stat.dtls_state, v8_stat->setDtlsState,
                V8RTCDtlsTransportState);
  SET_STAT_ENUM(webrtc_stat.ice_state, v8_stat->setIceState,
                V8RTCIceTransportState);
  SET_STAT(webrtc_stat.selected_candidate_pair_id,
           v8_stat->setSelectedCandidatePairId);
  SET_STAT(webrtc_stat.local_certificate_id, v8_stat->setLocalCertificateId);
  SET_STAT(webrtc_stat.remote_certificate_id, v8_stat->setRemoteCertificateId);
  SET_STAT(webrtc_stat.tls_version, v8_stat->setTlsVersion);
  SET_STAT(webrtc_stat.dtls_cipher, v8_stat->setDtlsCipher);
  SET_STAT_ENUM(webrtc_stat.dtls_role, v8_stat->setDtlsRole, V8RTCDtlsRole);
  SET_STAT(webrtc_stat.srtp_cipher, v8_stat->setSrtpCipher);
  SET_STAT(webrtc_stat.selected_candidate_pair_changes,
           v8_stat->setSelectedCandidatePairChanges);
  SET_STAT(webrtc_stat.ccfb_messages_received,
           v8_stat->setCcbfMessagesReceived);
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtctransportstats-rtcptransportstatsid
  SET_STAT(webrtc_stat.rtcp_transport_stats_id,
           v8_stat->setRtcpTransportStatsId);
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#icecandidate-dict*
RTCIceCandidateStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCIceCandidateStats& webrtc_stat) {
  RTCIceCandidateStats* v8_stat =
      MakeGarbageCollected<RTCIceCandidateStats>(script_state->GetIsolate());
  SET_STAT(webrtc_stat.transport_id, v8_stat->setTransportId);
  SET_STAT(webrtc_stat.address, v8_stat->setAddress);
  SET_STAT(webrtc_stat.port, v8_stat->setPort);
  SET_STAT(webrtc_stat.protocol, v8_stat->setProtocol);
  SET_STAT_ENUM(webrtc_stat.candidate_type, v8_stat->setCandidateType,
                V8RTCIceCandidateType);
  SET_STAT(webrtc_stat.priority, v8_stat->setPriority);
  SET_STAT(webrtc_stat.url, v8_stat->setUrl);
  SET_STAT_ENUM(webrtc_stat.relay_protocol, v8_stat->setRelayProtocol,
                V8RTCIceServerTransportProtocol);
  SET_STAT(webrtc_stat.foundation, v8_stat->setFoundation);
  SET_STAT(webrtc_stat.related_address, v8_stat->setRelatedAddress);
  SET_STAT(webrtc_stat.related_port, v8_stat->setRelatedPort);
  SET_STAT(webrtc_stat.username_fragment, v8_stat->setUsernameFragment);
  SET_STAT_ENUM(webrtc_stat.tcp_type, v8_stat->setTcpType,
                V8RTCIceTcpCandidateType);
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcicecandidatestats-networktype
  // Note: additional work needed to reach consensus on the privacy model.
  SET_STAT_ENUM(webrtc_stat.network_type, v8_stat->setNetworkType,
                V8RTCNetworkType);
  // Non-standard and obsolete stats.
  SET_STAT(webrtc_stat.is_remote, v8_stat->setIsRemote);
  SET_STAT(webrtc_stat.ip, v8_stat->setIp);
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#candidatepair-dict*
RTCIceCandidatePairStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCIceCandidatePairStats& webrtc_stat) {
  RTCIceCandidatePairStats* v8_stat =
      MakeGarbageCollected<RTCIceCandidatePairStats>(
          script_state->GetIsolate());
  SET_STAT(webrtc_stat.transport_id, v8_stat->setTransportId);
  SET_STAT(webrtc_stat.local_candidate_id, v8_stat->setLocalCandidateId);
  SET_STAT(webrtc_stat.remote_candidate_id, v8_stat->setRemoteCandidateId);
  SET_STAT_ENUM(webrtc_stat.state, v8_stat->setState,
                V8RTCStatsIceCandidatePairState);
  SET_STAT(webrtc_stat.nominated, v8_stat->setNominated);
  SET_STAT(webrtc_stat.packets_sent, v8_stat->setPacketsSent);
  SET_STAT(webrtc_stat.packets_received, v8_stat->setPacketsReceived);
  SET_STAT(webrtc_stat.bytes_sent, v8_stat->setBytesSent);
  SET_STAT(webrtc_stat.bytes_received, v8_stat->setBytesReceived);
  SET_STAT(webrtc_stat.last_packet_sent_timestamp,
           v8_stat->setLastPacketSentTimestamp);
  SET_STAT(webrtc_stat.last_packet_received_timestamp,
           v8_stat->setLastPacketReceivedTimestamp);
  SET_STAT(webrtc_stat.total_round_trip_time, v8_stat->setTotalRoundTripTime);
  SET_STAT(webrtc_stat.current_round_trip_time,
           v8_stat->setCurrentRoundTripTime);
  SET_STAT(webrtc_stat.available_outgoing_bitrate,
           v8_stat->setAvailableOutgoingBitrate);
  SET_STAT(webrtc_stat.available_incoming_bitrate,
           v8_stat->setAvailableIncomingBitrate);
  SET_STAT(webrtc_stat.requests_received, v8_stat->setRequestsReceived);
  SET_STAT(webrtc_stat.requests_sent, v8_stat->setRequestsSent);
  SET_STAT(webrtc_stat.responses_received, v8_stat->setResponsesReceived);
  SET_STAT(webrtc_stat.responses_sent, v8_stat->setResponsesSent);
  SET_STAT(webrtc_stat.consent_requests_sent, v8_stat->setConsentRequestsSent);
  if (webrtc_stat.packets_discarded_on_send.has_value()) {
    v8_stat->setPacketsDiscardedOnSend(
        base::saturated_cast<uint32_t>(*webrtc_stat.packets_discarded_on_send));
  }
  SET_STAT(webrtc_stat.bytes_discarded_on_send,
           v8_stat->setBytesDiscardedOnSend);
  // Non-standard and obsolete stats.
  SET_STAT(webrtc_stat.writable, v8_stat->setWritable);
  SET_STAT(webrtc_stat.priority, v8_stat->setPriority);
  return v8_stat;
}

// https://w3c.github.io/webrtc-stats/#certificatestats-dict*
RTCCertificateStats* ToV8Stat(ScriptState* script_state,
                              const webrtc::RTCCertificateStats& webrtc_stat) {
  RTCCertificateStats* v8_stat =
      MakeGarbageCollected<RTCCertificateStats>(script_state->GetIsolate());
  SET_STAT(webrtc_stat.fingerprint, v8_stat->setFingerprint);
  SET_STAT(webrtc_stat.fingerprint_algorithm, v8_stat->setFingerprintAlgorithm);
  SET_STAT(webrtc_stat.base64_certificate, v8_stat->setBase64Certificate);
  SET_STAT(webrtc_stat.issuer_certificate_id, v8_stat->setIssuerCertificateId);
  return v8_stat;
}

RTCStats* RTCStatsToIDL(ScriptState* script_state,
                        const webrtc::RTCStats& stat,
                        bool expose_hardware_caps) {
  auto v8_stats_type = V8RTCStatsType::Create(String::FromUTF8(stat.type()));
  CHECK(v8_stats_type.has_value());

  RTCStats* v8_stats = nullptr;
  switch (v8_stats_type->AsEnum()) {
    case V8RTCStatsType::Enum::kCodec:
      v8_stats = ToV8Stat(script_state, stat.cast_to<webrtc::RTCCodecStats>());
      break;
    case V8RTCStatsType::Enum::kInboundRtp:
      v8_stats = ToV8Stat(script_state,
                          stat.cast_to<webrtc::RTCInboundRtpStreamStats>(),
                          expose_hardware_caps);
      break;
    case V8RTCStatsType::Enum::kOutboundRtp:
      v8_stats = ToV8Stat(script_state,
                          stat.cast_to<webrtc::RTCOutboundRtpStreamStats>(),
                          expose_hardware_caps);
      break;
    case V8RTCStatsType::Enum::kRemoteInboundRtp:
      v8_stats = ToV8Stat(
          script_state, stat.cast_to<webrtc::RTCRemoteInboundRtpStreamStats>());
      break;
    case V8RTCStatsType::Enum::kRemoteOutboundRtp:
      v8_stats =
          ToV8Stat(script_state,
                   stat.cast_to<webrtc::RTCRemoteOutboundRtpStreamStats>());
      break;
    case V8RTCStatsType::Enum::kMediaSource: {
      // Type media-source indicates a parent type. The actual stats are based
      // on the kind.
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
      break;
    }
    case V8RTCStatsType::Enum::kMediaPlayout:
      v8_stats =
          ToV8Stat(script_state, stat.cast_to<webrtc::RTCAudioPlayoutStats>());
      break;
    case V8RTCStatsType::Enum::kPeerConnection:
      v8_stats = ToV8Stat(script_state,
                          stat.cast_to<webrtc::RTCPeerConnectionStats>());
      break;
    case V8RTCStatsType::Enum::kDataChannel:
      v8_stats =
          ToV8Stat(script_state, stat.cast_to<webrtc::RTCDataChannelStats>());
      break;
    case V8RTCStatsType::Enum::kTransport:
      v8_stats =
          ToV8Stat(script_state, stat.cast_to<webrtc::RTCTransportStats>());
      break;
    case V8RTCStatsType::Enum::kCandidatePair:
      v8_stats = ToV8Stat(script_state,
                          stat.cast_to<webrtc::RTCIceCandidatePairStats>());
      break;
    case V8RTCStatsType::Enum::kLocalCandidate:
      v8_stats = ToV8Stat(script_state,
                          stat.cast_to<webrtc::RTCLocalIceCandidateStats>());
      break;
    case V8RTCStatsType::Enum::kRemoteCandidate:
      v8_stats = ToV8Stat(script_state,
                          stat.cast_to<webrtc::RTCRemoteIceCandidateStats>());
      break;
    case V8RTCStatsType::Enum::kCertificate:
      v8_stats =
          ToV8Stat(script_state, stat.cast_to<webrtc::RTCCertificateStats>());
      break;
  }

  v8_stats->setId(String::FromUTF8(stat.id()));
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (window && window->GetFrame() &&
      window->GetFrame()->Loader().GetDocumentLoader()) {
    DocumentLoadTiming& time_converter =
        window->GetFrame()->Loader().GetDocumentLoader()->GetTiming();
    v8_stats->setTimestamp(time_converter
                               .MonotonicTimeToPseudoWallTime(
                                   ConvertToBaseTimeTicks(stat.timestamp()))
                               .InMillisecondsF());
  }
  v8_stats->setType(*v8_stats_type);
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
                     ScriptObject& object) override {
    return FetchNextItemIdl(script_state, key, object);
  }

  bool FetchNextItemIdl(ScriptState* script_state,
                        String& key,
                        ScriptObject& object) {
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
    object = ScriptObject::From(script_state, v8_stat);
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
RTCStatsReport::CreateIterationSource(ScriptState*) {
  return MakeGarbageCollected<RTCStatsReportIterationSource>(
      report_->CopyHandle());
}

bool RTCStatsReport::GetMapEntry(ScriptState* script_state,
                                 const String& key,
                                 ScriptObject& object) {
  const webrtc::RTCStats* stats = report_->stats_report().Get(key.Utf8());
  if (!stats) {
    return false;
  }

  RTCStats* v8_stats = RTCStatsToIDL(
      script_state, *stats, ExposeHardwareCapabilityStats(script_state));
  if (!v8_stats) {
    return false;
  }
  object = ScriptObject::From(script_state, v8_stats);
  return true;
}

}  // namespace blink
