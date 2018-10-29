// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/frame_stats.h"

#include "remoting/proto/video.pb.h"
#include "remoting/proto/video_stats.pb.h"

namespace remoting {
namespace protocol {

ClientFrameStats::ClientFrameStats() = default;
ClientFrameStats::ClientFrameStats(const ClientFrameStats&) = default;
ClientFrameStats::~ClientFrameStats() = default;
ClientFrameStats& ClientFrameStats::operator=(const ClientFrameStats&) =
    default;

HostFrameStats::HostFrameStats() = default;
HostFrameStats::HostFrameStats(const HostFrameStats&) = default;
HostFrameStats::~HostFrameStats() = default;

// static
HostFrameStats HostFrameStats::GetForVideoPacket(const VideoPacket& packet) {
  HostFrameStats result;
  result.frame_size = packet.data().size();
  if (packet.has_latest_event_timestamp()) {
    result.latest_event_timestamp =
        base::TimeTicks::FromInternalValue(packet.latest_event_timestamp());
  }
  if (packet.has_capture_time_ms()) {
    result.capture_delay =
        base::TimeDelta::FromMilliseconds(packet.capture_time_ms());
  }
  if (packet.has_encode_time_ms()) {
    result.encode_delay =
        base::TimeDelta::FromMilliseconds(packet.encode_time_ms());
  }
  if (packet.has_capture_pending_time_ms()) {
    result.capture_pending_delay =
        base::TimeDelta::FromMilliseconds(packet.capture_pending_time_ms());
  }
  if (packet.has_capture_overhead_time_ms()) {
    result.capture_overhead_delay =
        base::TimeDelta::FromMilliseconds(packet.capture_overhead_time_ms());
  }
  if (packet.has_encode_pending_time_ms()) {
    result.encode_pending_delay =
        base::TimeDelta::FromMilliseconds(packet.encode_pending_time_ms());
  }
  if (packet.has_send_pending_time_ms()) {
    result.send_pending_delay =
        base::TimeDelta::FromMilliseconds(packet.send_pending_time_ms());
  }
  return result;
}

// static
HostFrameStats HostFrameStats::FromFrameStatsMessage(
    const FrameStatsMessage& message) {
  HostFrameStats result;
  result.frame_size = message.frame_size();
  if (message.has_latest_event_timestamp()) {
    result.latest_event_timestamp =
        base::TimeTicks::FromInternalValue(message.latest_event_timestamp());
  }
  if (message.has_capture_time_ms()) {
    result.capture_delay =
        base::TimeDelta::FromMilliseconds(message.capture_time_ms());
  }
  if (message.has_encode_time_ms()) {
    result.encode_delay =
        base::TimeDelta::FromMilliseconds(message.encode_time_ms());
  }
  if (message.has_capture_pending_time_ms()) {
    result.capture_pending_delay =
        base::TimeDelta::FromMilliseconds(message.capture_pending_time_ms());
  }
  if (message.has_capture_overhead_time_ms()) {
    result.capture_overhead_delay =
        base::TimeDelta::FromMilliseconds(message.capture_overhead_time_ms());
  }
  if (message.has_encode_pending_time_ms()) {
    result.encode_pending_delay =
        base::TimeDelta::FromMilliseconds(message.encode_pending_time_ms());
  }
  if (message.has_send_pending_time_ms()) {
    result.send_pending_delay =
        base::TimeDelta::FromMilliseconds(message.send_pending_time_ms());
  }
  if (message.has_rtt_estimate_ms()) {
    result.rtt_estimate =
        base::TimeDelta::FromMilliseconds(message.rtt_estimate_ms());
  }
  if (message.has_bandwidth_estimate_kbps()) {
    result.bandwidth_estimate_kbps = message.bandwidth_estimate_kbps();
  }
  if (message.has_capturer_id()) {
    result.capturer_id = message.capturer_id();
  }
  if (message.has_frame_quality()) {
    result.frame_quality = message.frame_quality();
  }

  return result;
}

void HostFrameStats::ToFrameStatsMessage(FrameStatsMessage* message_out) const {
  message_out->set_frame_size(frame_size);

  if (!latest_event_timestamp.is_null()) {
    message_out->set_latest_event_timestamp(
        latest_event_timestamp.ToInternalValue());
  }
  if (capture_delay != base::TimeDelta::Max()) {
    message_out->set_capture_time_ms(capture_delay.InMilliseconds());
  }
  if (encode_delay != base::TimeDelta::Max()) {
    message_out->set_encode_time_ms(encode_delay.InMilliseconds());
  }
  if (capture_pending_delay != base::TimeDelta::Max()) {
    message_out->set_capture_pending_time_ms(
        capture_pending_delay.InMilliseconds());
  }
  if (capture_overhead_delay != base::TimeDelta::Max()) {
    message_out->set_capture_overhead_time_ms(
        capture_overhead_delay.InMilliseconds());
  }
  if (encode_pending_delay != base::TimeDelta::Max()) {
    message_out->set_encode_pending_time_ms(
        encode_pending_delay.InMilliseconds());
  }
  if (send_pending_delay != base::TimeDelta::Max()) {
    message_out->set_send_pending_time_ms(send_pending_delay.InMilliseconds());
  }
  if (rtt_estimate != base::TimeDelta::Max()) {
    message_out->set_rtt_estimate_ms(rtt_estimate.InMilliseconds());
  }
  if (bandwidth_estimate_kbps >= 0) {
    message_out->set_bandwidth_estimate_kbps(bandwidth_estimate_kbps);
  }
  if (capturer_id != webrtc::DesktopCapturerId::kUnknown) {
    message_out->set_capturer_id(capturer_id);
  }
  if (frame_quality != -1) {
    message_out->set_frame_quality(frame_quality);
  }
}

FrameStats::FrameStats() = default;
FrameStats::FrameStats(const FrameStats&) = default;
FrameStats::~FrameStats() = default;

}  // namespace protocol
}  // namespace remoting
